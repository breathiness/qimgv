#pragma once

#include <QImage>
#include <QSize>
#include <QString>

// Radiance HDR (.hdr, .rgbe): RGBE encoded high dynamic range images. The
// decoder is self contained, no extra library is involved.
namespace HdrDecoder {

bool isHdrSuffix(const QString &suffix);

// Image size from the header, without decoding pixels.
QSize size(const QString &path);

// Decodes the image and tone maps it for an 8 bit display. A valid targetSize
// asks for a downscaled result.
QImage read(const QString &path, const QSize &targetSize = QSize());

} // namespace HdrDecoder
