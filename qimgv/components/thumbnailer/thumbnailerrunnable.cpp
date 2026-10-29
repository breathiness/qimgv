#include "thumbnailerrunnable.h"

#ifdef USE_MPV
#include "mpvframegrabber.h"
#endif

#ifdef _WIN32
#include <windows.h>
#include <shobjidl.h>
#endif

ThumbnailerRunnable::ThumbnailerRunnable(ThumbnailCache* _cache, QString _path, int _size, bool _crop, bool _force) :
    path(_path),
    size(_size),
    crop(_crop),
    force(_force),
    cache(_cache)
{
}

void ThumbnailerRunnable::run() {
    emit taskStart(path, size);
    std::shared_ptr<Thumbnail> thumbnail = generate(cache, path, size, crop, force);
    emit taskEnd(thumbnail, path);
}

QString ThumbnailerRunnable::generateIdString(QString path, int size, bool crop) {
    QString queryStr = path + QString::number(size);
    if(crop)
        queryStr.append("s");
    queryStr = QString("%1").arg(QString(QCryptographicHash::hash(queryStr.toUtf8(),QCryptographicHash::Md5).toHex()));
    return queryStr;
}

std::shared_ptr<Thumbnail> ThumbnailerRunnable::generate(ThumbnailCache* cache, QString path, int size, bool crop, bool force) {
    // The file name and the modification time are all a cache hit needs. Working
    // out the type and the EXIF orientation means DocumentInfo sniffs the file,
    // which is wasted when the answer is already in the cache - and scrolling a
    // folder asks for the same thumbnails over and over.
    const QFileInfo fileInfo(path);
    const QString fileName = fileInfo.fileName();
    const QString thumbnailId = generateIdString(path, size, crop);
    const QString time = QString::number(fileInfo.lastModified().toMSecsSinceEpoch());
    std::unique_ptr<QImage> image;

    if(!force && cache) {
        image.reset(cache->readThumbnail(thumbnailId));
        if(image && image->text("lastModified") != time)
            image.reset(nullptr);
    }

    if(!image) {
        DocumentInfo imgInfo(path);
        if(imgInfo.type() == DocumentType::NONE) {
            std::shared_ptr<Thumbnail> thumbnail(new Thumbnail(fileName, "", size, nullptr));
            return thumbnail;
        }
        std::pair<QImage*, QSize> pair(nullptr, QSize());
        if(imgInfo.type() == VIDEO) {
#ifdef _WIN32
            // fastest first: the Explorer shell already has (or cheaply makes)
            // a frame, and falls back to decoding only when it has nothing
            pair = createShellThumbnail(path, size, crop);
#endif
            if(!pair.first)
                pair = createVideoThumbnail(path, size, crop);
        } else {
            pair = createThumbnail(imgInfo.filePath(), imgInfo.format().toStdString().c_str(), size, crop);
        }
        image.reset(pair.first);
        QSize originalSize = pair.second;

        if(!image) {
            // every strategy failed: report an error thumbnail instead of
            // dereferencing a null image further down
            std::shared_ptr<Thumbnail> failed(new Thumbnail(fileName, "", size, nullptr));
            return failed;
        }

        image = ImageLib::exifRotated(std::move(image), imgInfo.exifOrientation());

        // put in image info
        image->setText("originalWidth", QString::number(originalSize.width()));
        image->setText("originalHeight", QString::number(originalSize.height()));
        image->setText("lastModified", time);

        if(imgInfo.type() == ANIMATED)
            image->setText("label", " [a]");
        else if(imgInfo.type() == VIDEO)
            image->setText("label", " [v]");

        if(cache) {
            // An image is only worth caching once it had to be scaled down: a
            // cached copy of one smaller than the tile would just be an upscaled
            // one. A video frame is different - it cost a decode (and for clips
            // below the tile size, the only thing that ever gets cached is
            // exactly that small frame), so it is always kept.
            const bool worthCaching = imgInfo.type() == VIDEO
                                      || originalSize.width() > size
                                      || originalSize.height() > size;
            if(worthCaching)
                cache->saveThumbnail(image.get(), thumbnailId);
        }
    }
    auto && tmpPixmap = new QPixmap(QPixmap::fromImage(*image));
    tmpPixmap->setDevicePixelRatio(qApp->devicePixelRatio());

    QString label;
    if(tmpPixmap->width() == 0) {
        label = "error";
    } else  {
        // put info into Thumbnail object
        label = image->text("originalWidth") +
                "x" +
                image->text("originalHeight") +
                image->text("label");
    }
    std::shared_ptr<QPixmap> pixmapPtr(tmpPixmap);
    std::shared_ptr<Thumbnail> thumbnail(new Thumbnail(fileName, label, size, pixmapPtr));
    return thumbnail;
}

ThumbnailerRunnable::~ThumbnailerRunnable() {
}

std::pair<QImage*, QSize> ThumbnailerRunnable::createThumbnail(QString path, const char *format, int size, bool squared) {
    QImageReader *reader = new QImageReader(path, format);
    Qt::AspectRatioMode ARMode = squared?
                (Qt::KeepAspectRatioByExpanding):(Qt::KeepAspectRatio);
    QImage *result = nullptr;
    QSize originalSize;
    bool indexed = (reader->imageFormat() == QImage::Format_Indexed8);
    bool manualResize = indexed || !reader->supportsOption(QImageIOHandler::Size);
    if(!manualResize) { // resize during read via QImageReader (faster)
        QSize scaledSize = reader->size().scaled(size, size, ARMode);
        reader->setScaledSize(scaledSize);
        if(squared) {
            QRect clip(0, 0, size, size);
            QRect scaledRect(QPoint(0,0), scaledSize);
            clip.moveCenter(scaledRect.center());
            reader->setScaledClipRect(clip);
        }
        originalSize = reader->size();
        result = new QImage();
        if(!reader->read(result)) {
            // If read() returns false there's no guarantee that size conversion worked properly.
            // So we fallback to manual.
            // Se far I've seen this happen only on some weird (corrupted?) jpeg saved from camera
            manualResize = true;
            delete result;
            result = nullptr;
            // Force reset reader because it is really finicky
            // and can fail on the second read attempt (yeah wtf)
            reader->setFileName("");
            delete reader;
            reader = new QImageReader(path, format);
        }
    }
    if(manualResize) { // manual resize & crop. slower but should just work
        QImage *fullSize = new QImage();
        if(!reader->read(fullSize)) {
            // nothing to scale: report a failed thumbnail instead of an empty
            // one that would still be handed to the view
            delete fullSize;
            reader->setFileName("");
            delete reader;
            return std::make_pair(nullptr, QSize());
        }
        if(indexed) {
            auto newFmt = QImage::Format_RGB32;
            if(fullSize->hasAlphaChannel())
                newFmt = QImage::Format_ARGB32;
            auto tmp = new QImage(fullSize->convertToFormat(newFmt));
            delete fullSize;
            fullSize = tmp;
        }
        originalSize = fullSize->size();
        QSize scaledSize = fullSize->size().scaled(size, size, ARMode);
        if(squared) {
            QRect clip(0, 0, size, size);
            QRect scaledRect(QPoint(0,0), scaledSize);
            clip.moveCenter(scaledRect.center());
            QImage scaled = QImage(fullSize->scaled(scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            result = ImageLib::croppedRaw(&scaled, clip);
        } else {
            result = new QImage(fullSize->scaled(scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        }
        delete fullSize;
    }
    // force reader to close file so it can be deleted later
    reader->setFileName("");
    delete reader;
    return std::make_pair(result, originalSize);
}

#ifdef _WIN32
// Qt 5.15 no longer ships QImage::fromHBITMAP, so the DIB is read manually.
// Format_RGB32 is used on purpose: shell thumbnails often carry a zero alpha
// channel which would otherwise make the whole frame transparent.
static QImage imageFromHBITMAP(HBITMAP bitmap) {
    if(!bitmap)
        return QImage();
    BITMAP bm;
    if(!GetObject(bitmap, sizeof(BITMAP), &bm))
        return QImage();
    if(bm.bmWidth <= 0 || bm.bmHeight <= 0)
        return QImage();

    BITMAPINFOHEADER bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bm.bmWidth;
    bi.biHeight = -bm.bmHeight; // negative = top-down rows
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    QImage image(bm.bmWidth, bm.bmHeight, QImage::Format_RGB32);
    HDC dc = GetDC(nullptr);
    const int lines = GetDIBits(dc, bitmap, 0, bm.bmHeight, image.bits(),
                                reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);
    if(lines != bm.bmHeight)
        return QImage();
    return image;
}

// Explorer keeps a thumbnail cache for videos, so asking for it costs a few
// milliseconds instead of spawning a whole mpv process.
std::pair<QImage*, QSize> ThumbnailerRunnable::createShellThumbnail(QString path, int size, bool squared) {
    QImage image;
    const HRESULT initResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool shouldUninitialize = SUCCEEDED(initResult);

    IShellItemImageFactory *factory = nullptr;
    HRESULT hr = SHCreateItemFromParsingName(reinterpret_cast<PCWSTR>(path.utf16()),
                                             nullptr, IID_PPV_ARGS(&factory));
    if(SUCCEEDED(hr) && factory) {
        SIZE requestedSize;
        requestedSize.cx = size;
        requestedSize.cy = size;
        HBITMAP bitmap = nullptr;
        // THUMBNAILONLY: never settle for a generic icon, a real frame is wanted
        hr = factory->GetImage(requestedSize, SIIGBF_THUMBNAILONLY | SIIGBF_BIGGERSIZEOK, &bitmap);
        if(SUCCEEDED(hr) && bitmap) {
            QImage converted = imageFromHBITMAP(bitmap);
            DeleteObject(bitmap);
            if(!converted.isNull())
                image = converted;
        }
        factory->Release();
    }

    if(shouldUninitialize)
        CoUninitialize();

    if(image.isNull())
        return std::make_pair(nullptr, QSize());

    const QSize originalSize = image.size();
    QImage *result = nullptr;
    if(squared) {
        const QImage scaledImage = image.scaled(size, size, Qt::KeepAspectRatioByExpanding,
                                                Qt::SmoothTransformation);
        QRect clip(0, 0, size, size);
        clip.moveCenter(QRect(QPoint(0, 0), scaledImage.size()).center());
        result = new QImage(scaledImage.copy(clip));
    } else {
        const QSize scaledSize = originalSize.scaled(size, size, Qt::KeepAspectRatio);
        result = new QImage(image.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    return std::make_pair(result, originalSize);
}
#endif

std::pair<QImage*, QSize> ThumbnailerRunnable::createVideoThumbnail(QString path, int size, bool squared) {
    QImageReader reader;
    // Keyed by the thumbnail id rather than by the file name: two clips with the
    // same name in different folders, or the same clip at another size, would
    // otherwise write one temporary file from two worker threads at once.
    const QString tmpFilePath = settings->tmpDir() + generateIdString(path, size, squared) + ".png";
    QFile tmpFile(tmpFilePath);

    // One second in, so the frame shows something instead of a fade-in or a
    // black lead-in. Clips shorter than that have no frame there, so fall back
    // to the very beginning.
#ifdef USE_MPV
    if(!MpvFrameGrabber::grabFrame(path, 1.0, tmpFilePath)
       && !MpvFrameGrabber::grabFrame(path, 0.0, tmpFilePath)) {
        // a grab that timed out can still have left a partial file behind
        tmpFile.remove();
        return std::make_pair(nullptr, QSize());
    }
#else
    // built without video support: there is no libmpv to grab a frame with
    return std::make_pair(nullptr, QSize());
#endif

    reader.setFileName(tmpFilePath);
    reader.setFormat("png");
    Qt::AspectRatioMode ARMode = squared?
                (Qt::KeepAspectRatioByExpanding):(Qt::KeepAspectRatio);
    QImage *result = nullptr;

    // scale & crop
    QSize scaledSize = reader.size().scaled(size, size, ARMode);
    reader.setScaledSize(scaledSize);
    if(squared) {
        QRect clip(0, 0, size, size);
        QRect scaledRect(QPoint(0,0), scaledSize);
        clip.moveCenter(scaledRect.center());
        reader.setScaledClipRect(clip);
    }
    QSize originalSize = reader.size();
    result = new QImage(reader.read());

    // force reader to close file so it can be deleted later
    reader.setFileName("");

    // remove temporary file
    tmpFile.remove();

    return std::make_pair(result, originalSize);
}
