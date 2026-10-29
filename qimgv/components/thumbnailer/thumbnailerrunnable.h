#pragma once

#include <QRunnable>
#include <QThread>
#include <QCryptographicHash>
#include <ctime>
#include "sourcecontainers/thumbnail.h"
#include "components/cache/thumbnailcache.h"
#include "utils/imagefactory.h"
#include "utils/imagelib.h"
#include "settings.h"
#include <memory>
#include <QImageWriter>

class ThumbnailerRunnable : public QObject, public QRunnable {
    Q_OBJECT
public:
    ThumbnailerRunnable(ThumbnailCache* _cache, QString _path, int _size, bool _crop, bool _force);
    ~ThumbnailerRunnable();
    void run();
    static std::shared_ptr<Thumbnail> generate(ThumbnailCache *cache, QString path, int size, bool crop, bool force);
private:
    static QString generateIdString(QString path, int size, bool crop);

    // A generated thumbnail together with the size of the image it was made from.
    // The image is owned: these used to return a QImage*, so every failed decode
    // and every early return was a chance to forget a delete.
    struct ThumbnailImage {
        std::unique_ptr<QImage> image;
        QSize originalSize;
    };

    static ThumbnailImage createThumbnail(const QString &path, const char* format, int size, bool crop);
    static ThumbnailImage createVideoThumbnail(const QString &path, int size, bool crop);
#ifdef _WIN32
    // asks the Explorer shell for its cached thumbnail; much cheaper than
    // decoding the video ourselves
    static ThumbnailImage createShellThumbnail(const QString &path, int size, bool crop);
#endif
    QString path;
    int size;
    bool crop, force;
    ThumbnailCache* cache = nullptr;

signals:
    void taskStart(QString, int);
    void taskEnd(std::shared_ptr<Thumbnail>, QString);
};
