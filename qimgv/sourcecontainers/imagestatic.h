#pragma once

#include <QImage>
#include <QImageWriter>
#include <QSemaphore>
#include <QCryptographicHash>
#include <QMutex>
#include <QIcon>
#include <QSizeF>
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

    // pdf: the pages are rendered one below the other into a single tall image, so
    // scrolling reads the document like a long page. Page navigation then means
    // "scroll by one page".
    int pdfPageCount() const { return mPdfPageCount; }
    int pdfPage() const { return mPdfPage; }
    bool pdfIsStrip() const { return mPdfDocument && mPdfPageCount > 1; }
    int pdfPageHeight() const { return mPdfPageHeight; }
    int pdfRenderedPages() const { return mPdfRenderedPages; }
    // a protected document opens without pages until it is unlocked
    bool pdfNeedsPassword() const { return mPdfNeedsPassword; }
    bool unlockPdf(const QString &password);

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
    // pdf: pages are rendered by the optional provider plugin
    void loadPdf();
    std::shared_ptr<const QImage> renderPdfStrip();
    // renders the drawing; detailedSize receives the size the detailed pass would
    // have, which is what the preview claims to stand in for
    std::shared_ptr<const QImage> renderCad(bool detailed, QSize *detailedSize = nullptr) const;
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
    // pdf state: the open document, its page count and the page on screen
    std::shared_ptr<void> mPdfDocument;
    int mPdfPageCount = 0;
    bool mPdfNeedsPassword = false;
    // pixels of one page inside the strip, and how many pages the strip holds
    int mPdfPageHeight = 0;
    int mPdfRenderedPages = 0;
    int mPdfPage = 0;
    QSizeF mPdfPageSize;
    mutable QMutex mImageLock;
};
