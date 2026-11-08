#include "imagestatic.h"
#include "3d/model3d.h"
#include "cad/caddrawing.h"
#include "pdf/pdfpagemodel.h"
#include <QElapsedTimer>
#include <time.h>

// Two-pass loading policy: a preview pass only pays off for files that are big
// enough to be noticeable, and only for formats Qt can decode at a reduced
// scale without decoding everything first (libjpeg's DCT scaling). For anything
// else QImageReader decodes the whole image and scales it afterwards, which is
// strictly worse than decoding once at full size.
static const qint64 PREVIEW_MIN_PIXELS = 8000000; // ~8 MP
static const int PREVIEW_MAX_DIVISOR = 8;

// the viewer tells us how big it is; a preview smaller than the window would be
// upscaled on screen and would immediately ask for the full resolution image
static QSize g_previewSizeHint(2560, 1440);

void ImageStatic::setPreviewSizeHint(QSize devicePixels) {
    if(!devicePixels.isEmpty())
        g_previewSizeHint = devicePixels;
}

ImageStatic::ImageStatic(QString _path, QSize previewTarget)
    : Image(_path), mPreviewTarget(previewTarget)
{
    load();
}

ImageStatic::ImageStatic(std::unique_ptr<DocumentInfo> _info, QSize previewTarget)
    : Image(std::move(_info)), mPreviewTarget(previewTarget)
{
    load();
}

ImageStatic::~ImageStatic() {
}

QSize ImageStatic::suggestPreviewTarget(QString path) {
    if(!settings->twoPassLoading())
        return QSize();
    QImageReader r(path);
    if(!r.canRead())
        return QSize();
    const QByteArray format = r.format();
    if(format != "jpeg" && format != "jpg")
        return QSize();
    const QSize full = r.size();
    if(full.isEmpty())
        return QSize();
    if((qint64) full.width() * full.height() < PREVIEW_MIN_PIXELS)
        return QSize();
    const int longSide = qMax(full.width(), full.height());
    const int needed = qMax(g_previewSizeHint.width(), g_previewSizeHint.height());
    // biggest power of two divisor that still keeps the preview at least as
    // large as the window; powers of two keep libjpeg on its fast scaling path
    int divisor = 1;
    while(divisor < PREVIEW_MAX_DIVISOR && longSide / (divisor * 2) >= needed)
        divisor *= 2;
    if(divisor == 1)
        return QSize();
    return QSize(qMax(1, full.width() / divisor), qMax(1, full.height() / divisor));
}

//load image data from disk
void ImageStatic::load() {
    if(isLoaded()) {
        return;
    }
    if(mDocInfo->isCad()) {
        loadCad();
        return;
    }
    if(mDocInfo->isPdf()) {
        loadPdf();
        return;
    }
    if(mDocInfo->is3d()) {
        load3d();
        return;
    }
    if(mDocInfo->mimeType().name() == "image/vnd.microsoft.icon") {
        loadICO();
        return;
    }
    // Only formats that can hold several images are asked; for everything else
    // this is a no-op and the loading path below is exactly what it was.
    detectPageCount();
    if(mPreviewTarget.isEmpty()) {
        loadGeneric();
        return;
    }
    // the header tells us the real size before anything is decoded
    QImageReader header(mPath, mDocInfo->format().toStdString().c_str());
    if(!header.canRead() || header.size().isEmpty()) {
        loadGeneric();
        return;
    }
    setSourceSize(header.size());
    std::shared_ptr<const QImage> preview = readImage(mPreviewTarget);
    if(!preview) {
        // let the regular path deal with (and report) a broken file
        loadGeneric();
        return;
    }
    {
        QMutexLocker lock(&mImageLock);
        image = preview;
        mFullLoaded = false;
    }
    mLoaded = true;
}

// decodes the rest of the image and swaps it in, keeping the same object
bool ImageStatic::loadFull() {
    {
        QMutexLocker lock(&mImageLock);
        if(mFullLoaded || mFullLoading) // already done, or someone is on it
            return false;
        mFullLoading = true;
    }
    std::shared_ptr<const QImage> full;
    if(mDocInfo->isCad())
        full = renderCad(true);
    else if(mDocInfo->is3d())
        full = render3d(true);
    else
        full = readImage(QSize());
    QMutexLocker lock(&mImageLock);
    mFullLoading = false;
    if(!full)
        return false;
    if(mFullLoaded)
        return false;
    image = full;
    mFullLoaded = true;
    mSourceSize = full->size();
    mLoaded = true;
    return true;
}

bool ImageStatic::isPreviewOnly() const {
    QMutexLocker lock(&mImageLock);
    return mLoaded && !mFullLoaded;
}

// A drawing is rendered instead of decoded. The first pass is sized to the window
// so it appears instantly; the detailed pass renders much larger and is requested
// by the viewer through loadFull() once the image is zoomed in.
void ImageStatic::loadCad() {
    QSize detailedSize;
    std::shared_ptr<const QImage> preview = renderCad(false, &detailedSize);
    if(!preview) {
        mLoaded = true;
        return;
    }
    {
        QMutexLocker lock(&mImageLock);
        image = preview;
        mFullLoaded = false;
    }
    // claim the area the detailed render will cover, so the viewer asks for it
    // once the image is zoomed past the preview's own resolution
    mSourceSize = detailedSize.isValid() ? detailedSize : preview->size();
    mLoaded = true;
}

std::shared_ptr<const QImage> ImageStatic::renderCad(bool detailed, QSize *detailedSize) const {
    CadDrawing drawing;
    if(!drawing.load(mPath))
        return nullptr;
    const QRectF bounds = drawing.bounds();
    if(bounds.width() <= 0 || bounds.height() <= 0)
        return nullptr;

    // the detailed pass is what the preview stands in for: its size tells the
    // viewer when to ask for more pixels
    const double aspectForSize = bounds.width() / bounds.height();
    const double pixelsPerLongSideSquared = aspectForSize >= 1.0 ? 1.0 / aspectForSize : aspectForSize;
    const qint64 budgetForSize = qint64(settings->memoryAllocationLimit()) * 1024 * 1024;
    const double maxDetailedSide = std::sqrt(double(budgetForSize) / 4.0
                                             / std::max(1e-6, pixelsPerLongSideSquared));
    const int detailedLongSide = int(qBound(1200.0, std::min(6000.0, maxDetailedSide), 6000.0));
    if(detailedSize) {
        const double detailedScale = double(detailedLongSide) / qMax(bounds.width(), bounds.height());
        *detailedSize = QSize(qMax(16, int(bounds.width() * detailedScale)),
                              qMax(16, int(bounds.height() * detailedScale)));
    }

    int longSide = 0;
    if(detailed) {
        // the detailed pass has to stay sharp while zooming, so it aims at a much
        // larger canvas than the window; the memory budget caps it for big sheets
        longSide = detailedLongSide;
    } else {
        const QSize hint = g_previewSizeHint.isValid() ? g_previewSizeHint : QSize(2560, 1440);
        longSide = qBound(900, qMax(hint.width(), hint.height()), 2600);
    }

    const double scale = double(longSide) / qMax(bounds.width(), bounds.height());
    const int width = qMax(16, int(bounds.width() * scale));
    const int height = qMax(16, int(bounds.height() * scale));
    const QImage rendered = drawing.render(width, height);
    if(rendered.isNull())
        return nullptr;
    return std::make_shared<const QImage>(rendered);
}

// A pdf carries no pixels of its own: the pages are drawn by PdfView, which asks
// this model for the ones on screen. Loading therefore only opens the document and
// records how many pages there are, which is what makes a 351 page book open in the
// same instant as a one page letter.
void ImageStatic::loadPdf() {
    mPdfModel = std::make_shared<PdfPageModel>();
    QString error;
    if(!mPdfModel->open(mPath, &error)) {
        mPdfNeedsPassword = mPdfModel->isEncrypted();
        mPdfModel.reset();
        mLoaded = true;
        return;
    }
    mPdfNeedsPassword = mPdfModel->isEncrypted();
    // a page is what this document shows, so that is the size worth reporting
    const QSizeF pageSize = mPdfModel->maxPageSize();
    mSourceSize = QSize(qRound(pageSize.width()), qRound(pageSize.height()));
    mLoaded = true;
}

// Unlocks a protected document. The view then re-reads the page list from the model.
bool ImageStatic::unlockPdf(const QString &password) {
    if(!mPdfModel || !mPdfNeedsPassword)
        return false;
    if(!mPdfModel->unlock(password))
        return false;
    mPdfNeedsPassword = false;
    if(mPdfModel->pageCount() <= 0)
        return false;
    // While the document was locked there was no page to measure, so the size
    // recorded by loadPdf() is (-1, -1). The window title reads it from here;
    // without this the unlocked document kept reporting "-1 x -1" even though
    // the pages rendered fine.
    const QSizeF pageSize = mPdfModel->maxPageSize();
    mSourceSize = QSize(qRound(pageSize.width()), qRound(pageSize.height()));
    return true;
}

void ImageStatic::loadGeneric() {
    /* QImageReader::read() seems more reliable than just reading via QImage.
     * For example: "Invalid JPEG file structure: two SOF markers"
     * QImageReader::read() returns false, but still reads an image. Meanwhile QImage just fails.
     *
     * I havent checked qimage's code, but it seems like it sees an exception
     * from libjpeg or whatever and just gives up on reading the file.
     *
     * tldr: qimage bad
     */
    std::shared_ptr<const QImage> img = readImage(QSize());
    {
        QMutexLocker lock(&mImageLock);
        image = img;
        mFullLoaded = true;
    }
    if(img)
        mSourceSize = img->size();
    mLoaded = true;
}

// decodes the file, optionally asking Qt for a downscaled copy
std::unique_ptr<const QImage> ImageStatic::readImage(QSize scaledTarget, int page) {
    QImageReader r(mPath, mDocInfo->format().toStdString().c_str());
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    r.setAllocationLimit(settings->memoryAllocationLimit());
#endif
    // page 0 is what a reader shows by default, so the single page path does not
    // even touch the seek machinery
    if(page > 0 && !r.jumpToImage(page))
        return nullptr;
    if(!scaledTarget.isEmpty()) {
        const QSize full = r.size();
        if(!full.isEmpty() && full.width() > scaledTarget.width())
            r.setScaledSize(full.scaled(scaledTarget, Qt::KeepAspectRatio));
    }
    std::unique_ptr<QImage> tmp(new QImage());
    if(!r.read(tmp.get()) || tmp->isNull()) {
        // HEIC/JPEG XL/camera raw/PSD come from the optional image format pack
        // (see plugins/imageformats/formats); without it they simply fail
        return nullptr;
    }
    std::unique_ptr<const QImage> img(std::move(tmp));
    img = ImageLib::exifRotated(std::move(img), mDocInfo.get()->exifOrientation());
    // scaling this format via qt results in transparent background
    // it rare enough so lets just convert it to the closest working thing
    if(img->format() == QImage::Format_Mono)
        img.reset(new QImage(img->convertToFormat(QImage::Format_Grayscale8)));
    return img;
}

// dimensions of the file itself, before any downscaling; 90/270 degree rotations
// swap the axes, matching what ImageLib::exifRotated() does
void ImageStatic::setSourceSize(QSize fullSizeFromHeader) {
    const int orientation = mDocInfo.get()->exifOrientation();
    if(orientation >= 4 && orientation <= 7)
        fullSizeFromHeader = fullSizeFromHeader.transposed();
    mSourceSize = fullSizeFromHeader;
}

// Only TIFF (in practice) stores several images per file. Opening a reader just
// for that costs a header read; a one page file leaves mPageCount at 1 and is
// treated exactly like any other image.
void ImageStatic::detectPageCount() {
    const QString format = mDocInfo->format().toLower();
    if(format != QLatin1String("tiff") && format != QLatin1String("tif"))
        return;
    QImageReader reader(mPath, mDocInfo->format().toStdString().c_str());
    if(!reader.canRead())
        return;
    const int count = reader.imageCount();
    if(count > 1)
        mPageCount = count;
}

// Switches to another page of the same file. Decoding stays on demand: only the
// page that ends up on screen is read, and the previous one is dropped.
bool ImageStatic::is3dFile() const {
    return mDocInfo && mDocInfo->is3d();
}

// Rotates the model by a mouse drag and repaints at interactive size. The pixels
// are swapped in place, so the viewer keeps its zoom while the user turns the
// model around.
bool ImageStatic::rotate3d(float deltaYaw, float deltaPitch) {
    if(!mDocInfo || !mDocInfo->is3d())
        return false;
    if(!mModel3d) {
        auto model = std::make_shared<Model3D>();
        if(!model->load(mPath))
            return false;
        mModel3d = model;
    }
    constexpr float maxPitch = 1.55f;   // just short of straight up/down
    mModelYaw += deltaYaw;
    mModelPitch = qBound(-maxPitch, mModelPitch + deltaPitch, maxPitch);

    Model3D::View view;
    view.yaw = mModelYaw;
    view.pitch = mModelPitch;
    const QImage rendered = mModel3d->render(640, 640, view);
    if(rendered.isNull())
        return false;
    {
        QMutexLocker lock(&mImageLock);
        image = std::make_shared<const QImage>(rendered);
        mFullLoaded = false;
    }
    return true;
}

// Repaints at full size once the drag is over.
bool ImageStatic::finish3dRotation() {
    if(!mModel3d)
        return false;
    Model3D::View view;
    view.yaw = mModelYaw;
    view.pitch = mModelPitch;
    const QImage rendered = mModel3d->render(2048, 2048, view);
    if(rendered.isNull())
        return false;
    {
        QMutexLocker lock(&mImageLock);
        image = std::make_shared<const QImage>(rendered);
        mFullLoaded = true;
    }
    mSourceSize = QSize(2048, 2048);
    return true;
}

void ImageStatic::load3d() {
    QSize detailedSize;
    std::shared_ptr<const QImage> preview = render3d(false, &detailedSize);
    if(!preview) {
        mLoaded = true;
        return;
    }
    {
        QMutexLocker lock(&mImageLock);
        image = preview;
        mFullLoaded = false;
    }
    // claim the area the detailed render will cover, so the viewer asks for it
    // once the image is zoomed past the preview's own resolution
    mSourceSize = detailedSize.isValid() ? detailedSize : preview->size();
    mLoaded = true;
}

std::shared_ptr<const QImage> ImageStatic::render3d(bool detailed, QSize *detailedSize) const {
    if(!mModel3d) {
        auto model = std::make_shared<Model3D>();
        if(!model->load(mPath))
            return nullptr;
        mModel3d = model;
    }

    // a square canvas keeps the model centred whatever the window looks like
    const int detailedSide = 2048;
    int side = detailedSide;
    if(!detailed) {
        if(mPreviewTarget.isValid()) {
            // the thumbnailer asks for a small size; honour it instead of
            // rendering a full preview only to shrink it
            side = qBound(256, qMax(mPreviewTarget.width(), mPreviewTarget.height()), 1280);
        } else {
            const QSize hint = g_previewSizeHint.isValid() ? g_previewSizeHint : QSize(2560, 1440);
            side = qBound(512, qMax(hint.width(), hint.height()), 1280);
        }
    }
    Model3D::View view;
    view.yaw = mModelYaw;
    view.pitch = mModelPitch;
    const QImage rendered = mModel3d->render(side, side, view);
    if(rendered.isNull())
        return nullptr;
    if(detailedSize)
        *detailedSize = QSize(detailedSide, detailedSide);
    return std::make_shared<const QImage>(rendered);
}

bool ImageStatic::setPage(int page) {
    if(mPageCount <= 1)
        return false;
    page = qBound(0, page, mPageCount - 1);
    if(page == mCurrentPage)
        return false;
    std::shared_ptr<const QImage> next(readImage(QSize(), page));
    if(!next)
        return false;
    {
        QMutexLocker lock(&mImageLock);
        image = next;
        mFullLoaded = true;
    }
    mCurrentPage = page;
    mSourceSize = next->size();
    mLoaded = true;
    return true;
}

std::shared_ptr<const QImage> ImageStatic::currentImage() const {
    QMutexLocker lock(&mImageLock);
    return image;
}

// TODO: move this out somewhere to use in other places
void ImageStatic::loadICO() {
    // Big brain code. It's mostly for small ico files so whatever. I'm not patching Qt for this.
    QIcon icon(mPath);
    QList<QSize> sizes = icon.availableSizes();
    QSize maxSize(0, 0);
    for(auto sz : sizes)
        if(maxSize.width() < sz.width())
            maxSize = sz;
    QPixmap iconPix = icon.pixmap(maxSize);
    std::unique_ptr<const QImage> img(new QImage(iconPix.toImage()));
    {
        QMutexLocker lock(&mImageLock);
        image = std::move(img);
        mFullLoaded = true;
    }
    mSourceSize = currentImage()->size();
    mLoaded = true;
}

QString ImageStatic::generateHash(QString str) {
    return QString(QCryptographicHash::hash(str.toUtf8(), QCryptographicHash::Md5).toHex());
}

// TODO: move saving to directorymodel
bool ImageStatic::save(QString destPath) {
    // never write a preview back over the original file
    if(isPreviewOnly())
        loadFull();
    QString tmpPath = destPath + "_" + generateHash(destPath);
    QFileInfo fi(destPath);
    QString ext = fi.suffix();
    // png compression note from libpng
    // Note that tests have shown that zlib compression levels 3-6 usually perform as well
    // as level 9 for PNG images, and do considerably fewer caclulations
    int quality = 95;
    if(ext.compare("png", Qt::CaseInsensitive) == 0)
        quality = 30;
    else if(ext.compare("jpg", Qt::CaseInsensitive) == 0 || ext.compare("jpeg", Qt::CaseInsensitive) == 0)
        quality = settings->JPEGSaveQuality();

    bool backupExists = false, success = false, originalExists = false;

    if(QFile::exists(destPath))
        originalExists = true;

    // backup the original file if possible
    if(originalExists) {
        QFile::remove(tmpPath);
        if(!QFile::copy(destPath, tmpPath)) {
            qDebug() << "ImageStatic::save() - Could not create file backup.";
            return false;
        }
        backupExists = true;
    }
    // save file
    std::shared_ptr<const QImage> src = currentImage();
    if(isEdited()) {
        success = imageEdited->save(destPath, ext.toStdString().c_str(), quality);
        image.swap(imageEdited);
        discardEditedImage();
        src = currentImage();
        if(src)
            mSourceSize = src->size();
    } else if(src) {
        success = src->save(destPath, ext.toStdString().c_str(), quality);
    }
    if(backupExists) {
        // everything ok - remove the backup
        if(success) {
            QFile file(tmpPath);
            file.remove();
        } else if(originalExists) {
            // revert on fail: restore the destination from the backup taken of
            // it above. destPath is not necessarily the file being displayed
            // ("save as"), and the source must never be touched in that case.
            QFile::remove(destPath);
            QFile::copy(tmpPath, destPath);
            QFile::remove(tmpPath);
        }
    }
    if(destPath == mPath && success)
        mDocInfo->refresh();
    return success;
}

bool ImageStatic::save() {
    return save(mPath);
}

std::unique_ptr<QPixmap> ImageStatic::getPixmap() {
    std::unique_ptr<QPixmap> pix(new QPixmap());
    if(isEdited()) {
        pix->convertFromImage(*imageEdited);
    } else {
        std::shared_ptr<const QImage> src = currentImage();
        if(src) {
            pix->convertFromImage(*src, Qt::NoFormatConversion);
            // a preview stands in for the full image: let it claim the area the
            // full resolution version would cover, so that nothing above has to
            // know that it is looking at a downscaled copy
            if(isPreviewOnly() && !mSourceSize.isEmpty() && pix->width() > 0) {
                const qreal ratio = (qreal) pix->width() / (qreal) mSourceSize.width();
                if(ratio > 0.0 && ratio < 1.0)
                    pix->setDevicePixelRatio(ratio);
            }
        }
    }
    return pix;
}

std::shared_ptr<const QImage> ImageStatic::getSourceImage() {
    return currentImage();
}

std::shared_ptr<const QImage> ImageStatic::getImage() {
    // callers that end up here need real pixels: editing, saving, clipboard,
    // printing. Make sure the preview has been replaced by the full image.
    if(isPreviewOnly())
        loadFull();
    return isEdited() ? imageEdited : currentImage();
}

std::shared_ptr<const QImage> ImageStatic::getPreviewImage() {
    return isEdited() ? imageEdited : currentImage();
}

int ImageStatic::height() {
    return isEdited() ? imageEdited->height() : mSourceSize.height();
}

int ImageStatic::width() {
    return isEdited() ? imageEdited->width() : mSourceSize.width();
}

QSize ImageStatic::size() {
    return isEdited() ? imageEdited->size() : mSourceSize;
}

bool ImageStatic::setEditedImage(std::unique_ptr<const QImage> imageEditedNew) {
    if(imageEditedNew && imageEditedNew->width() != 0) {
        discardEditedImage();
        imageEdited = std::move(imageEditedNew);
        mEdited = true;
        return true;
    }
    return false;
}

bool ImageStatic::discardEditedImage() {
    if(imageEdited) {
        imageEdited.reset();
        mEdited = false;
        return true;
    }
    return false;
}
