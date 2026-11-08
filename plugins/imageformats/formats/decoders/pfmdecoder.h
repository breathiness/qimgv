#pragma once

#include <QImage>
#include <QSize>
#include <QString>

// PFM (Portable Float Map): a tiny header followed by 32 bit float samples, used
// by renderers as a dump of the linear frame buffer. Colour ("PF") and greyscale
// ("Pf") variants exist; the data is stored bottom up.
namespace PfmImage {

bool isPfmSuffix(const QString &suffix);

QSize size(const QString &path);

QImage read(const QString &path, const QSize &targetSize = QSize());

} // namespace PfmImage
