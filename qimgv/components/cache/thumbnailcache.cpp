#include "thumbnailcache.h"

ThumbnailCache::ThumbnailCache() {
    cacheDirPath = settings->thumbnailCacheDir();
}

QString ThumbnailCache::thumbnailPath(QString id) {
    // Thumbnails are previews, so they are stored as JPEG: on 200 thumbnails at
    // 192px, PNG came to 33 KB each and JPEG q88 to 7 KB, and reading the
    // smaller files back is faster as well. The extension is part of the switch
    // on purpose: caches written before it stay unreachable instead of being
    // served as if they were current.
    return QString(cacheDirPath + id + ".jpg");
}

void ThumbnailCache::saveThumbnail(const QImage *image, QString id) {
    if(!image)
        return;
    const QString filePath = thumbnailPath(id);
    // Write beside the target and rename into place: a cache file that is only
    // half written when the app goes away would otherwise be loaded as a
    // thumbnail the next time. Saving can also fail (no space, read-only cache
    // directory), in which case nothing should be left behind.
    const QString tmpPath = filePath + ".part";
    if(!image->save(tmpPath, "JPEG", 88)) {
        QFile::remove(tmpPath);
        return;
    }
    QFile::remove(filePath);
    if(!QFile::rename(tmpPath, filePath))
        QFile::remove(tmpPath);
}

std::unique_ptr<QImage> ThumbnailCache::readThumbnail(QString id) {
    const QString filePath = thumbnailPath(id);
    const QFileInfo file(filePath);
    if(!file.exists() || !file.isReadable())
        return nullptr;
    auto thumb = std::make_unique<QImage>();
    if(!thumb->load(filePath))
        return nullptr;
    return thumb;
}
