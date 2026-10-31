#pragma once

#include <QString>
#include <QSize>
#include <QImage>

// JPEG XL still images, decoded with libjxl.
//
// Qt has no JXL reader in this build (the format needs a plugin), so the viewer
// and the thumbnailer fall back to this when a .jxl file cannot be opened the
// usual way. Only the first frame is decoded: an animated JXL shows its first
// frame instead of not opening at all.
namespace JxlImage {

// true when libjxl was linked in
bool isAvailable();

// true for the file suffixes this decoder handles ("jxl")
bool isJxlSuffix(const QString &suffix);

// Size of the image, without decoding the pixels. Invalid when the file cannot
// be read.
QSize size(const QString &path);

// Decodes the first frame. A valid targetSize only asks the decoder for a
// downscaled result where the format allows it (JXL has no cheap sub-sampling,
// so the image is decoded and then resized).
QImage read(const QString &path, const QSize &targetSize = QSize());

} // namespace JxlImage
