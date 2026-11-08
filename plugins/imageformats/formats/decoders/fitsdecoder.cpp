#include "fitsdecoder.h"

#include <QFile>

#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace {

struct FitsHeader {
    bool ok = false;
    int bitPix = 0;
    int width = 0;
    int height = 0;
    int dataOffset = 0;
};

QByteArray cardValue(const QByteArray &card)
{
    // "KEYWORD = value / comment": everything between the '=' and the '/'
    const int equals = card.indexOf('=');
    if(equals < 0)
        return QByteArray();
    QByteArray value = card.mid(equals + 1);
    const int comment = value.indexOf('/');
    if(comment >= 0)
        value.truncate(comment);
    return value.trimmed();
}

FitsHeader parseHeader(const QByteArray &data)
{
    FitsHeader header;
    int position = 0;
    while(position + 80 <= data.size()) {
        const QByteArray card = data.mid(position, 80);
        const QByteArray keyword = card.left(8).trimmed();
        if(keyword == "END")
            break;
        if(keyword == "SIMPLE") {
            // only the primary HDU is read
        } else if(keyword == "BITPIX") {
            header.bitPix = cardValue(card).toInt();
        } else if(keyword == "NAXIS1") {
            header.width = cardValue(card).toInt();
        } else if(keyword == "NAXIS2") {
            header.height = cardValue(card).toInt();
        }
        position += 80;
    }
    if(header.width <= 0 || header.height <= 0)
        return header;
    if(header.bitPix != 8 && header.bitPix != 16 && header.bitPix != 32
       && header.bitPix != -32 && header.bitPix != -64) {
        return header;
    }
    // the data starts on the next 2880 byte boundary after the END card
    header.dataOffset = ((position / 2880) + 1) * 2880;
    header.ok = true;
    return header;
}

double readBigEndianValue(const uchar *bytes, int bitPix)
{
    switch(bitPix) {
    case 8:
        return bytes[0];
    case 16:
        return double(qint16((quint16(bytes[0]) << 8) | quint16(bytes[1])));
    case 32:
        return double(qint32((quint32(bytes[0]) << 24) | (quint32(bytes[1]) << 16)
                             | (quint32(bytes[2]) << 8) | quint32(bytes[3])));
    case -32: {
        const quint32 raw = (quint32(bytes[0]) << 24) | (quint32(bytes[1]) << 16)
                            | (quint32(bytes[2]) << 8) | quint32(bytes[3]);
        float value = 0.0f;
        std::memcpy(&value, &raw, 4);
        return value;
    }
    case -64: {
        const quint64 raw = (quint64(bytes[0]) << 56) | (quint64(bytes[1]) << 48)
                            | (quint64(bytes[2]) << 40) | (quint64(bytes[3]) << 32)
                            | (quint64(bytes[4]) << 24) | (quint64(bytes[5]) << 16)
                            | (quint64(bytes[6]) << 8) | quint64(bytes[7]);
        double value = 0.0;
        std::memcpy(&value, &raw, 8);
        return value;
    }
    default:
        return 0.0;
    }
}

} // namespace

namespace FitsImage {

bool isFitsSuffix(const QString &suffix)
{
    return suffix.compare(QLatin1String("fits"), Qt::CaseInsensitive) == 0
           || suffix.compare(QLatin1String("fit"), Qt::CaseInsensitive) == 0;
}

QSize size(const QString &path)
{
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
        return QSize();
    const FitsHeader header = parseHeader(file.read(32 * 1024));
    return header.ok ? QSize(header.width, header.height) : QSize();
}

QImage read(const QString &path, const QSize &targetSize)
{
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
        return QImage();
    const QByteArray data = file.readAll();
    const FitsHeader header = parseHeader(data);
    if(!header.ok)
        return QImage();

    const int bytesPerSample = header.bitPix == 8 ? 1 : (header.bitPix == 16 ? 2
                             : (header.bitPix == 32 || header.bitPix == -32) ? 4 : 8);
    const qint64 pixelCount = qint64(header.width) * header.height;
    if(qint64(data.size()) - header.dataOffset < pixelCount * bytesPerSample)
        return QImage();

    const uchar *samples = reinterpret_cast<const uchar *>(data.constData()) + header.dataOffset;
    std::vector<double> sampleValues(static_cast<size_t>(pixelCount));
    double minimum = std::numeric_limits<double>::max();
    double maximum = -std::numeric_limits<double>::max();
    for(qint64 i = 0; i < pixelCount; i++) {
        const double value = readBigEndianValue(samples + i * bytesPerSample, header.bitPix);
        sampleValues[static_cast<size_t>(i)] = value;
        if(std::isfinite(value)) {
            minimum = qMin(minimum, value);
            maximum = qMax(maximum, value);
        }
    }
    if(!(maximum > minimum))
        maximum = minimum + 1.0;

    QImage image(header.width, header.height, QImage::Format_RGB888);
    if(image.isNull())
        return QImage();
    const double range = maximum - minimum;
    for(int y = 0; y < header.height; y++) {
        // FITS rows start at the bottom
        const double *row = &sampleValues[static_cast<size_t>(qint64(header.height - 1 - y) * header.width)];
        uchar *dest = image.scanLine(y);
        for(int x = 0; x < header.width; x++) {
            double normalized = (row[x] - minimum) / range;
            // the usual stretch for astronomical data: a gentle gamma keeps the
            // dim detail visible
            normalized = std::pow(qBound(0.0, normalized, 1.0), 1.0 / 2.2);
            const uchar value = uchar(qBound(0, int(normalized * 255.0 + 0.5), 255));
            dest[x * 3 + 0] = value;
            dest[x * 3 + 1] = value;
            dest[x * 3 + 2] = value;
        }
    }

    if(targetSize.isValid()
       && (targetSize.width() < image.width() || targetSize.height() < image.height())) {
        return image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return image;
}

} // namespace FitsImage
