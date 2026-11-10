#include "qoidecoder.h"

#include <QFile>

namespace {

quint32 readBigEndian(const uchar *data)
{
    return (quint32(data[0]) << 24) | (quint32(data[1]) << 16) | (quint32(data[2]) << 8)
           | quint32(data[3]);
}

// The 64 entry colour cache the encoder and decoder build identically.
struct ColorHash {
    quint32 values[64] = {};
    static int index(quint8 r, quint8 g, quint8 b, quint8 a)
    {
        return int((r * 3 + g * 5 + b * 7 + a * 11) % 64);
    }
};

} // namespace

namespace QoiImage {

bool isQoiSuffix(const QString &suffix)
{
    return suffix.compare(QLatin1String("qoi"), Qt::CaseInsensitive) == 0;
}

QSize size(const QString &path)
{
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
        return QSize();
    const QByteArray header = file.read(14);
    if(header.size() < 14 || !header.startsWith("qoif"))
        return QSize();
    const uchar *bytes = reinterpret_cast<const uchar *>(header.constData());
    const quint32 width = readBigEndian(bytes + 4);
    const quint32 height = readBigEndian(bytes + 8);
    if(width == 0 || height == 0 || width > 100000 || height > 100000)
        return QSize();
    return QSize(int(width), int(height));
}

QImage read(const QString &path, const QSize &targetSize)
{
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
        return QImage();
    const QByteArray data = file.readAll();
    if(data.size() < 22 || !data.startsWith("qoif"))
        return QImage();

    const uchar *bytes = reinterpret_cast<const uchar *>(data.constData());
    const quint32 width = readBigEndian(bytes + 4);
    const quint32 height = readBigEndian(bytes + 8);
    const int channels = bytes[12];
    if(width == 0 || height == 0 || width > 100000 || height > 100000)
        return QImage();
    if(channels != 3 && channels != 4)
        return QImage();

    QImage image(int(width), int(height), QImage::Format_RGBA8888);
    if(image.isNull())
        return QImage();

    ColorHash cache;
    uchar r = 0, g = 0, b = 0, a = 255;
    int run = 0;
    qsizetype pos = 14;   // after the header
    const qsizetype pixelCount = qsizetype(width) * qsizetype(height);

    for(qsizetype pixel = 0; pixel < pixelCount; pixel++) {
        if(run > 0) {
            run--;
        } else if(pos < data.size()) {
            const uchar op = bytes[pos++];
            if(op == 0xfe) {              // RGB
                if(pos + 3 > data.size())
                    return QImage();
                r = bytes[pos];
                g = bytes[pos + 1];
                b = bytes[pos + 2];
                pos += 3;
            } else if(op == 0xff) {       // RGBA
                if(pos + 4 > data.size())
                    return QImage();
                r = bytes[pos];
                g = bytes[pos + 1];
                b = bytes[pos + 2];
                a = bytes[pos + 3];
                pos += 4;
            } else if((op & 0xc0) == 0xc0) {   // RUN (0xc0..0xfd)
                // Only the two top bits set means RUN. Testing just "op & 0xc0"
                // also swallowed DIFF (0x40..0x7f) and LUMA (0x80..0xbf) into the
                // run counter, so every picture that used them decoded as a
                // repeat of its first pixels.
                run = (op & 0x3f);
            } else if((op & 0xc0) == 0x00) {   // INDEX
                const int index = op & 0x3f;
                // the cache stores r,g,b,a packed; unpack it
                const quint32 value = cache.values[index];
                r = uchar(value >> 24);
                g = uchar(value >> 16);
                b = uchar(value >> 8);
                a = uchar(value);
            } else if((op & 0xc0) == 0x40) {   // DIFF
                r = uchar(int(r) + ((op >> 4) & 3) - 2);
                g = uchar(int(g) + ((op >> 2) & 3) - 2);
                b = uchar(int(b) + (op & 3) - 2);
            } else if((op & 0xc0) == 0x80) {   // LUMA
                if(pos >= data.size())
                    return QImage();
                const uchar second = bytes[pos++];
                const int deltaG = (op & 0x3f) - 32;
                r = uchar(int(r) + deltaG + ((second >> 4) & 0x0f) - 8);
                g = uchar(int(g) + deltaG);
                b = uchar(int(b) + deltaG + (second & 0x0f) - 8);
            }
        }
        cache.values[ColorHash::index(r, g, b, a)] =
            (quint32(r) << 24) | (quint32(g) << 16) | (quint32(b) << 8) | quint32(a);

        uchar *dest = image.scanLine(int(pixel / width)) + (pixel % width) * 4;
        dest[0] = r;
        dest[1] = g;
        dest[2] = b;
        dest[3] = a;
    }

    if(targetSize.isValid()
       && (targetSize.width() < image.width() || targetSize.height() < image.height())) {
        return image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return image;
}

} // namespace QoiImage
