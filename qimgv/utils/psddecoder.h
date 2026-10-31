#pragma once

#include <QString>
#include <QSize>
#include <QImage>

// Photoshop documents (PSD/PSB), decoded without Photoshop: the merged composite
// image every PSD stores at the end of the file is read directly.
//
// Qt has no reader for PSD, and the third-party plugins are not available for
// this toolchain, so the format is parsed here. Layers are not decoded - the
// composite is what every viewer shows anyway.
//
// Supported: PSD (v1) and PSB (v2), 8 and 16 bit, RGB / grayscale / CMYK, raw,
// RLE and ZIP compressed image data, and the thumbnail Photoshop embeds in the
// image resources (used for folder view tiles).
namespace PsdDecoder {

// true when the file can be opened at all (i.e. it is a PSD/PSB)
bool isAvailable();

// true for the file suffixes this decoder handles ("psd", "psb")
bool isPsdSuffix(const QString &suffix);

// Size of the merged image. Invalid when the file cannot be read.
QSize size(const QString &path);

// Decodes the merged image. A valid targetSize asks for a downscaled result
// (the composite is decoded, then resized).
QImage read(const QString &path, const QSize &targetSize = QSize());

// The thumbnail Photoshop stores in the image resources (resource 1036, or the
// JPEG one in 1033). Empty when the file has none, or when it is smaller than
// minSize pixels wide - the caller then decodes the composite instead.
QImage readThumbnail(const QString &path, int minSize = 0);

} // namespace PsdDecoder
