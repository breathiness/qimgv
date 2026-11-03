#pragma once

#include <QString>
#include <QImage>
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
// Decodes one frame of the video at `positionSec`.
bool grabFrame(const QString &videoPath, double positionSec, QImage *frame);
// Writes one frame to `outPath` (png); the file based entry point of older builds.
bool grabFrameToFile(const QString &videoPath, double positionSec, const QString &outPath);
}

// Exported so the executable can grab video thumbnails without linking libmpv:
// video support lives in this plugin and can be left out of an installation.
extern "C" QIMGV_GRAB_EXPORT bool QimgvGrabVideoFrame(const char *videoPath, double positionSec, const char *outPath);
// Fills a freshly allocated RGB32 buffer; release it with QimgvFreeVideoFrame.
// Returns 1 on success.
extern "C" QIMGV_GRAB_EXPORT int QimgvGrabVideoFrameRaw(const char *videoPath, double positionSec,
                                                        unsigned char **pixels, int *width,
                                                        int *height, int *stride);
extern "C" QIMGV_GRAB_EXPORT void QimgvFreeVideoFrame(unsigned char *pixels);

