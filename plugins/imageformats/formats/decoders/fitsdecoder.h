#pragma once

#include <QImage>
#include <QSize>
#include <QString>

// FITS (Flexible Image Transport System), the astronomy format: 80 byte header
// cards in 2880 byte blocks followed by the pixel data as big endian integers or
// floats. Only the primary image is read.
namespace FitsImage {

bool isFitsSuffix(const QString &suffix);

QSize size(const QString &path);

QImage read(const QString &path, const QSize &targetSize = QSize());

} // namespace FitsImage
