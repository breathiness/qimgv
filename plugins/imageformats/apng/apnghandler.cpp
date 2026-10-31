#include "apnghandler.h"

#include <QBuffer>
#include <QPainter>
#include <QImageReader>
#include <QIODevice>
#include <cstring>

namespace {

// CRC32 for the chunks of the one-frame PNGs we build. Kept local so the plugin
// does not depend on zlib.
quint32 crc32(const uchar *data, int length)
{
    static quint32 table[256];
    static bool ready = false;
    if(!ready) {
        for(quint32 n = 0; n < 256; ++n) {
            quint32 c = n;
            for(int k = 0; k < 8; ++k)
                c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1;
            table[n] = c;
        }
        ready = true;
    }
    quint32 c = 0xffffffffu;
    for(int i = 0; i < length; i++)
        c = table[(c ^ data[i]) & 0xff] ^ (c >> 8);
    return c ^ 0xffffffffu;
}

quint32 readU32(const uchar *p)
{
    return (quint32(p[0]) << 24) | (quint32(p[1]) << 16) | (quint32(p[2]) << 8) | quint32(p[3]);
}

quint16 readU16(const uchar *p)
{
    return quint16((quint16(p[0]) << 8) | p[1]);
}

void appendChunk(QByteArray &out, const char *type, const QByteArray &data)
{
    const quint32 length = quint32(data.size());
    uchar header[4] = {uchar(length >> 24), uchar(length >> 16), uchar(length >> 8), uchar(length)};
    out.append(reinterpret_cast<const char *>(header), 4);
    const int typeAt = out.size();
    out.append(type, 4);
    out.append(data);
    const quint32 crc = crc32(reinterpret_cast<const uchar *>(out.constData()) + typeAt,
                              int(4 + data.size()));
    uchar crcBytes[4] = {uchar(crc >> 24), uchar(crc >> 16), uchar(crc >> 8), uchar(crc)};
    out.append(reinterpret_cast<const char *>(crcBytes), 4);
}

QByteArray buildPng(const QSize &size, quint8 bitDepth, quint8 colorType, quint8 interlace,
                    const QByteArray &palette, const QByteArray &transparency,
                    const QByteArray &frameData)
{
    QByteArray out;
    out.append("\x89PNG\r\n\x1a\n", 8);

    QByteArray ihdr;
    ihdr.append(char(size.width() >> 24));
    ihdr.append(char(size.width() >> 16));
    ihdr.append(char(size.width() >> 8));
    ihdr.append(char(size.width()));
    ihdr.append(char(size.height() >> 24));
    ihdr.append(char(size.height() >> 16));
    ihdr.append(char(size.height() >> 8));
    ihdr.append(char(size.height()));
    ihdr.append(char(bitDepth));
    ihdr.append(char(colorType));
    ihdr.append(char(0)); // compression
    ihdr.append(char(0)); // filter
    ihdr.append(char(interlace));
    appendChunk(out, "IHDR", ihdr);

    if(!palette.isEmpty())
        appendChunk(out, "PLTE", palette);
    if(!transparency.isEmpty())
        appendChunk(out, "tRNS", transparency);
    appendChunk(out, "IDAT", frameData);
    appendChunk(out, "IEND", QByteArray());
    return out;
}

} // namespace

ApngHandler::ApngHandler() = default;

bool ApngHandler::canRead() const
{
    if(!ensureParsed())
        return false;
    return mValid;
}

bool ApngHandler::supportsOption(ImageOption option) const
{
    switch(option) {
    case Animation:
    case Size:
    case ImageFormat:
    case BackgroundColor:
        return true;
    default:
        return QImageIOHandler::supportsOption(option);
    }
}

bool ApngHandler::ensureParsed() const
{
    if(mParsed)
        return mValid;
    mParsed = true;
    mValid = false;

    QIODevice *device = this->device();
    if(!device)
        return false;
    const bool wasOpen = device->isOpen();
    if(!wasOpen && !device->open(QIODevice::ReadOnly))
        return false;

    const qint64 start = device->pos();
    const QByteArray blob = device->readAll();
    if(!wasOpen)
        device->close();

    if(blob.size() < 8 || blob.left(8) != QByteArray("\x89PNG\r\n\x1a\n", 8))
        return false;

    QByteArray idat;
    QByteArray frameData;          // data of the frame currently being collected
    bool seenIdat = false;
    bool hasAnimation = false;
    bool idatSeen = false;
    bool idatIsFrame = false;
    int currentFrame = -1;

    int offset = 8;
    while(offset + 8 <= blob.size()) {
        const quint32 length = readU32(reinterpret_cast<const uchar *>(blob.constData()) + offset);
        const QByteArray type = blob.mid(offset + 4, 4);
        const int dataOffset = offset + 8;
        if(dataOffset + int(length) > blob.size())
            return false;
        const QByteArray data = blob.mid(dataOffset, int(length));

        if(type == "IHDR") {
            if(data.size() < 13)
                return false;
            const uchar *p = reinterpret_cast<const uchar *>(data.constData());
            mSize = QSize(int(readU32(p)), int(readU32(p + 4)));
            mBitDepth = quint8(p[8]);
            mColorType = quint8(p[9]);
            mInterlace = quint8(p[12]);
        } else if(type == "PLTE") {
            mPalette = data;
        } else if(type == "tRNS") {
            mTransparency = data;
        } else if(type == "acTL") {
            if(data.size() < 8)
                return false;
            const uchar *p = reinterpret_cast<const uchar *>(data.constData());
            mPlayCount = int(readU32(p + 4));
            hasAnimation = true;
        } else if(type == "fcTL") {
            if(data.size() < 26)
                return false;
            // the data collected so far belongs to the frame that just ended
            if(currentFrame >= 0) {
                mFrames[currentFrame].data = frameData;
                frameData.clear();
            }
            Frame frame;
            const uchar *p = reinterpret_cast<const uchar *>(data.constData());
            // sequence number, then width/height/x/y
            frame.rect = QRect(int(readU32(p + 12)), int(readU32(p + 16)),
                               int(readU32(p + 4)), int(readU32(p + 8)));
            const quint16 delayNum = readU16(p + 20);
            const quint16 delayDen = readU16(p + 22);
            const int denominator = delayDen == 0 ? 100 : delayDen;
            frame.delayMs = qMax(1, int(qint64(delayNum) * 1000 / denominator));
            frame.disposeOp = p[24];
            frame.blendOp = p[25];
            mFrames.append(frame);
            currentFrame = mFrames.size() - 1;
        } else if(type == "IDAT") {
            if(!idatSeen) {
                idatSeen = true;
                // a default image that is not part of the animation sits before
                // the first fcTL and is skipped
                idatIsFrame = currentFrame >= 0;
            }
            if(idatIsFrame)
                frameData.append(data);
        } else if(type == "fdAT") {
            if(currentFrame < 0 || data.size() < 4)
                return false;
            // 4 byte sequence number, then the frame's zlib data
            frameData.append(data.mid(4));
        } else if(type == "IEND") {
            break;
        }

        offset = dataOffset + int(length) + 4; // + crc
    }

    // the data collected after the last fcTL belongs to the last frame
    if(currentFrame >= 0)
        mFrames[currentFrame].data = frameData;

    if(!hasAnimation || mFrames.isEmpty() || mSize.isEmpty())
        return false;
    for(const Frame &frame : mFrames) {
        if(frame.data.isEmpty())
            return false;
    }
    mValid = true;
    mCurrentFrame = 0;
    Q_UNUSED(start)
    return true;
}

QImage ApngHandler::decodeFrame(int index) const
{
    if(index < 0 || index >= mFrames.size())
        return QImage();
    const Frame &frame = mFrames[index];
    if(frame.rect.isEmpty())
        return QImage();
    QByteArray png = buildPng(frame.rect.size(), mBitDepth, mColorType, mInterlace,
                              mPalette, mTransparency, frame.data);
    QBuffer buffer(&png);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, "png");
    QImage image = reader.read();
    if(image.isNull()) {
        return QImage();
    }
    return image.convertToFormat(QImage::Format_ARGB32);
}

QImage ApngHandler::renderUpTo(int index) const
{
    if(mSize.isEmpty() || index < 0 || index >= mFrames.size())
        return QImage();
    if(index == mCanvasFrame && !mCanvas.isNull())
        return mCanvas;

    if(index == mCanvasFrame + 1 && !mCanvas.isNull()) {
        // sequential playback: dispose the frame we returned last time, then
        // compose the next one
        disposeFrame(mCanvas, mFrames[mCanvasFrame]);
    } else {
        mCanvas = QImage(mSize, QImage::Format_ARGB32);
        mCanvas.fill(Qt::transparent);
        mCanvasFrame = -1;
        for(int i = 0; i < index; i++) {
            if(!composeFrame(mCanvas, i))
                return QImage();
            disposeFrame(mCanvas, mFrames[i]);
        }
    }

    QImage before = mCanvas;   // needed by dispose_op 2 (restore to previous)
    if(!composeFrame(mCanvas, index))
        return QImage();
    mLastFrameBefore = before;
    mCanvasFrame = index;
    return mCanvas;
}

bool ApngHandler::composeFrame(QImage &canvas, int index) const
{
    const Frame &frame = mFrames[index];
    QImage pixels = decodeFrame(index);
    if(pixels.isNull())
        return false;
    QPainter painter(&canvas);
    painter.setCompositionMode(frame.blendOp == 0 ? QPainter::CompositionMode_Source
                                                  : QPainter::CompositionMode_SourceOver);
    painter.drawImage(frame.rect.topLeft(), pixels);
    return true;
}

void ApngHandler::disposeFrame(QImage &canvas, const Frame &frame) const
{
    switch(frame.disposeOp) {
    case 1: { // background
        QPainter clear(&canvas);
        clear.setCompositionMode(QPainter::CompositionMode_Clear);
        clear.fillRect(frame.rect, Qt::transparent);
        clear.end();
    } break;
    case 2: // previous
        if(!mLastFrameBefore.isNull())
            canvas = mLastFrameBefore;
        break;
    default:
        break;
    }
}

bool ApngHandler::read(QImage *image)
{
    if(!ensureParsed() || !image || mFrames.isEmpty())
        return false;

    // QMovie reads frames by calling read() repeatedly without asking the handler
    // to advance, so each call produces the next frame (this is also how Qt's own
    // GIF and WebP handlers behave). jumpToImage() repositions that sequence.
    const int index = mNextFrame;
    if(index < 0 || index >= mFrames.size())
        return false;

    if(mCanvasFrame != index - 1 || mCanvas.isNull()) {
        mCanvas = QImage(mSize, QImage::Format_ARGB32);
        mCanvas.fill(Qt::transparent);
        mCanvasFrame = -1;
        for(int i = 0; i < index; i++) {
            if(!composeFrame(mCanvas, i))
                return false;
            disposeFrame(mCanvas, mFrames[i]);
        }
    }
    const QImage before = mCanvas;
    if(!composeFrame(mCanvas, index))
        return false;
    mLastFrameBefore = before;
    mCanvasFrame = index;
    mCurrentFrame = index;
    mNextFrame = (index + 1) % mFrames.size();
    *image = mCanvas;
    return true;
}

int ApngHandler::imageCount() const
{
    if(!ensureParsed())
        return 0;
    return mFrames.size();
}

int ApngHandler::currentImageNumber() const
{
    return mCurrentFrame;
}

QRect ApngHandler::currentImageRect() const
{
    if(!ensureParsed())
        return QRect();
    if(mCurrentFrame < 0 || mCurrentFrame >= mFrames.size())
        return QRect(QPoint(0, 0), mSize);
    return mFrames[mCurrentFrame].rect;
}

bool ApngHandler::jumpToImage(int imageNumber)
{
    if(!ensureParsed())
        return false;
    if(imageNumber < 0 || imageNumber >= mFrames.size())
        return false;
    mNextFrame = imageNumber;
    mCurrentFrame = imageNumber;
    return true;
}

bool ApngHandler::jumpToNextImage()
{
    if(!ensureParsed() || mFrames.isEmpty())
        return false;
    mNextFrame = (mNextFrame + 1) % mFrames.size();
    mCurrentFrame = mNextFrame;
    return true;
}

int ApngHandler::loopCount() const
{
    if(!ensureParsed())
        return 0;
    return mPlayCount;
}

int ApngHandler::nextImageDelay() const
{
    if(!ensureParsed())
        return 100;
    if(mCurrentFrame < 0 || mCurrentFrame >= mFrames.size())
        return 100;
    return mFrames[mCurrentFrame].delayMs;
}
