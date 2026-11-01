#pragma once

#include <QImageIOHandler>
#include <QImage>
#include <QSize>
#include <QVariant>
#include <QString>
#include <QVector>

#include "decoders/jxldecoder.h"

// Reads the formats qimgv cannot get from Qt itself: HEIC/HEIF/AVIF (libheif),
// JPEG XL (libjxl), camera raw (libraw) and PSD/PSB (parsed here).
//
// All of them live in this plugin so the application itself stays free of those
// dependencies: dropping the plugin out of imageformats/ only removes the
// formats, the viewer still starts and runs.
class FormatsHandler : public QImageIOHandler
{
public:
    FormatsHandler();

    bool canRead() const override;
    bool read(QImage *image) override;
    bool supportsOption(ImageOption option) const override;
    QVariant option(ImageOption option) const override;
    void setOption(ImageOption option, const QVariant &value) override;

    // animated JPEG XL: QMovie drives these
    int imageCount() const override;
    int currentImageNumber() const override;
    bool jumpToImage(int imageNumber) override;
    bool jumpToNextImage() override;
    int loopCount() const override;
    int nextImageDelay() const override;

    // in-memory decode is not supported, the decoders work on files
    static QStringList supportedSuffixes();
    static bool isSupportedSuffix(const QString &suffix);

private:
    QString filePath() const;
    QString resolvedSuffix() const;
    QImage decode(const QString &path, const QString &suffix, const QSize &target) const;
    // decodes the whole sequence once; false for still images (and for frames
    // too large to keep in memory)
    bool ensureAnimationLoaded() const;

    // QImageReader forwards setScaledSize() through setOption(). A handler that
    // claims ScaledSize support must do the scaling itself: Qt only rescales the
    // result for handlers that do *not* support it.
    QSize mScaledSize;

    mutable bool mAnimProbed = false;
    mutable bool mAnimIsAnimated = false;
    mutable QVector<JxlImage::Frame> mAnimFrames;
    mutable int mAnimNumLoops = 0;
    mutable int mAnimNext = 0;
    mutable int mAnimCurrent = 0;
};
