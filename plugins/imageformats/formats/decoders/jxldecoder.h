#pragma once

#include <QString>
#include <QSize>
#include <QImage>
#include <QVector>

// JPEG XL images, decoded with libjxl.
//
// Qt has no JXL reader in this build (the format needs a plugin), so the viewer
// and the thumbnailer fall back to this when a .jxl file cannot be opened the
// usual way. `read()` decodes the first frame, `readAnimation()` the sequence.
namespace JxlImage {

// true when libjxl was linked in
bool isAvailable();

// true for the file suffixes this decoder handles ("jxl")
bool isJxlSuffix(const QString &suffix);

// Size of the image, without decoding the pixels. Invalid when the file cannot
// be read.
QSize size(const QString &path);

// true when the codestream header says the file is animated (header only)
bool isAnimated(const QString &path);

// Decodes the first frame. A valid targetSize only asks the decoder for a
// downscaled result where the format allows it (JXL has no cheap sub-sampling,
// so the image is decoded and then resized).
QImage read(const QString &path, const QSize &targetSize = QSize());

struct Frame {
    QImage image;
    int delayMs = 100;
};

struct Animation {
    QVector<Frame> frames;
    // JXL num_loops: 0 means "forever", n means "play n times"
    int numLoops = 0;
    // the file had more frames than the memory budget allowed; the frames that
    // are here are still correct and in order
    bool truncated = false;
};

// Decodes every displayed frame. libjxl coalesces them, so each frame is a full
// canvas; the frames stay in memory and the budget in the implementation bounds
// that cost, stopping early (and setting `truncated`) when it would be exceeded.
bool readAnimation(const QString &path, Animation *out,
                   const QSize &targetSize = QSize());

} // namespace JxlImage
