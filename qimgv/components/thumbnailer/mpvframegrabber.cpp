#include "mpvframegrabber.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QLibrary>
#include <QStringList>
#include <cstring>

// Grabbing a frame needs libmpv, which the application does not link: it ships
// with the optional player plugin, so video support can be left out of an
// installation. The plugin exports plain C functions for this.
namespace {

typedef int (*GrabFrameRawFn)(const char *, double, unsigned char **, int *, int *, int *);
typedef void (*FreeFrameFn)(unsigned char *);
// entry point of older plugins: writes a png to a path
typedef bool (*GrabFrameToFileFn)(const char *, double, const char *);

struct Grabber {
    GrabFrameRawFn raw = nullptr;
    FreeFrameFn release = nullptr;
    GrabFrameToFileFn toFile = nullptr;
};

Grabber resolveGrabber()
{
    static Grabber grabber;
    static bool tried = false;
    if(tried)
        return grabber;
    tried = true;

    const QStringList names = {
        QApplication::applicationDirPath() + "/plugins/player_mpv",
        QApplication::applicationDirPath() + "/plugins/libplayer_mpv"
    };
    for(const QString &name : names) {
        auto *library = new QLibrary(name);
        if(!library->load()) {
            delete library;
            continue;
        }
        grabber.raw = reinterpret_cast<GrabFrameRawFn>(library->resolve("QimgvGrabVideoFrameRaw"));
        grabber.release = reinterpret_cast<FreeFrameFn>(library->resolve("QimgvFreeVideoFrame"));
        if(grabber.raw && grabber.release)
            return grabber;
        // an older plugin only has the file based call
        grabber.toFile = reinterpret_cast<GrabFrameToFileFn>(library->resolve("QimgvGrabVideoFrame"));
        if(grabber.toFile)
            return grabber;
        grabber = Grabber();
        library->unload();
        delete library;
    }
    return grabber;
}

} // namespace

bool MpvFrameGrabber::grabFrame(const QString &videoPath, double positionSec, QImage *frame)
{
    if(!frame)
        return false;
    const Grabber grabber = resolveGrabber();

    // paths go through as UTF-8, matching what the plugin expects
    const QByteArray path = videoPath.toUtf8();
    if(grabber.raw && grabber.release) {
        unsigned char *pixels = nullptr;
        int width = 0;
        int height = 0;
        int stride = 0;
        if(!grabber.raw(path.constData(), positionSec, &pixels, &width, &height, &stride) || !pixels)
            return false;
        // the plugin hands over RGB32 rows, stride included; copy row by row
        QImage image(width, height, QImage::Format_RGB32);
        if(image.isNull()) {
            grabber.release(pixels);
            return false;
        }
        const int rowBytes = qMin(stride, width * 4);
        for(int y = 0; y < height; y++)
            std::memcpy(image.scanLine(y), pixels + size_t(y) * size_t(stride), size_t(rowBytes));
        grabber.release(pixels);
        *frame = image;
        return true;
    }

    // old plugin: take the file it writes and read it back
    if(grabber.toFile) {
        const QString tmpPath = QDir::tempPath() + "/qimgv-grab-"
                                + QString::number(qHash(videoPath)) + ".png";
        if(!grabber.toFile(path.constData(), positionSec, tmpPath.toUtf8().constData()))
            return false;
        QImageReader reader(tmpPath, "png");
        const QImage image = reader.read();
        QFile::remove(tmpPath);
        if(image.isNull())
            return false;
        *frame = image;
        return true;
    }
    return false;
}
