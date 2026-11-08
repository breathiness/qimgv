#pragma once

#include <QImage>
#include <QSize>
#include <QString>

// OpenEXR (.exr), decoded with the OpenEXR library. These are 16 bit half float
// images with linear light, so the decoder tone maps them for an 8 bit display.
namespace ExrDecoder {

// true when the OpenEXR library was linked in
bool isAvailable();

bool isExrSuffix(const QString &suffix);

// Image size from the file header, without decoding pixels.
QSize size(const QString &path);

// Decodes the first RGBA layer. A valid targetSize asks for a downscaled result;
// the decoder samples the file instead of reading every pixel for it.
QImage read(const QString &path, const QSize &targetSize = QSize());

} // namespace ExrDecoder
