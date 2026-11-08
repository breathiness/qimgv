#include "anidecoder.h"

#include <QByteArray>
#include <QFile>

namespace {

quint32 readU32(const QByteArray &data, int offset)
{
    if(offset < 0 || offset + 4 > data.size())
        return 0;
    return quint32(uchar(data.at(offset)))
           | (quint32(uchar(data.at(offset + 1))) << 8)
           | (quint32(uchar(data.at(offset + 2))) << 16)
           | (quint32(uchar(data.at(offset + 3))) << 24);
}

// Header sizes are untrusted: a broken file must not walk past the buffer.
int clampSize(const QByteArray &data, int payload, quint32 size, int limit)
{
    const int available = qMin(data.size(), limit) - payload;
    if(available <= 0)
        return 0;
    return qMin(int(size), available);
}

struct AniHeader {
    int frameCount = 0;
    int displayRate = 0;     // jiffies (1/60 s)
    quint32 flags = 0;
};

// Walks the RIFF chunks. Frames are .ico blobs inside LIST "fram"; the rate and
// sequence chunks decide how they are shown.
bool parse(const QByteArray &data, AniHeader *header,
           QVector<quint32> *rates, QVector<quint32> *sequence,
           QVector<QByteArray> *icons)
{
    if(data.size() < 12 || !data.startsWith("RIFF") || data.mid(8, 4) != "ACON")
        return false;

    int pos = 12;
    while(pos + 8 <= data.size()) {
        const QByteArray id = data.mid(pos, 4);
        quint32 size = readU32(data, pos + 4);
        const int payload = pos + 8;
        size = quint32(clampSize(data, payload, size, data.size()));
        if(size == 0 && id != "LIST")
            break;

        if(id == "anih" && size >= 36) {
            header->frameCount = int(readU32(data, payload + 4));
            header->displayRate = int(readU32(data, payload + 28));
            header->flags = readU32(data, payload + 32);
        } else if(id == "rate") {
            const int count = int(size / 4);
            for(int i = 0; i < count; i++)
                rates->append(readU32(data, payload + i * 4));
        } else if(id == "seq ") {
            const int count = int(size / 4);
            for(int i = 0; i < count; i++)
                sequence->append(readU32(data, payload + i * 4));
        } else if(id == "LIST" && size >= 4 && data.mid(payload, 4) == "fram") {
            int sub = payload + 4;
            const int end = payload + int(size);
            while(sub + 8 <= end) {
                const QByteArray subId = data.mid(sub, 4);
                const int subPayload = sub + 8;
                const int subSize = clampSize(data, subPayload, readU32(data, sub + 4), end);
                if(subSize <= 0)
                    break;
                if(subId == "icon")
                    icons->append(data.mid(subPayload, subSize));
                sub = subPayload + subSize + (subSize & 1);
            }
        }
        pos = payload + int(size) + (int(size) & 1);
    }
    return !icons->isEmpty();
}

} // namespace

namespace AniImage {

bool isAniSuffix(const QString &suffix)
{
    return suffix.compare(QLatin1String("ani"), Qt::CaseInsensitive) == 0;
}

QSize size(const QString &path)
{
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
        return QSize();
    // the header chunks sit at the start; the frames follow
    const QByteArray head = file.read(4096);
    AniHeader header;
    QVector<quint32> rates;
    QVector<quint32> sequence;
    QVector<QByteArray> icons;
    parse(head, &header, &rates, &sequence, &icons);
    for(const QByteArray &icon : icons) {
        const QImage image = QImage::fromData(icon);
        if(!image.isNull())
            return image.size();
    }
    return QSize();
}

bool readAnimation(const QString &path, QVector<Frame> *frames)
{
    if(!frames)
        return false;
    frames->clear();

    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray data = file.readAll();

    AniHeader header;
    QVector<quint32> rates;
    QVector<quint32> sequence;
    QVector<QByteArray> icons;
    if(!parse(data, &header, &rates, &sequence, &icons))
        return false;

    // Decode every icon once; the sequence may replay frames several times.
    QVector<QImage> decoded;
    decoded.reserve(icons.size());
    for(const QByteArray &icon : icons) {
        const QImage image = QImage::fromData(icon);
        if(image.isNull())
            return false;
        decoded.append(image);
    }

    int steps = sequence.isEmpty() ? decoded.size() : sequence.size();
    if(header.frameCount > 0 && sequence.isEmpty())
        steps = qMin(steps, header.frameCount);
    if(steps <= 0)
        return false;

    for(int step = 0; step < steps; step++) {
        int index = sequence.isEmpty() ? step : int(sequence.at(step));
        if(index < 0 || index >= decoded.size())
            index = step % decoded.size();

        // the rate chunk is in jiffies (1/60 s); the header holds the fallback
        const quint32 ticks = step < rates.size() ? rates.at(step) : quint32(header.displayRate);
        int delayMs = ticks > 0 ? int(qint64(ticks) * 1000 / 60) : 100;
        if(delayMs <= 0)
            delayMs = 100;

        Frame frame;
        frame.image = decoded.at(index);
        frame.delayMs = delayMs;
        frames->append(frame);
    }
    return !frames->isEmpty();
}

} // namespace AniImage
