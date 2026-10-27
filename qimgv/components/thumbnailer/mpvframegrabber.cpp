#include "mpvframegrabber.h"

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>

#include <mpv/client.h>

namespace {

// Runs the mpv event loop until playback of the single requested frame ends.
// The image video output sends END_FILE once it has written the file.
bool waitForEnd(mpv_handle *mpv, int timeoutMs) {
    QElapsedTimer timer;
    timer.start();
    while(timer.elapsed() < timeoutMs) {
        mpv_event *event = mpv_wait_event(mpv, 0.1);
        if(event->event_id == MPV_EVENT_END_FILE || event->event_id == MPV_EVENT_SHUTDOWN)
            return true;
    }
    return false;
}

}

bool MpvFrameGrabber::grabFrame(const QString &videoPath, double positionSec, const QString &outPath) {
    mpv_handle *mpv = mpv_create();
    if(!mpv)
        return false;

    // Same behaviour as `mpv --frames=1 --o=<file>`, without the process:
    // the image output writes exactly one frame and then reaches end of file.
    // Paths go to mpv as UTF-8: QFile::encodeName() would use the local 8-bit
    // code page on Windows, so any non-ASCII file name would not be found.
    const QByteArray output = outPath.toUtf8();
    mpv_set_option_string(mpv, "config", "no");
    mpv_set_option_string(mpv, "load-scripts", "no");
    mpv_set_option_string(mpv, "terminal", "no");
    mpv_set_option_string(mpv, "audio", "no");
    mpv_set_option_string(mpv, "aid", "no");
    mpv_set_option_string(mpv, "sid", "no");
    mpv_set_option_string(mpv, "sub", "no");
    mpv_set_option_string(mpv, "vo", "image");
    mpv_set_option_string(mpv, "o", output.constData());
    mpv_set_option_string(mpv, "frames", "1");
    if(positionSec > 0.0) {
        const QByteArray start = QByteArray::number(positionSec, 'f', 3);
        mpv_set_option_string(mpv, "start", start.constData());
    }

    if(mpv_initialize(mpv) < 0) {
        mpv_destroy(mpv);
        return false;
    }

    const QByteArray input = videoPath.toUtf8();
    const char *command[] = {"loadfile", input.constData(), nullptr};
    const bool started = mpv_command(mpv, command) >= 0;
    const bool finished = started && waitForEnd(mpv, 8000);
    mpv_destroy(mpv);

    const QFileInfo written(outPath);
    return finished && written.exists() && written.size() > 0;
}
