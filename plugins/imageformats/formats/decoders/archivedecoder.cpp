#include "archivedecoder.h"

#include <QByteArray>
#include <QFile>
#include <QStringList>
#include <QVector>

#ifdef USE_ZLIB
#include <zlib.h>
#endif

namespace {

#ifdef USE_ZLIB

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

struct ZipEntry {
    QString name;
    quint16 method = 0;
    quint32 compressedSize = 0;
    quint32 localOffset = 0;
};

// Reads the central directory. Zip64 archives are not handled: layer documents
// that large are far beyond what a viewer should try to open anyway.
bool readDirectory(QFile &file, QVector<ZipEntry> *entries)
{
    const qint64 fileSize = file.size();
    if(fileSize < 22)
        return false;
    const int tailSize = int(qMin<qint64>(fileSize, 65557));
    if(!file.seek(fileSize - tailSize))
        return false;
    const QByteArray tail = file.read(tailSize);
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
    if(!file.seek(directoryOffset))
        return false;
    const QByteArray directory = file.read(directorySize);
    if(directory.size() != int(directorySize))
        return false;

    int pos = 0;
    for(int i = 0; i < count; i++) {
        if(pos + 46 > directory.size() || readU32(directory, pos) != kCentralHeader)
            break;
        ZipEntry entry;
        entry.method = readU16(directory, pos + 10);
        entry.compressedSize = readU32(directory, pos + 20);
        const quint16 nameLength = readU16(directory, pos + 28);
        const quint16 extraLength = readU16(directory, pos + 30);
        const quint16 commentLength = readU16(directory, pos + 32);
        entry.localOffset = readU32(directory, pos + 42);
        if(pos + 46 + nameLength > directory.size())
            break;
        entry.name = QString::fromUtf8(directory.mid(pos + 46, nameLength));
        entries->append(entry);
        pos += 46 + nameLength + extraLength + commentLength;
    }
    return !entries->isEmpty();
}

bool inflateEntry(const QByteArray &raw, QByteArray *out)
{
    z_stream stream{};
    if(inflateInit2(&stream, -MAX_WBITS) != Z_OK)
        return false;
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(raw.constData()));
    stream.avail_in = uInt(raw.size());

    QByteArray buffer(64 * 1024, Qt::Uninitialized);
    out->clear();
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
        // no more input and the entry is not finished: the file is truncated
        if(status == Z_OK && stream.avail_in == 0 && stream.avail_out != 0)
            break;
    } while(status != Z_STREAM_END);
    inflateEnd(&stream);
    return !out->isEmpty();
}

bool extractEntry(QFile &file, const ZipEntry &entry, QByteArray *out, int maxCompressed = -1)
{
    if(!file.seek(entry.localOffset))
        return false;
    const QByteArray header = file.read(30);
    if(header.size() < 30 || readU32(header, 0) != kLocalHeader)
        return false;
    const quint16 nameLength = readU16(header, 26);
    const quint16 extraLength = readU16(header, 28);
    if(!file.seek(entry.localOffset + 30 + nameLength + extraLength))
        return false;
    QByteArray raw = file.read(entry.compressedSize);
    if(maxCompressed >= 0 && raw.size() > maxCompressed)
        raw.truncate(maxCompressed);
    else if(raw.size() != int(entry.compressedSize))
        return false;
    if(entry.method == 0) {
        *out = raw;
        return true;
    }
    if(entry.method != 8)
        return false;
    return inflateEntry(raw, out);
}

quint32 readBigEndianU32(const QByteArray &data, int offset)
{
    return (quint32(uchar(data.at(offset))) << 24)
           | (quint32(uchar(data.at(offset + 1))) << 16)
           | (quint32(uchar(data.at(offset + 2))) << 8)
           | quint32(uchar(data.at(offset + 3)));
}

// PNG: 8 byte signature, 4 byte chunk length, "IHDR", then width and height as
// big endian 32 bit values.
QSize pngSize(const QByteArray &data)
{
    if(data.size() < 24 || !data.startsWith("\x89PNG"))
        return QSize();
    if(!data.mid(12, 4).compare("IHDR") == 0)
        return QSize();
    const qint64 width = readBigEndianU32(data, 16);
    const qint64 height = readBigEndianU32(data, 20);
    if(width <= 0 || height <= 0 || width > 100000 || height > 100000)
        return QSize();
    return QSize(int(width), int(height));
}

const QStringList &candidateNames()
{
    static const QStringList names = {
        QStringLiteral("mergedimage.png"),
        QStringLiteral("preview.png"),
        QStringLiteral("thumbnails/thumbnail.png")
    };
    return names;
}

#endif // USE_ZLIB

} // namespace

namespace ArchiveImageDecoder {

bool isAvailable()
{
#ifdef USE_ZLIB
    return true;
#else
    return false;
#endif
}

bool isArchiveSuffix(const QString &suffix)
{
    return suffix.compare(QLatin1String("kra"), Qt::CaseInsensitive) == 0
           || suffix.compare(QLatin1String("ora"), Qt::CaseInsensitive) == 0;
}

QSize size(const QString &path)
{
#ifdef USE_ZLIB
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
        return QSize();
    QVector<ZipEntry> entries;
    if(!readDirectory(file, &entries))
        return QSize();
    for(const QString &candidate : candidateNames()) {
        for(const ZipEntry &entry : entries) {
            if(entry.name.compare(candidate, Qt::CaseInsensitive) != 0)
                continue;
            // Only the start of the entry is inflated: the PNG header is in the
            // first bytes and the image itself is never decoded here.
            const int maxCompressed = entry.method == 8 ? 256 * 1024 : 64;
            QByteArray data;
            if(!extractEntry(file, entry, &data, maxCompressed))
                continue;
            const QSize imageSize = pngSize(data);
            if(imageSize.isValid())
                return imageSize;
        }
    }
    return QSize();
#else
    Q_UNUSED(path)
    return QSize();
#endif
}

QImage read(const QString &path, const QSize &targetSize)
{
#ifdef USE_ZLIB
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
        return QImage();
    QVector<ZipEntry> entries;
    if(!readDirectory(file, &entries))
        return QImage();

    // Krita writes the flattened image, OpenRaster too; the thumbnail is the
    // fallback for files saved without one. The largest candidate wins.
    QImage image;
    for(const QString &candidate : candidateNames()) {
        for(const ZipEntry &entry : entries) {
            if(entry.name.compare(candidate, Qt::CaseInsensitive) != 0)
                continue;
            QByteArray data;
            if(!extractEntry(file, entry, &data))
                continue;
            const QImage decoded = QImage::fromData(data);
            if(decoded.isNull())
                continue;
            if(image.isNull()
               || qint64(decoded.width()) * decoded.height() > qint64(image.width()) * image.height()) {
                image = decoded;
            }
        }
    }
    if(image.isNull())
        return image;
    if(targetSize.isValid()
       && (targetSize.width() < image.width() || targetSize.height() < image.height())) {
        return image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return image;
#else
    Q_UNUSED(path)
    Q_UNUSED(targetSize)
    return QImage();
#endif
}

} // namespace ArchiveImageDecoder
