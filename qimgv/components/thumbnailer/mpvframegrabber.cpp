#include "mpvframegrabber.h"

#include <QApplication>
#include <QLibrary>
#include <QStringList>

// Grabbing a frame needs libmpv, which the application does not link: it ships
// with the optional player plugin, so video support can be left out of an
// installation. The plugin exports a plain C function for this.
namespace {

typedef bool (*GrabVideoFrameFn)(const char *videoPath, double positionSec, const char *outPath);

GrabVideoFrameFn resolveGrabber()
{
    static GrabVideoFrameFn fn = nullptr;
    static bool tried = false;
    static QLibrary *library = nullptr;
    if(tried)
        return fn;
    tried = true;

    const QStringList names = {
        QApplication::applicationDirPath() + "/plugins/player_mpv",
        QApplication::applicationDirPath() + "/plugins/libplayer_mpv"
    };
    for(const QString &name : names) {
        library = new QLibrary(name);
        if(!library->load()) {
            delete library;
            library = nullptr;
            continue;
        }
        fn = reinterpret_cast<GrabVideoFrameFn>(library->resolve("QimgvGrabVideoFrame"));
        if(fn)
            return fn;
        // wrong plugin without the grabber: drop it and try the next name
        library->unload();
        delete library;
        library = nullptr;
    }
    return nullptr;
}

} // namespace

bool MpvFrameGrabber::grabFrame(const QString &videoPath, double positionSec, const QString &outPath)
{
    const GrabVideoFrameFn fn = resolveGrabber();
    if(!fn)
        return false;
    // paths go through as UTF-8, matching what the plugin expects
    return fn(videoPath.toUtf8().constData(), positionSec, outPath.toUtf8().constData());
}
