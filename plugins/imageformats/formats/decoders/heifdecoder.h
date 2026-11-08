#pragma once

#include <QImage>
#include <QString>
#include <QSize>
#include <QVector>

// HEIC/HEIF and AVIF still images, decoded with libheif.
//
// Qt has no reader for these formats unless a third-party plugin happens to be
// installed. libheif also exposes the thumbnail an encoder usually stores, which
// makes the folder view cheap: iPhones put a ~320 px preview in every picture.
namespace HeifDecoder {

// one picture of an image sequence (animated HEIC/AVIF)
struct Frame {
    QImage image;
    int delayMs = 0;
};

struct Animation {
    QVector<Frame> frames;
    // number of plays, 0 = forever (same convention as the JPEG XL decoder)
    int numLoops = 1;
};

// true when libheif was linked in
bool isAvailable();

// true for the file suffixes this decoder handles ("heic", "heif", "hif", "avif")
bool isHeifSuffix(const QString &suffix);

// Size of the primary image, without decoding the pixels. Invalid when the file
// cannot be read.
QSize size(const QString &path);

// Decodes the primary image. A valid targetSize asks for a downscaled result
// (still decoded in full, then resized - libheif has no reduced-size decode).
QImage read(const QString &path, const QSize &targetSize = QSize());

// Decodes the embedded thumbnail, or an empty image when the file has none or it
// is smaller than the requested size would need.
QImage readThumbnail(const QString &path, int minSize);

// True when the file carries an image sequence rather than a single picture.
// Only reads the container, no pixels are decoded.
bool isAnimated(const QString &path);

// Decodes every picture of the sequence with its display duration. False for a
// still image, a container libheif refuses, or a sequence shorter than two
// pictures (which needs no animation path).
bool readAnimation(const QString &path, Animation *animation);

} // namespace HeifDecoder
