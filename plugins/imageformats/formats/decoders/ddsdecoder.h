#pragma once

#include <QImage>
#include <QSize>
#include <QString>

// DirectDraw Surface (.dds): game textures. The pixel data is usually BC1-BC7
// compressed (decoded with the vendored bcdec header) but plain 24/32 bit
// surfaces show up too.
namespace DdsImage {

bool isDdsSuffix(const QString &suffix);

// Size of the top mip level, from the header.
QSize size(const QString &path);

QImage read(const QString &path, const QSize &targetSize = QSize());

} // namespace DdsImage
