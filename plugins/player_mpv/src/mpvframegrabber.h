#pragma once

#include <QString>
#include <QtGlobal>

#if defined QIMGV_PLAYER_MPV_LIBRARY
    #define QIMGV_GRAB_EXPORT Q_DECL_EXPORT
#else
    #define QIMGV_GRAB_EXPORT Q_DECL_IMPORT
#endif

// Renders a single video frame with libmpv, in-process. The player plugin is not
// involved: this creates its own mpv instance, so it is safe to call from the
// thumbnailer threads and needs no external mpv executable.
namespace MpvFrameGrabber {
// Writes one frame of the video at `positionSec` to `outPath` (png).
// Returns true when a file was written.
bool grabFrame(const QString &videoPath, double positionSec, const QString &outPath);
}

// Exported so the executable can grab video thumbnails without linking libmpv:
// video support lives in this plugin and can be left out of an installation.
extern "C" QIMGV_GRAB_EXPORT bool QimgvGrabVideoFrame(const char *videoPath, double positionSec, const char *outPath);

