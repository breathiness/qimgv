#pragma once

#include <QString>
#include <QImage>

// Video thumbnails without linking libmpv: the frame grabber lives in the
// optional player plugin, which exports a C function for it. When the pack is
// not installed, grabFrame() simply reports failure and video tiles stay empty.
namespace MpvFrameGrabber {
// Decodes one frame of the video at `positionSec` into `frame`.
bool grabFrame(const QString &videoPath, double positionSec, QImage *frame);
}
