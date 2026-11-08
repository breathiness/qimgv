#pragma once

#include <QImage>
#include <QSize>
#include <QString>
#include <QVector>

// Windows animated cursors (.ani): a RIFF container holding the frames as .ico
// images, plus the timing. They are small, so every frame is decoded at once and
// the plugin plays them like any other animation.
namespace AniImage {

struct Frame {
    QImage image;
    int delayMs = 0;
};

bool isAniSuffix(const QString &suffix);

// Size of the first frame, or an invalid size when the file cannot be parsed.
QSize size(const QString &path);

// Decodes the whole animation. False when the file is not a readable .ani.
bool readAnimation(const QString &path, QVector<Frame> *frames);

} // namespace AniImage
