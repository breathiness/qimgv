#pragma once

#include <QString>

// Renders a single video frame with libmpv, in-process. The player plugin is not
// involved: this creates its own mpv instance, so it is safe to call from the
// thumbnailer threads and needs no external mpv executable.
namespace MpvFrameGrabber {
// Writes one frame of the video at `positionSec` to `outPath` (png).
// Returns true when a file was written.
bool grabFrame(const QString &videoPath, double positionSec, const QString &outPath);
}
