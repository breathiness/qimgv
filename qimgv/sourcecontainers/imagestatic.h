#pragma once

#include <QImage>
#include <QImageWriter>
#include <QSemaphore>
#include <QCryptographicHash>
#include <QMutex>
#include <QIcon>
#include "image.h"
#include "utils/imagelib.h"
#include <settings.h>

class ImageStatic : public Image {
public:
    // previewTarget: when valid, only a downscaled copy is decoded here and
    // the caller is expected to call loadFull() afterwards. Empty means the
    // whole file is decoded right away, like before.
    ImageStatic(QString _path, QSize previewTarget = QSize());
    ImageStatic(std::unique_ptr<DocumentInfo> _info, QSize previewTarget = QSize());
    ~ImageStatic();

    std::unique_ptr<QPixmap> getPixmap();
    std::shared_ptr<const QImage> getSourceImage();
    std::shared_ptr<const QImage> getImage();
    std::shared_ptr<const QImage> getPreviewImage();

    bool isPreviewOnly() const;
    bool loadFull();

    int height();
    int width();
    QSize size();

    bool setEditedImage(std::unique_ptr<const QImage> imageEditedNew);
    bool discardEditedImage();

    // Size at which a preview pass is worth doing, or an invalid QSize when the
    // file should be decoded in one go (small file, or a format where Qt
    // decodes the whole thing before scaling anyway).
    static QSize suggestPreviewTarget(QString path);
    // approximate size of the viewer in device pixels; a preview must be at
    // least this large or it would immediately trigger a full resolution load
    static void setPreviewSizeHint(QSize devicePixels);

public slots:
    void crop(QRect newRect);
    bool save();
    bool save(QString destPath);

private:
    void load();
    void loadGeneric();
    void loadICO();
    // decodes the file, optionally asking Qt for a downscaled copy
    std::unique_ptr<const QImage> readImage(QSize scaledTarget);
    std::shared_ptr<const QImage> currentImage() const;
    void setSourceSize(QSize fullSizeFromHeader);
    QString generateHash(QString str);

    std::shared_ptr<const QImage> image, imageEdited;
    // original (exif rotated) dimensions; valid even while only a preview is held
    QSize mSourceSize;
    bool mFullLoaded = false;
    bool mFullLoading = false;
    QSize mPreviewTarget;
    mutable QMutex mImageLock;
};
