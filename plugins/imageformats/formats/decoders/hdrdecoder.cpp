#include "hdrdecoder.h"

#include <QByteArray>
#include <QFile>
#include <QList>

#include <cmath>
#include <vector>

namespace {

// Parsed header of a Radiance file.
struct HdrHeader {
    bool ok = false;
    QSize size;
    bool flipX = false;      // "-X" resolution: stored right to left
    bool flipY = false;      // "+Y" resolution: stored bottom to top
    int dataOffset = 0;      // first byte of the scanline data
};

// The header is a "#?RADIANCE" magic, keyword lines and a blank line; the
// resolution line ("-Y 1024 +X 2048") follows the blank line.
HdrHeader parseHeader(const QByteArray &data)
{
    HdrHeader header;
    if(!data.startsWith("#?"))
        return header;

    int pos = 0;
    bool resolutionFound = false;
    while(pos < data.size()) {
        const int eol = data.indexOf('\n', pos);
        if(eol < 0)
            return header;
        QByteArray line = data.mid(pos, eol - pos);
        if(line.endsWith('\r'))
            line.chop(1);
        pos = eol + 1;
        if(line.isEmpty()) {
            resolutionFound = true;
            break;
        }
    }
    if(!resolutionFound)
        return header;

    const int eol = data.indexOf('\n', pos);
    if(eol < 0)
        return header;
    QByteArray resolution = data.mid(pos, eol - pos).simplified();
    if(resolution.endsWith('\r'))
        resolution.chop(1);
    pos = eol + 1;

    // the line is "<axisY> <height> <axisX> <width>"
    const QList<QByteArray> parts = resolution.split(' ');
    if(parts.size() != 4)
        return header;
    if(!(parts.at(0).endsWith('Y') || parts.at(0).endsWith('y'))
       || !(parts.at(2).endsWith('X') || parts.at(2).endsWith('x')))
        return header;
    const int height = parts.at(1).toInt();
    const int width = parts.at(3).toInt();
    if(width <= 0 || height <= 0)
        return header;

    header.size = QSize(width, height);
    header.flipY = parts.at(0).startsWith('+');
    header.flipX = parts.at(2).startsWith('-');
    header.dataOffset = pos;
    header.ok = true;
    return header;
}

inline uchar toneMap(float value)
{
    if(!(value > 0.0f))
        return 0;
    const float clamped = value > 1.0f ? 1.0f : value;
    return uchar(qBound(0, int(std::pow(clamped, 1.0f / 2.2f) * 255.0f + 0.5f), 255));
}

// RGBE to linear RGB. The exponent is biased by 128 and scaled by 8 bits.
inline void rgbeToRgb(const uchar *rgbe, float &r, float &g, float &b)
{
    if(rgbe[3] == 0) {
        r = g = b = 0.0f;
        return;
    }
    const float scale = std::ldexp(1.0f, int(rgbe[3]) - (128 + 8));
    r = rgbe[0] * scale;
    g = rgbe[1] * scale;
    b = rgbe[2] * scale;
}

} // namespace

namespace HdrDecoder {

bool isHdrSuffix(const QString &suffix)
{
    return suffix.compare(QLatin1String("hdr"), Qt::CaseInsensitive) == 0
           || suffix.compare(QLatin1String("rgbe"), Qt::CaseInsensitive) == 0;
}

QSize size(const QString &path)
{
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
        return QSize();
    // header only: the resolution line is near the start
    const HdrHeader header = parseHeader(file.read(4096));
    return header.ok ? header.size : QSize();
}

QImage read(const QString &path, const QSize &targetSize)
{
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
        return QImage();
    const QByteArray data = file.readAll();
    const HdrHeader header = parseHeader(data);
    if(!header.ok)
        return QImage();

    const int width = header.size.width();
    const int height = header.size.height();
    QImage image(width, height, QImage::Format_RGB888);
    if(image.isNull())
        return QImage();

    // RLE is only used for "new style" files; narrow or very wide images are
    // stored as plain RGBE
    const bool oldStyle = width < 8 || width > 0x7fff;

    std::vector<uchar> scanline(size_t(width) * 4);
    int pos = header.dataOffset;
    for(int y = 0; y < height; y++) {
        // A "new style" scanline starts with 0x02 0x02, the width in two bytes,
        // and then the four RLE encoded components. When that header is missing
        // the row is stored as plain RGBE, which readers have to accept.
        bool rle = false;
        if(!oldStyle && pos + 4 <= data.size()) {
            const uchar first = uchar(data.at(pos));
            const uchar second = uchar(data.at(pos + 1));
            const int scanWidth = (int(uchar(data.at(pos + 2))) << 8) | uchar(data.at(pos + 3));
            if(first == 2 && second == 2 && scanWidth == width) {
                rle = true;
                pos += 4;
            }
        }

        if(!rle) {
            if(pos + width * 4 > data.size())
                return QImage();
            std::copy(data.constData() + pos, data.constData() + pos + width * 4, scanline.begin());
            pos += width * 4;
        } else {
            for(int component = 0; component < 4; component++) {
                int x = 0;
                while(x < width) {
                    if(pos >= data.size())
                        return QImage();
                    int count = uchar(data.at(pos++));
                    if(count > 128) {
                        count -= 128;
                        if(count == 0 || x + count > width || pos >= data.size())
                            return QImage();
                        const uchar value = uchar(data.at(pos++));
                        for(int i = 0; i < count; i++)
                            scanline[size_t(4 * (x++) + component)] = value;
                    } else {
                        if(count == 0 || x + count > width || pos + count > data.size())
                            return QImage();
                        for(int i = 0; i < count; i++)
                            scanline[size_t(4 * (x++) + component)] = uchar(data.at(pos++));
                    }
                }
            }
        }

        uchar *dest = image.scanLine(header.flipY ? height - 1 - y : y);
        for(int x = 0; x < width; x++) {
            float r = 0.0f, g = 0.0f, b = 0.0f;
            rgbeToRgb(&scanline[size_t(x) * 4], r, g, b);
            const int outX = header.flipX ? width - 1 - x : x;
            uchar *pixel = dest + size_t(outX) * 3;
            pixel[0] = toneMap(r);
            pixel[1] = toneMap(g);
            pixel[2] = toneMap(b);
        }
    }

    if(targetSize.isValid()
       && (targetSize.width() < width || targetSize.height() < height)) {
        return image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return image;
}

} // namespace HdrDecoder
