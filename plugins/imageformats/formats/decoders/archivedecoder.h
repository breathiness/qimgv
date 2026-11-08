#pragma once

#include <QImage>
#include <QSize>
#include <QString>

// Layered documents that store a flattened preview inside a zip: Krita (.kra)
// and OpenRaster (.ora). Only that preview is decoded; the layers themselves
// are not parsed, which matches how PSD files are handled here.
namespace ArchiveImageDecoder {

// true when zlib is available to read the deflated entries
bool isAvailable();

bool isArchiveSuffix(const QString &suffix);

// Size of the flattened image, read from the PNG header inside the archive
// without decoding it. Invalid when the document has no preview.
QSize size(const QString &path);

// The flattened image, downscaled to targetSize when one is given.
QImage read(const QString &path, const QSize &targetSize = QSize());

} // namespace ArchiveImageDecoder
