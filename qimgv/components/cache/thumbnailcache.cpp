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

bool ThumbnailCache::exists(QString id) {
    QString filePath = thumbnailPath(id);
    QFileInfo file(filePath);
    return file.exists() && file.isReadable();
}

void ThumbnailCache::saveThumbnail(QImage *image, QString id) {
    if(image) {
        QString filePath = thumbnailPath(id);
        // q88 is visually clean at thumbnail size and keeps the file small
        image->save(filePath, "JPEG", 88);
    }
}

QImage *ThumbnailCache::readThumbnail(QString id) {
    QString filePath = thumbnailPath(id);
    QFileInfo file(filePath);
    if(file.exists() && file.isReadable()) {
        QImage *thumb = new QImage();
        if(thumb->load(filePath)) {
            return thumb;
        } else {
            delete thumb;
            return nullptr;
        }
    } else {
        return nullptr;
    }
}
