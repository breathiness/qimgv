#pragma once

#include <QImage>
#include <QImageWriter>
#include <QSemaphore>
#include <QCryptographicHash>
#include <QMutex>
#include <QIcon>
#include <QSizeF>
#include "image.h"

#include <memory>

class Model3D;
class PdfPageModel;
class ZipArchive;
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

    // pdf: pages are drawn by the paged view, which gets them from this model; the
    // image itself carries no pixels
    std::shared_ptr<PdfPageModel> pdfModel() const { return mPdfModel; }
    // a protected document opens without pages until it is unlocked
    bool pdfNeedsPassword() const { return mPdfNeedsPassword; }
    bool unlockPdf(const QString &password);

    // Multi page images (TIFF) keep every page in the file and decode one at a
    // time. A single page file reports 1/0 and setPage() does nothing, so the
    // ordinary browsing path is unchanged.
    int pageCount() const { return mPageCount; }
    int currentPage() const { return mCurrentPage; }
    bool setPage(int page);

    // comic archive: pages are images inside the zip; the page cursor works like
    // the one a multi page tiff has
    bool isComicFile() const;

    // 3D models: rotate by a mouse delta and repaint. The interactive render is
    // small; finish3dRotation() repaints at full size when the drag ends.
    bool is3dFile() const;
    bool rotate3d(float deltaYaw, float deltaPitch);
    bool finish3dRotation();

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
    // renders a DXF/DWG through the CAD code; a smaller preview first, the detailed
    // one when the viewer asks for full resolution
    void loadCad();
    // comic archive (CBZ): the pages are decoded one at a time
    void loadComic();
    std::shared_ptr<const QImage> readComicPage(int page) const;
    // 3D models: rendered into a square image by the model renderer
    void load3d();
    std::shared_ptr<const QImage> render3d(bool detailed, QSize *detailedSize = nullptr) const;
    // pdf: pages are rendered by the optional provider plugin
    void loadPdf();
    std::shared_ptr<const QImage> renderPdfStrip();
    // renders the drawing; detailedSize receives the size the detailed pass would
    // have, which is what the preview claims to stand in for
    std::shared_ptr<const QImage> renderCad(bool detailed, QSize *detailedSize = nullptr) const;
    // decodes the file, optionally asking Qt for a downscaled copy
    std::unique_ptr<const QImage> readImage(QSize scaledTarget, int page = -1);
    // counts the pages of a page-capable format; only TIFF is asked
    void detectPageCount();
    std::shared_ptr<const QImage> currentImage() const;
    void setSourceSize(QSize fullSizeFromHeader);
    QString generateHash(QString str);

    std::shared_ptr<const QImage> image, imageEdited;
    // original (exif rotated) dimensions; valid even while only a preview is held
    QSize mSourceSize;
    bool mFullLoaded = false;
    bool mFullLoading = false;
    QSize mPreviewTarget;
    // pdf state: the open document, its page count and the page on screen
    std::shared_ptr<PdfPageModel> mPdfModel;
    bool mPdfNeedsPassword = false;
    // multi page image state (TIFF); 1/0 for everything else
    int mPageCount = 1;
    int mCurrentPage = 0;
    // comic state: entry names and the open archive, so a page turn is one read
    QStringList mComicPages;
    mutable std::shared_ptr<ZipArchive> mComicArchive;
    // 3D state: the parsed model is kept so rotating does not re-read the file
    mutable std::shared_ptr<Model3D> mModel3d;
    float mModelYaw = 0.65f;
    float mModelPitch = 0.45f;
    mutable QMutex mImageLock;
};
