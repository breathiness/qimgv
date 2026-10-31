#pragma once

#include <QString>

// Video thumbnails without linking libmpv: the frame grabber lives in the
// optional player plugin, which exports a C function for it. When the pack is
// not installed, grabFrame() simply reports failure and video tiles stay empty.
namespace MpvFrameGrabber {
// Writes one frame of the video at `positionSec` to `outPath` (png).
// Returns true when a file was written.
bool grabFrame(const QString &videoPath, double positionSec, const QString &outPath);
}
