#include "imagestatic.h"
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
    std::shared_ptr<const QImage> full = readImage(QSize());
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
    if(!r.read(tmp.get()) || tmp->isNull())
        return nullptr;
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
