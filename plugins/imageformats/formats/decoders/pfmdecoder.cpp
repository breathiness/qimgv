#include "pfmdecoder.h"

#include <QFile>

#include <cmath>
#include <cstring>
#include <vector>

namespace {

struct PfmHeader {
    bool ok = false;
    bool color = true;
    int width = 0;
    int height = 0;
    bool littleEndian = true;
    int dataOffset = 0;
};

// Three lines: "PF"/"Pf", "width height" and the scale factor (negative means
// little endian samples).
PfmHeader parseHeader(const QByteArray &data)
{
    PfmHeader header;
    QList<QByteArray> lines;
    int pos = 0;
    while(lines.size() < 3 && pos < data.size()) {
        const int eol = data.indexOf('\n', pos);
        QByteArray line = eol < 0 ? data.mid(pos) : data.mid(pos, eol - pos);
        if(line.endsWith('\r'))
            line.chop(1);
        lines.append(line);
        pos = eol < 0 ? data.size() : eol + 1;
    }
    if(lines.size() < 3)
        return header;

    const QByteArray signature = lines.at(0).trimmed();
    if(signature != "PF" && signature != "Pf")
        return header;
    const QList<QByteArray> sizes = lines.at(1).simplified().split(' ');
    if(sizes.size() != 2)
        return header;
    header.width = sizes.at(0).toInt();
    header.height = sizes.at(1).toInt();
    if(header.width <= 0 || header.height <= 0)
        return header;
    const float scaleValue = lines.at(2).trimmed().toFloat();
    if(scaleValue == 0.0f)
        return header;
    header.color = signature == "PF";
    header.littleEndian = scaleValue < 0.0f;
    header.dataOffset = pos;
    header.ok = true;
    return header;
}

float readFloat(const uchar *bytes, bool littleEndian)
{
    uchar buffer[4];
    if(littleEndian) {
        buffer[0] = bytes[0];
        buffer[1] = bytes[1];
        buffer[2] = bytes[2];
        buffer[3] = bytes[3];
    } else {
        buffer[0] = bytes[3];
        buffer[1] = bytes[2];
        buffer[2] = bytes[1];
        buffer[3] = bytes[0];
    }
    float value = 0.0f;
    std::memcpy(&value, buffer, 4);
    return value;
}

inline uchar toneMap(float value)
{
    if(!(value > 0.0f))
        return 0;
    const float clamped = value > 1.0f ? 1.0f : value;
    return uchar(qBound(0, int(std::pow(clamped, 1.0f / 2.2f) * 255.0f + 0.5f), 255));
}

} // namespace

namespace PfmImage {

bool isPfmSuffix(const QString &suffix)
{
    return suffix.compare(QLatin1String("pfm"), Qt::CaseInsensitive) == 0;
}

QSize size(const QString &path)
{
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
        return QSize();
    const PfmHeader header = parseHeader(file.read(256));
    return header.ok ? QSize(header.width, header.height) : QSize();
}

QImage read(const QString &path, const QSize &targetSize)
{
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
        return QImage();
    const QByteArray data = file.readAll();
    const PfmHeader header = parseHeader(data);
    if(!header.ok)
        return QImage();

    const int channels = header.color ? 3 : 1;
    const qint64 rowBytes = qint64(header.width) * channels * 4;
    if(qint64(data.size()) - header.dataOffset < rowBytes * header.height)
        return QImage();

    QImage image(header.width, header.height, QImage::Format_RGB888);
    if(image.isNull())
        return QImage();

    const uchar *samples = reinterpret_cast<const uchar *>(data.constData()) + header.dataOffset;
    for(int y = 0; y < header.height; y++) {
        // the file stores the last row first
        const uchar *row = samples + rowBytes * (header.height - 1 - y);
        uchar *dest = image.scanLine(y);
        for(int x = 0; x < header.width; x++) {
            uchar *pixel = dest + x * 3;
            if(channels == 3) {
                const uchar *source = row + x * 12;
                pixel[0] = toneMap(readFloat(source, header.littleEndian));
                pixel[1] = toneMap(readFloat(source + 4, header.littleEndian));
                pixel[2] = toneMap(readFloat(source + 8, header.littleEndian));
            } else {
                const uchar value = toneMap(readFloat(row + x * 4, header.littleEndian));
                pixel[0] = pixel[1] = pixel[2] = value;
            }
        }
    }

    if(targetSize.isValid()
       && (targetSize.width() < image.width() || targetSize.height() < image.height())) {
        return image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return image;
}

} // namespace PfmImage
