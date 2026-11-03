#include "mpvframegrabber.h"

#include <QDebug>

#include <QElapsedTimer>
#include <QImage>

#include <cstdlib>
#include <cstring>
#include <mpv/client.h>

namespace {

// Waits for a specific event, ignoring everything else mpv reports meanwhile.
// Returns false on timeout or when playback ends first.
bool waitForEvent(mpv_handle *mpv, int wanted, int timeoutMs) {
    QElapsedTimer timer;
    timer.start();
    while(timer.elapsed() < timeoutMs) {
        mpv_event *event = mpv_wait_event(mpv, 0.05);
        if(!event)
            continue;
        if(event->event_id == MPV_EVENT_NONE)
            continue;
        if(event->event_id == wanted)
            return true;
        if(event->event_id == MPV_EVENT_END_FILE || event->event_id == MPV_EVENT_SHUTDOWN)
            return false;
    }
    return false;
}

bool mapInt(mpv_node *node, const char *key, qint64 *out) {
    if(!node || node->format != MPV_FORMAT_NODE_MAP || !node->u.list)
        return false;
    mpv_node_list *list = node->u.list;
    for(int i = 0; i < list->num; i++) {
        if(std::strcmp(list->keys[i], key) != 0)
            continue;
        if(list->values[i].format != MPV_FORMAT_INT64)
            return false;
        *out = list->values[i].u.int64;
        return true;
    }
    return false;
}

bool mapString(mpv_node *node, const char *key, QByteArray *out) {
    if(!node || node->format != MPV_FORMAT_NODE_MAP || !node->u.list)
        return false;
    mpv_node_list *list = node->u.list;
    for(int i = 0; i < list->num; i++) {
        if(std::strcmp(list->keys[i], key) != 0)
            continue;
        if(list->values[i].format != MPV_FORMAT_STRING)
            return false;
        *out = QByteArray(list->values[i].u.string);
        return true;
    }
    return false;
}

mpv_byte_array *mapBytes(mpv_node *node) {
    if(!node || node->format != MPV_FORMAT_NODE_MAP || !node->u.list)
        return nullptr;
    mpv_node_list *list = node->u.list;
    for(int i = 0; i < list->num; i++) {
        if(std::strcmp(list->keys[i], "data") != 0)
            continue;
        if(list->values[i].format != MPV_FORMAT_BYTE_ARRAY)
            return nullptr;
        return list->values[i].u.ba;
    }
    return nullptr;
}

// The frame mpv hands over as raw pixels; the layout depends on the platform and
// mpv reports it in the reply, so the copy below has to look at the format.
QImage imageFromScreenshot(mpv_node *node) {
    qint64 width = 0;
    qint64 height = 0;
    qint64 stride = 0;
    QByteArray format;
    if(!mapInt(node, "w", &width) || !mapInt(node, "h", &height) || !mapInt(node, "stride", &stride))
        return QImage();
    if(!mapString(node, "format", &format))
        return QImage();
    mpv_byte_array *bytes = mapBytes(node);
    if(!bytes || !bytes->data)
        return QImage();
    if(width <= 0 || height <= 0)
        return QImage();
    // QImage::Format_RGB32 keeps its bytes as B,G,R,A on little-endian, so a
    // "bgr*" source is copied as is and an "rgb*" source has red and blue swapped.
    const bool sourceIsRgb = format.startsWith("rgb");
    if(!sourceIsRgb && !format.startsWith("bgr"))
        return QImage();
    // "rgb0"/"bgr0" carry a padding byte per pixel, "rgb24"/"bgr24" do not
    const int bytesPerPixel = format.endsWith("0") ? 4 : 3;
    if(stride < width * bytesPerPixel)
        return QImage();

    QImage image(static_cast<int>(width), static_cast<int>(height), QImage::Format_RGB32);
    if(image.isNull())
        return QImage();
    const auto *source = static_cast<const uchar *>(bytes->data);
    for(qint64 y = 0; y < height; y++) {
        const uchar *src = source + y * stride;
        uchar *dst = image.scanLine(static_cast<int>(y));
        for(qint64 x = 0; x < width; x++, src += bytesPerPixel, dst += 4) {
            if(sourceIsRgb) {
                dst[0] = src[2];
                dst[1] = src[1];
                dst[2] = src[0];
            } else {
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
            }
            dst[3] = 0xff;
        }
    }
    return image;
}

}  // namespace

namespace MpvFrameGrabber {

// Decodes one frame without any video output and takes it from mpv itself. It used
// to go through mpv's image output, which writes with ffmpeg's image2 muxer: that
// muxer refuses a plain file name ("does not contain an image sequence pattern"),
// so every video silently lost its thumbnail.
bool grabFrame(const QString &videoPath, double positionSec, QImage *frame) {
    if(!frame)
        return false;
    mpv_handle *mpv = mpv_create();
    if(!mpv)
        return false;

    mpv_set_option_string(mpv, "config", "no");
    mpv_set_option_string(mpv, "load-scripts", "no");
    mpv_set_option_string(mpv, "terminal", "no");
    mpv_set_option_string(mpv, "audio", "no");
    mpv_set_option_string(mpv, "aid", "no");
    mpv_set_option_string(mpv, "sid", "no");
    mpv_set_option_string(mpv, "sub", "no");
    mpv_set_option_string(mpv, "vo", "null");
    mpv_set_option_string(mpv, "ao", "null");
    mpv_set_option_string(mpv, "pause", "yes");
    mpv_set_option_string(mpv, "hr-seek", "yes");
    if(positionSec > 0.0) {
        const QByteArray start = QByteArray::number(positionSec, 'f', 3);
        mpv_set_option_string(mpv, "start", start.constData());
    }

    const int initResult = mpv_initialize(mpv);
    if(initResult < 0) {
        qWarning() << "MpvFrameGrabber: mpv_initialize failed:" << mpv_error_string(initResult);
        mpv_destroy(mpv);
        return false;
    }

    const QByteArray input = videoPath.toUtf8();
    const char *command[] = {"loadfile", input.constData(), nullptr};
    const int loadResult = mpv_command(mpv, command);
    if(loadResult < 0) {
        qWarning() << "MpvFrameGrabber: loadfile failed on" << videoPath << "-" << mpv_error_string(loadResult);
        mpv_destroy(mpv);
        return false;
    }

    bool ok = false;
    // playback restarts once the seek finished and a frame is ready
    if(waitForEvent(mpv, MPV_EVENT_FILE_LOADED, 8000)
       && waitForEvent(mpv, MPV_EVENT_PLAYBACK_RESTART, 8000)) {
        const char *screenshot[] = {"screenshot-raw", "video", nullptr};
        if(mpv_command_async(mpv, 0, screenshot) >= 0) {
            QElapsedTimer timer;
            timer.start();
            while(timer.elapsed() < 8000) {
                mpv_event *event = mpv_wait_event(mpv, 0.05);
                if(!event || event->event_id != MPV_EVENT_COMMAND_REPLY)
                    continue;
                if(event->error < 0) {
                    qWarning() << "MpvFrameGrabber: screenshot failed:" << mpv_error_string(event->error);
                    break;
                }
                auto *reply = static_cast<mpv_event_command *>(event->data);
                const QImage image = imageFromScreenshot(&reply->result);
                if(!image.isNull()) {
                    *frame = image;
                    ok = true;
                }
                break;
            }
        }
    } else {
        qWarning() << "MpvFrameGrabber: no frame within 8s for" << videoPath;
    }
    mpv_destroy(mpv);
    return ok;
}

// The old, file based entry point; kept so a previously built application keeps
// working against this plugin.
bool grabFrameToFile(const QString &videoPath, double positionSec, const QString &outPath) {
    QImage frame;
    if(!grabFrame(videoPath, positionSec, &frame))
        return false;
    return frame.save(outPath, "png");
}

}  // namespace MpvFrameGrabber

// Pixel handover: the application gets a buffer it owns and frees with
// QimgvFreeVideoFrame. No temporary file is involved, so a blocked temp directory
// cannot cost every video its thumbnail.
extern "C" QIMGV_GRAB_EXPORT int QimgvGrabVideoFrameRaw(const char *videoPath, double positionSec,
                                                        unsigned char **pixels, int *width,
                                                        int *height, int *stride) {
    if(!videoPath || !pixels || !width || !height || !stride)
        return 0;
    QImage frame;
    if(!MpvFrameGrabber::grabFrame(QString::fromUtf8(videoPath), positionSec, &frame))
        return 0;
    const QImage argb = frame.convertToFormat(QImage::Format_RGB32);
    if(argb.isNull())
        return 0;
    const int rowBytes = argb.width() * 4;
    auto *buffer = static_cast<unsigned char *>(std::malloc(size_t(rowBytes) * size_t(argb.height())));
    if(!buffer)
        return 0;
    for(int y = 0; y < argb.height(); y++)
        std::memcpy(buffer + size_t(y) * size_t(rowBytes), argb.constScanLine(y), size_t(rowBytes));
    *pixels = buffer;
    *width = argb.width();
    *height = argb.height();
    *stride = rowBytes;
    return 1;
}

extern "C" QIMGV_GRAB_EXPORT void QimgvFreeVideoFrame(unsigned char *pixels) {
    std::free(pixels);
}

// The application resolves this symbol with QLibrary; keep it a plain C name so
// no name mangling or ABI detail gets in the way.
extern "C" QIMGV_GRAB_EXPORT bool QimgvGrabVideoFrame(const char *videoPath, double positionSec, const char *outPath)
{
    if(!videoPath || !outPath)
        return false;
    return MpvFrameGrabber::grabFrameToFile(QString::fromUtf8(videoPath), positionSec,
                                            QString::fromUtf8(outPath));
}
