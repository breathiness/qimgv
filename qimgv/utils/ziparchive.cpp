#include "ziparchive.h"

#include <QCollator>
#include <QFileInfo>

#include <algorithm>

#ifdef USE_ZLIB
#include <zlib.h>
#endif

namespace {

constexpr quint32 kEndOfDirectory = 0x06054b50;
constexpr quint32 kCentralHeader = 0x02014b50;
constexpr quint32 kLocalHeader = 0x04034b50;

quint16 readU16(const QByteArray &data, int offset)
{
    return quint16(uchar(data.at(offset))) | (quint16(uchar(data.at(offset + 1))) << 8);
}

quint32 readU32(const QByteArray &data, int offset)
{
    return quint32(uchar(data.at(offset)))
           | (quint32(uchar(data.at(offset + 1))) << 8)
           | (quint32(uchar(data.at(offset + 2))) << 16)
           | (quint32(uchar(data.at(offset + 3))) << 24);
}

#ifdef USE_ZLIB
bool inflateRaw(const QByteArray &raw, QByteArray *out)
{
    z_stream stream{};
    if(inflateInit2(&stream, -MAX_WBITS) != Z_OK)
        return false;
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(raw.constData()));
    stream.avail_in = uInt(raw.size());

    QByteArray buffer(64 * 1024, Qt::Uninitialized);
    int status = Z_OK;
    do {
        stream.next_out = reinterpret_cast<Bytef *>(buffer.data());
        stream.avail_out = uInt(buffer.size());
        status = inflate(&stream, Z_NO_FLUSH);
        if(status != Z_OK && status != Z_STREAM_END) {
            inflateEnd(&stream);
            return false;
        }
        out->append(buffer.constData(), int(buffer.size() - stream.avail_out));
        if(status == Z_OK && stream.avail_in == 0 && stream.avail_out != 0)
            break;
    } while(status != Z_STREAM_END);
    inflateEnd(&stream);
    return !out->isEmpty();
}
#endif

} // namespace

bool ZipArchive::open(const QString &path)
{
    mOpen = false;
    mEntries.clear();
    mFile.setFileName(path);
    if(!mFile.open(QIODevice::ReadOnly))
        return false;

    const qint64 fileSize = mFile.size();
    if(fileSize < 22)
        return false;
    const int tailSize = int(qMin<qint64>(fileSize, 65557));
    if(!mFile.seek(fileSize - tailSize))
        return false;
    const QByteArray tail = mFile.read(tailSize);
    int end = -1;
    for(int i = tail.size() - 22; i >= 0; i--) {
        if(readU32(tail, i) == kEndOfDirectory) {
            end = i;
            break;
        }
    }
    if(end < 0)
        return false;

    const quint16 count = readU16(tail, end + 10);
    const quint32 directorySize = readU32(tail, end + 12);
    const quint32 directoryOffset = readU32(tail, end + 16);
    if(count == 0 || directorySize == 0)
        return false;
    if(!mFile.seek(directoryOffset))
        return false;
    const QByteArray directory = mFile.read(directorySize);
    if(directory.size() != int(directorySize))
        return false;

    int pos = 0;
    for(int i = 0; i < count; i++) {
        if(pos + 46 > directory.size() || readU32(directory, pos) != kCentralHeader)
            break;
        Entry entry;
        entry.method = readU16(directory, pos + 10);
        entry.compressedSize = readU32(directory, pos + 20);
        const quint16 nameLength = readU16(directory, pos + 28);
        const quint16 extraLength = readU16(directory, pos + 30);
        const quint16 commentLength = readU16(directory, pos + 32);
        entry.localOffset = readU32(directory, pos + 42);
        if(pos + 46 + nameLength > directory.size())
            break;
        entry.name = QString::fromUtf8(directory.mid(pos + 46, nameLength));
        mEntries.append(entry);
        pos += 46 + nameLength + extraLength + commentLength;
    }
    mOpen = !mEntries.isEmpty();
    return mOpen;
}

QStringList ZipArchive::names() const
{
    QStringList result;
    result.reserve(mEntries.size());
    for(const Entry &entry : mEntries)
        result.append(entry.name);

    // "page2" before "page10": a plain string sort puts 10 first, which makes a
    // comic unreadable
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::stable_sort(result.begin(), result.end(), [&collator](const QString &a, const QString &b) {
        return collator.compare(a, b) < 0;
    });
    return result;
}

QStringList comicImageEntries(const QStringList &names)
{
    static const QStringList imageSuffixes = {
        QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
        QStringLiteral("webp"), QStringLiteral("gif"), QStringLiteral("bmp"),
        QStringLiteral("tif"), QStringLiteral("tiff"), QStringLiteral("avif"),
        QStringLiteral("heic"), QStringLiteral("heif"), QStringLiteral("jxl"),
        QStringLiteral("qoi"), QStringLiteral("pfm")
    };
    QStringList pages;
    for(const QString &name : names) {
        if(imageSuffixes.contains(QFileInfo(name).suffix().toLower()))
            pages.append(name);
    }
    return pages;
}

QByteArray ZipArchive::read(const QString &name) const
{
#ifdef USE_ZLIB
    if(!mOpen)
        return QByteArray();
    const Entry *entry = nullptr;
    for(const Entry &candidate : mEntries) {
        if(candidate.name == name) {
            entry = &candidate;
            break;
        }
    }
    if(!entry)
        return QByteArray();
    if(!mFile.seek(entry->localOffset))
        return QByteArray();
    const QByteArray header = mFile.read(30);
    if(header.size() < 30 || readU32(header, 0) != kLocalHeader)
        return QByteArray();
    const quint16 nameLength = readU16(header, 26);
    const quint16 extraLength = readU16(header, 28);
    if(!mFile.seek(entry->localOffset + 30 + nameLength + extraLength))
        return QByteArray();
    const QByteArray raw = mFile.read(entry->compressedSize);
    if(raw.size() != int(entry->compressedSize))
        return QByteArray();
    if(entry->method == 0)
        return raw;
    if(entry->method != 8)
        return QByteArray();
    QByteArray out;
    if(!inflateRaw(raw, &out))
        return QByteArray();
    return out;
#else
    Q_UNUSED(name)
    return QByteArray();
#endif
}
