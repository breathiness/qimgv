#pragma once

#include <QImageIOHandler>
#include <QImage>
#include <QSize>
#include <QRect>
#include <QByteArray>
#include <QVector>

// Animated PNG reader.
//
// Qt's own PNG plugin reads the default image and stops, so an APNG plays as a
// single frame. This handler claims the "apng" key only (qimgv asks for that
// format explicitly once it has detected the acTL chunk), decodes every frame by
// handing Qt's PNG decoder a one-frame PNG built from the fdAT/IDAT payloads, and
// composites the result the way the format prescribes.
class ApngHandler : public QImageIOHandler
{
public:
    ApngHandler();

    bool canRead() const override;
    bool read(QImage *image) override;
    bool supportsOption(ImageOption option) const override;

    bool jumpToNextImage() override;
    bool jumpToImage(int imageNumber) override;
    int imageCount() const override;
    int currentImageNumber() const override;
    QRect currentImageRect() const override;
    int loopCount() const override;
    int nextImageDelay() const override;

private:
    struct Frame {
        QRect rect;
        int delayMs = 100;
        int disposeOp = 0;   // 0 none, 1 background, 2 previous
        int blendOp = 0;     // 0 source, 1 over
        QByteArray data;     // zlib stream of one frame
    };

    bool ensureParsed() const;
    QImage decodeFrame(int index) const;
    QImage renderUpTo(int index) const;
    bool composeFrame(QImage &canvas, int index) const;
    void disposeFrame(QImage &canvas, const Frame &frame) const;

    mutable bool mParsed = false;
    mutable bool mValid = false;
    mutable QSize mSize;
    mutable quint8 mBitDepth = 8;
    mutable quint8 mColorType = 6;
    mutable quint8 mInterlace = 0;
    mutable QByteArray mPalette;
    mutable QByteArray mTransparency;
    mutable QVector<Frame> mFrames;
    mutable int mPlayCount = 0;
    mutable int mCurrentFrame = 0;
    mutable int mNextFrame = 0;
    // canvas kept between frames so sequential playback is one compose per frame
    mutable QImage mCanvas;
    mutable int mCanvasFrame = -1;
    mutable QImage mLastFrameBefore;
};
