#include "psddecoder.h"

#include <QFile>
#include <QByteArray>
#include <QtGlobal>
#include <cstring>
#include <vector>

#ifdef USE_ZLIB
#include <zlib.h>
#endif

namespace {

// Colour modes the decoder understands.
enum ColourMode {
    ModeBitmap = 0,
    ModeGrayscale = 1,
    ModeRGB = 3,
    ModeCMYK = 4
};

struct Header {
    int version = 0;   // 1 = PSD, 2 = PSB
    int channels = 0;
    int height = 0;
    int width = 0;
    int depth = 0;
    int mode = 0;
};

// Small reader with bounds checking; every failure path just returns false and
// the caller turns that into an empty image.
class Reader {
public:
    explicit Reader(QFile &file) : mFile(file) {}

    bool readHeader(Header &header)
    {
        QByteArray signature(4, 0);
        if(mFile.read(signature.data(), 4) != 4 || signature != QByteArray("8BPS"))
            return false;
        quint16 version = 0;
        if(!readU16(version) || (version != 1 && version != 2))
            return false;
        if(!skip(6))
            return false;
        quint16 channels = 0;
        quint32 height = 0, width = 0;
        quint16 depth = 0, mode = 0;
        if(!readU16(channels) || !readU32(height) || !readU32(width)
           || !readU16(depth) || !readU16(mode))
            return false;
        header.version = version;
        header.channels = channels;
        header.height = int(height);
        header.width = int(width);
        header.depth = depth;
        header.mode = mode;
        return header.width > 0 && header.height > 0 && header.channels > 0;
    }

    bool readU8(quint8 &value)
    {
        char c = 0;
        if(mFile.read(&c, 1) != 1)
            return false;
        value = quint8(c);
        return true;
    }

    bool readU16(quint16 &value)
    {
        unsigned char buffer[2];
        if(mFile.read(reinterpret_cast<char *>(buffer), 2) != 2)
            return false;
        value = quint16((buffer[0] << 8) | buffer[1]);
        return true;
    }

    bool readU32(quint32 &value)
    {
        unsigned char buffer[4];
        if(mFile.read(reinterpret_cast<char *>(buffer), 4) != 4)
            return false;
        value = (quint32(buffer[0]) << 24) | (quint32(buffer[1]) << 16)
                | (quint32(buffer[2]) << 8) | quint32(buffer[3]);
        return true;
    }

    bool skip(qint64 bytes)
    {
        if(bytes < 0)
            return false;
        const qint64 target = mFile.pos() + bytes;
        if(target > mFile.size())
            return false;
        return mFile.seek(target);
    }

    QByteArray readBytes(qint64 bytes)
    {
        if(bytes < 0 || mFile.pos() + bytes > mFile.size())
            return QByteArray();
        return mFile.read(bytes);
    }

    QByteArray readRest()
    {
        return mFile.read(mFile.size() - mFile.pos());
    }

    qint64 pos() const { return mFile.pos(); }
    bool seek(qint64 position) { return mFile.seek(position); }
    qint64 size() const { return mFile.size(); }

private:
    QFile &mFile;
};

// PackBits row decoder: a control byte below 128 is a literal run of n+1 bytes,
// above 128 a repeat of the next byte 257-n times.
bool unpackBitsRow(const uchar *src, int srcLength, uchar *dst, int dstLength)
{
    int in = 0;
    int out = 0;
    while(in < srcLength && out < dstLength) {
        const int control = src[in++];
        if(control < 128) {
            const int count = control + 1;
            if(in + count > srcLength)
                return false;
            const int copy = qMin(count, dstLength - out);
            std::memcpy(dst + out, src + in, size_t(copy));
            out += copy;
            in += count;
        } else if(control > 128) {
            if(in >= srcLength)
                return false;
            const int count = 257 - control;
            const int copy = qMin(count, dstLength - out);
            std::memset(dst + out, src[in], size_t(copy));
            out += copy;
            in++;
        }
        // 128 is a no-op
    }
    return out == dstLength;
}

#ifdef USE_ZLIB
bool inflateBuffer(const QByteArray &input, QByteArray &output, qint64 expected)
{
    if(input.isEmpty() || expected <= 0)
        return false;
    output.resize(int(expected));
    uLongf destLen = uLongf(expected);
    const int status = uncompress(reinterpret_cast<Bytef *>(output.data()), &destLen,
                                  reinterpret_cast<const Bytef *>(input.constData()),
                                  uLong(input.size()));
    if(status != Z_OK)
        return false;
    output.resize(int(destLen));
    return destLen == uLong(expected);
}

// ZIP with prediction stores the first sample of a row raw and the rest as
// differences; 8 bit diff one byte back, 16 bit two bytes back.
void undoPrediction(QByteArray &plane, int width, int height, int bytesPerSample)
{
    uchar *data = reinterpret_cast<uchar *>(plane.data());
    const int rowBytes = width * bytesPerSample;
    for(int y = 0; y < height; y++) {
        uchar *row = data + qsizetype(y) * rowBytes;
        for(int x = bytesPerSample; x < rowBytes; x++)
            row[x] = uchar(row[x] + row[x - bytesPerSample]);
    }
}
#endif

// Photoshop's thumbnail resource 1036: a 28 byte header, then either a jpeg
// (format 1) or planar RGB data (format 0). The header is
//   format, width, height, widthBytes, totalSize, compressedSize, bpp, planes
QImage thumbnailFromResource1036(const QByteArray &data)
{
    if(data.size() < 28)
        return QImage();
    const auto u32 = [&data](int offset) -> quint32 {
        return (quint32(quint8(data[offset])) << 24)
               | (quint32(quint8(data[offset + 1])) << 16)
               | (quint32(quint8(data[offset + 2])) << 8)
               | quint32(quint8(data[offset + 3]));
    };
    const quint32 format = u32(0);
    const int width = int(u32(4));
    const int height = int(u32(8));
    const int widthBytes = int(u32(12));
    const int totalSize = int(u32(16));
    const int compressedSize = int(u32(20));
    const quint16 bitsPerPixel = quint16((quint8(data[24]) << 8) | quint8(data[25]));
    if(width <= 0 || height <= 0 || bitsPerPixel != 24)
        return QImage();

    if(format == 1) { // jpeg
        if(compressedSize <= 0 || 28 + compressedSize > data.size())
            return QImage();
        QImage thumbnail;
        if(thumbnail.loadFromData(data.mid(28, compressedSize)))
            return thumbnail;
        return QImage();
    }

    const quint16 planes = quint16((quint8(data[26]) << 8) | quint8(data[27]));
    if(planes != 3 || widthBytes < width || 28 + totalSize > data.size())
        return QImage();

    QImage image(width, height, QImage::Format_RGB32);
    if(image.isNull())
        return QImage();
    const uchar *src = reinterpret_cast<const uchar *>(data.constData()) + 28;
    const qint64 planeSize = qint64(widthBytes) * height;
    for(int y = 0; y < height; y++) {
        uchar *dst = image.scanLine(y);
        const uchar *r = src + qint64(y) * widthBytes;
        const uchar *g = r + planeSize;
        const uchar *b = g + planeSize;
        for(int x = 0; x < width; x++) {
            dst[4 * x + 0] = b[x];
            dst[4 * x + 1] = g[x];
            dst[4 * x + 2] = r[x];
            dst[4 * x + 3] = 0;
        }
    }
    return image;
}

} // namespace

namespace PsdDecoder {

bool isAvailable()
{
    return true;
}

bool isPsdSuffix(const QString &suffix)
{
    const QString s = suffix.toLower();
    return s == QLatin1String("psd") || s == QLatin1String("psb");
}

QSize size(const QString &path)
{
    QFile file(path);
    if(!file.open(QFile::ReadOnly))
        return QSize();
    Reader reader(file);
    Header header;
    if(!reader.readHeader(header))
        return QSize();
    return QSize(header.width, header.height);
}

QImage read(const QString &path, const QSize &targetSize)
{
    QFile file(path);
    if(!file.open(QFile::ReadOnly))
        return QImage();
    Reader reader(file);
    Header header;
    if(!reader.readHeader(header))
        return QImage();
    if(header.mode != ModeRGB && header.mode != ModeGrayscale && header.mode != ModeCMYK)
        return QImage();
    if(header.depth != 8 && header.depth != 16)
        return QImage();

    // colour mode data
    quint32 colourModeLength = 0;
    if(!reader.readU32(colourModeLength) || !reader.skip(colourModeLength))
        return QImage();

    // image resources (only their length matters here)
    quint32 resourcesLength = 0;
    if(!reader.readU32(resourcesLength) || !reader.skip(resourcesLength))
        return QImage();

    // layer and mask information
    quint32 layerLength = 0;
    if(!reader.readU32(layerLength) || !reader.skip(layerLength))
        return QImage();

    quint16 compression = 0;
    if(!reader.readU16(compression))
        return QImage();

    const int bytesPerSample = header.depth / 8;
    const int pixelCount = header.width * header.height;
    const int planeSize = pixelCount * bytesPerSample;
    const int channels = header.channels;

    // sanity: a channel plane must fit in memory
    if(qint64(planeSize) * channels > 4LL * 1024 * 1024 * 1024)
        return QImage();

    std::vector<QByteArray> planes;
    planes.resize(size_t(channels));

    if(compression == 0) {
        for(int c = 0; c < channels; c++) {
            planes[size_t(c)] = reader.readBytes(planeSize);
            if(planes[size_t(c)].size() != planeSize)
                return QImage();
        }
    } else if(compression == 1) {
        // all row counts first (4 bytes each in PSB, 2 in PSD), then all rows
        const int countBytes = header.version == 2 ? 4 : 2;
        const qint64 rowCountsSize = qint64(header.height) * channels * countBytes;
        if(reader.pos() + rowCountsSize > reader.size())
            return QImage();
        std::vector<int> rowLengths(size_t(header.height) * channels, 0);
        for(int c = 0; c < channels; c++) {
            for(int y = 0; y < header.height; y++) {
                quint32 value = 0;
                if(countBytes == 4) {
                    if(!reader.readU32(value))
                        return QImage();
                } else {
                    quint16 small = 0;
                    if(!reader.readU16(small))
                        return QImage();
                    value = small;
                }
                rowLengths[size_t(c) * header.height + y] = int(value);
            }
        }
        for(int c = 0; c < channels; c++) {
            QByteArray plane(planeSize, Qt::Uninitialized);
            uchar *dst = reinterpret_cast<uchar *>(plane.data());
            const qint64 rowsStart = reader.pos();
            qint64 consumed = 0;
            for(int y = 0; y < header.height; y++) {
                const int rowLength = rowLengths[size_t(c) * header.height + y];
                consumed += rowLength;
                if(rowLength <= 0 || rowsStart + consumed > reader.size())
                    return QImage();
            }
            // read the whole channel's rows in one go, then unpack per row
            const QByteArray rows = reader.readBytes(consumed);
            if(rows.size() != consumed)
                return QImage();
            const uchar *src = reinterpret_cast<const uchar *>(rows.constData());
            qint64 offset = 0;
            for(int y = 0; y < header.height; y++) {
                const int rowLength = rowLengths[size_t(c) * header.height + y];
                const int rowBytes = header.width * bytesPerSample;
                if(!unpackBitsRow(src + offset, rowLength, dst + qsizetype(y) * rowBytes, rowBytes))
                    return QImage();
                offset += rowLength;
            }
            planes[size_t(c)] = plane;
        }
    } else if(compression == 2 || compression == 3) {
#ifdef USE_ZLIB
        const QByteArray compressed = reader.readRest();
        QByteArray inflated;
        if(!inflateBuffer(compressed, inflated, qint64(planeSize) * channels))
            return QImage();
        for(int c = 0; c < channels; c++) {
            QByteArray plane = inflated.mid(c * planeSize, planeSize);
            if(compression == 3)
                undoPrediction(plane, header.width, header.height, bytesPerSample);
            planes[size_t(c)] = plane;
        }
#else
        return QImage();
#endif
    } else {
        return QImage();
    }

    // compose the image
    const bool hasAlpha = (header.mode == ModeRGB && channels >= 4)
                          || (header.mode == ModeGrayscale && channels >= 2)
                          || (header.mode == ModeCMYK && channels >= 5);
    QImage image(header.width, header.height,
                 hasAlpha ? QImage::Format_ARGB32 : QImage::Format_RGB32);
    if(image.isNull())
        return QImage();

    // 16 bit data is scaled down to 8 bit by keeping the high byte
    const uchar *planePtr[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
    for(int c = 0; c < channels && c < 5; c++)
        planePtr[c] = reinterpret_cast<const uchar *>(planes[size_t(c)].constData());
    const auto sample = [bytesPerSample](const uchar *plane, int index) -> uchar {
        return plane[qint64(index) * bytesPerSample];
    };

    for(int y = 0; y < header.height; y++) {
        uchar *dst = image.scanLine(y);
        for(int x = 0; x < header.width; x++) {
            const int index = y * header.width + x;
            uchar r = 0, g = 0, b = 0, a = 255;
            if(header.mode == ModeRGB) {
                r = sample(planePtr[0], index);
                g = sample(planePtr[1], index);
                b = sample(planePtr[2], index);
                if(hasAlpha)
                    a = sample(planePtr[3], index);
            } else if(header.mode == ModeGrayscale) {
                r = g = b = sample(planePtr[0], index);
                if(hasAlpha)
                    a = sample(planePtr[1], index);
            } else { // CMYK: the usual naive conversion, no colour profile involved
                const int c = sample(planePtr[0], index);
                const int m = sample(planePtr[1], index);
                const int ye = sample(planePtr[2], index);
                const int k = sample(planePtr[3], index);
                r = uchar((255 - c) * (255 - k) / 255);
                g = uchar((255 - m) * (255 - k) / 255);
                b = uchar((255 - ye) * (255 - k) / 255);
                if(hasAlpha)
                    a = sample(planePtr[4], index);
            }
            dst[4 * x + 0] = b;
            dst[4 * x + 1] = g;
            dst[4 * x + 2] = r;
            dst[4 * x + 3] = a;
        }
    }

    if(targetSize.isValid() && (targetSize.width() < image.width()
                                || targetSize.height() < image.height())) {
        image = image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return image;
}

QImage readThumbnail(const QString &path, int minSize)
{
    QFile file(path);
    if(!file.open(QFile::ReadOnly))
        return QImage();
    Reader reader(file);
    Header header;
    if(!reader.readHeader(header))
        return QImage();

    quint32 colourModeLength = 0;
    if(!reader.readU32(colourModeLength) || !reader.skip(colourModeLength))
        return QImage();

    quint32 resourcesLength = 0;
    if(!reader.readU32(resourcesLength) || resourcesLength == 0)
        return QImage();
    const QByteArray resources = reader.readBytes(resourcesLength);
    if(resources.size() != qint64(resourcesLength))
        return QImage();

    qint64 offset = 0;
    while(offset + 12 <= resources.size()) {
        const QByteArray signature = resources.mid(int(offset), 4);
        if(signature != QByteArray("8BIM") && signature != QByteArray("MeSa"))
            break;
        const quint32 resourceId = (quint32(quint8(resources[int(offset + 4)])) << 8)
                                   | quint32(quint8(resources[int(offset + 5)]));
        const int nameLength = quint8(resources[int(offset + 6)]);
        qint64 headerLength = 6 + 1 + nameLength;
        if(headerLength % 2)
            headerLength++;
        const qint64 sizeOffset = offset + headerLength;
        if(sizeOffset + 4 > resources.size())
            break;
        const quint32 dataSize = (quint32(quint8(resources[int(sizeOffset)])) << 24)
                                 | (quint32(quint8(resources[int(sizeOffset + 1)])) << 16)
                                 | (quint32(quint8(resources[int(sizeOffset + 2)])) << 8)
                                 | quint32(quint8(resources[int(sizeOffset + 3)]));
        const qint64 dataOffset = sizeOffset + 4;
        if(dataOffset + dataSize > resources.size())
            break;
        const QByteArray data = resources.mid(int(dataOffset), int(dataSize));
        if(resourceId == 1036) {
            QImage thumbnail = thumbnailFromResource1036(data);
            if(!thumbnail.isNull()) {
                if(minSize > 0 && thumbnail.width() < minSize)
                    return QImage();
                return thumbnail;
            }
        } else if(resourceId == 1033) {
            // the older resource is a plain jpeg stream
            QImage thumbnail;
            if(thumbnail.loadFromData(data) && !thumbnail.isNull()) {
                if(minSize > 0 && thumbnail.width() < minSize)
                    return QImage();
                return thumbnail;
            }
        }
        offset = dataOffset + dataSize + (dataSize % 2);
    }
    return QImage();
}

} // namespace PsdDecoder
