#pragma once

#include <QString>
#include <QSize>
#include <QImage>

// Camera raw images (CR2/CR3, NEF, ARW, DNG, RAF, ORF, RW2, ...) through libraw.
//
// Qt cannot open these at all. libraw can, and it also exposes the preview the
// camera stores inside the file, which is what a thumbnail tile should use: the
// embedded JPEG decodes in milliseconds, while a full raw development takes
// hundreds of them.
namespace RawImage {

// true when libraw was linked in
bool isAvailable();

// true for the camera raw suffixes handled below
bool isRawSuffix(const QString &suffix);

// Size of the developed image (already accounting for the camera's rotation).
// Invalid when the file cannot be read.
QSize size(const QString &path);

// Develops the image. A valid targetSize asks libraw for half resolution where
// the raw size is far larger than what is on screen.
QImage read(const QString &path, const QSize &targetSize = QSize());

// The preview stored inside the file, or an empty image when there is none.
QImage readPreview(const QString &path);

} // namespace RawImage
