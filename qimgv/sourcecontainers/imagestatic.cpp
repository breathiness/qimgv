#include "imagestatic.h"
#include "cad/caddrawing.h"
#include "utils/pdfprovider.h"
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
    if(mDocInfo->mimeType().name() == "image/vnd.microsoft.icon") {
        loadICO();
        return;
    }
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

// A pdf is drawn as one tall image: the pages follow each other top to bottom, so
// scrolling reads the document instead of paging through it. The width and the
// number of pages are chosen so the image stays inside the memory budget - a 350
// page book cannot be a single bitmap at a readable size, and the strip ends where
// the budget does.
void ImageStatic::loadPdf() {
    QString error;
    mPdfDocument = PdfProvider::instance().open(mPath, &error);
    if(!mPdfDocument) {
        mLoaded = true;
        return;
    }
    mPdfPageCount = PdfProvider::instance().pageCount(mPdfDocument.get());
    if(mPdfPageCount <= 0) {
        mPdfNeedsPassword = PdfProvider::instance().isEncrypted(mPdfDocument.get());
        mLoaded = true;
        return;
    }
    mPdfPage = 0;

    std::shared_ptr<const QImage> strip = renderPdfStrip();
    if(!strip) {
        mLoaded = true;
        return;
    }
    {
        QMutexLocker lock(&mImageLock);
        image = strip;
        mFullLoaded = true;
    }
    mSourceSize = strip->size();
    mLoaded = true;
}

std::shared_ptr<const QImage> ImageStatic::renderPdfStrip() {
    if(!mPdfDocument || mPdfPageCount <= 0)
        return nullptr;

    // page geometry; pages of one document are usually identical
    const QSizeF firstPage = PdfProvider::instance().pageSize(mPdfDocument.get(), 0);
    if(firstPage.isEmpty())
        return nullptr;
    const double aspect = firstPage.height() / firstPage.width();

    // 4 bytes per pixel, and never more than a quarter of the configured limit so a
    // big book cannot push the viewer into swapping
    const qint64 budgetBytes = qint64(settings->memoryAllocationLimit()) * 1024 * 1024 / 4;
    const qint64 maxPixels = budgetBytes / 4;

    int pages = mPdfPageCount;
    int width = 1400;
    // shrink the pages first (still readable), then cut the strip short
    while(pages > 1) {
        const qint64 totalPixels = qint64(width) * qint64(width * aspect) * pages;
        if(totalPixels <= maxPixels)
            break;
        if(width > 700) {
            width -= 100;
            continue;
        }
        pages--;
    }
    pages = qMax(1, pages);
    const int pageHeight = qMax(16, int(width * aspect));
    const int totalHeight = pageHeight * pages;

    QImage strip(width, totalHeight, QImage::Format_RGB32);
    if(strip.isNull())
        return nullptr;
    strip.fill(Qt::white);

    QPainter painter(&strip);
    int y = 0;
    int rendered = 0;
    for(int page = 0; page < pages; page++) {
        const QImage pageImage = PdfProvider::instance().renderPage(mPdfDocument.get(), page,
                                                                   QSize(width, pageHeight));
        if(pageImage.isNull())
            break;
        painter.drawImage(0, y, pageImage);
        y += pageHeight;
        rendered++;
    }
    painter.end();
    mPdfPageHeight = pageHeight;
    mPdfRenderedPages = rendered;
    if(rendered == 0)
        return nullptr;
    if(rendered < pages) {
        // drop the unused tail if a page failed to render
        const QImage trimmed = strip.copy(0, 0, width, pageHeight * rendered);
        return std::make_shared<const QImage>(trimmed);
    }
    return std::make_shared<const QImage>(strip);
}

// Unlocks a protected document and renders it. Called from the gui thread once the
// password is known.
bool ImageStatic::unlockPdf(const QString &password) {
    if(!mPdfDocument || !mPdfNeedsPassword)
        return false;
    if(!PdfProvider::instance().unlock(mPdfDocument.get(), password))
        return false;
    mPdfPageCount = PdfProvider::instance().pageCount(mPdfDocument.get());
    if(mPdfPageCount <= 0)
        return false;
    std::shared_ptr<const QImage> strip = renderPdfStrip();
    if(!strip)
        return false;
    {
        QMutexLocker lock(&mImageLock);
        image = strip;
        mFullLoaded = true;
    }
    mSourceSize = strip->size();
    mPdfNeedsPassword = false;
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
std::unique_ptr<const QImage> ImageStatic::readImage(QSize scaledTarget) {
    QImageReader r(mPath, mDocInfo->format().toStdString().c_str());
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    r.setAllocationLimit(settings->memoryAllocationLimit());
#endif
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
