#pragma once

#include <QImage>
#include <QSize>
#include <QString>

// QOI ("Quite OK Image"), a small lossless format: a 14 byte header and a
// handful of run-length/difference ops. No library involved.
namespace QoiImage {

bool isQoiSuffix(const QString &suffix);

QSize size(const QString &path);

QImage read(const QString &path, const QSize &targetSize = QSize());

} // namespace QoiImage
