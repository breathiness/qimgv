#include "thumbnailerrunnable.h"

#include "cad/caddrawing.h"
#include "utils/pdfprovider.h"

#include <QDir>

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
        image = cache->readThumbnail(thumbnailId);
        if(image && image->text("lastModified") != time)
            image.reset();
    }

    if(!image) {
        DocumentInfo imgInfo(path);
        if(imgInfo.type() == DocumentType::NONE) {
            return std::make_shared<Thumbnail>(fileName, "", size, nullptr);
        }

        ThumbnailImage generated;
        if(imgInfo.type() == VIDEO) {
#ifdef _WIN32
            // fastest first: the Explorer shell already has (or cheaply makes)
            // a frame, and falls back to decoding only when it has nothing
            generated = createShellThumbnail(path, size, crop);
#endif
            if(!generated.image)
                generated = createVideoThumbnail(path, size, crop);
        } else {
            // QImageReader copies the format name, but keeping it alive for the
            // call beats relying on how long a temporary would last.
            const QByteArray format = imgInfo.format().toUtf8();
            if(imgInfo.isCad() || imgInfo.isPdf())
                generated = createDocumentThumbnail(imgInfo.filePath(), imgInfo.type(), size, crop);
            else
                generated = createThumbnail(imgInfo.filePath(), format.constData(), size, crop);
        }
        const QSize originalSize = generated.originalSize;
        image = std::move(generated.image);

        if(!image) {
            // every strategy failed: report an error thumbnail instead of
            // dereferencing a null image further down
            return std::make_shared<Thumbnail>(fileName, "", size, nullptr);
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
    auto pixmap = std::make_unique<QPixmap>(QPixmap::fromImage(*image));
    pixmap->setDevicePixelRatio(qApp->devicePixelRatio());

    QString label;
    if(pixmap->width() == 0) {
        label = "error";
    } else  {
        // put info into Thumbnail object
        label = image->text("originalWidth") +
                "x" +
                image->text("originalHeight") +
                image->text("label");
    }
    std::shared_ptr<QPixmap> pixmapPtr(std::move(pixmap));
    return std::make_shared<Thumbnail>(fileName, label, size, pixmapPtr);
}

ThumbnailerRunnable::~ThumbnailerRunnable() {
}

// Fast first: a dxf that carries its own preview is done in a single file read; a
// drawing without one, or a pdf, is rendered straight at the thumbnail size, which
// is small enough that the geometry work stays cheap. Nothing here renders at
// screen size - that is the loader's job when the file is actually opened.
ThumbnailerRunnable::ThumbnailImage ThumbnailerRunnable::createDocumentThumbnail(const QString &path,
                                                                                DocumentType type, int size,
                                                                                bool squared) {
    ThumbnailImage result;
    Q_UNUSED(type)
    const int target = qMax(16, size) * 2;   // 2x for a little headroom when the grid scales
    QImage source;

    const QImage embedded = CadDrawing::embeddedThumbnail(path);
    if(!embedded.isNull()) {
        source = embedded;
    } else if(path.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)) {
        QString error;
        std::shared_ptr<void> document = PdfProvider::instance().open(path, &error);
        if(document) {
            const QSizeF pageSize = PdfProvider::instance().pageSize(document.get(), 0);
            if(!pageSize.isEmpty()) {
                const double scale = double(target) / qMax(pageSize.width(), pageSize.height());
                const QSize renderSize(qMax(16, int(pageSize.width() * scale)),
                                       qMax(16, int(pageSize.height() * scale)));
                source = PdfProvider::instance().renderPage(document.get(), 0, renderSize);
            }
        }
    } else {
        CadDrawing drawing;
        if(drawing.load(path)) {
            const QRectF bounds = drawing.bounds();
            if(bounds.width() > 0 && bounds.height() > 0) {
                const double scale = double(target) / qMax(bounds.width(), bounds.height());
                source = drawing.render(qMax(16, int(bounds.width() * scale)),
                                        qMax(16, int(bounds.height() * scale)));
            }
        }
    }
    if(source.isNull())
        return result;

    result.originalSize = source.size();
    if(squared) {
        const int side = qMin(source.width(), source.height());
        source = source.copy((source.width() - side) / 2, (source.height() - side) / 2, side, side);
    }
    result.image.reset(new QImage(source.scaled(size, size, Qt::KeepAspectRatio,
                                                Qt::SmoothTransformation)));
    return result;
}

ThumbnailerRunnable::ThumbnailImage ThumbnailerRunnable::createThumbnail(const QString &path, const char *format, int size, bool squared) {
    Qt::AspectRatioMode ARMode = squared?
                (Qt::KeepAspectRatioByExpanding):(Qt::KeepAspectRatio);
    auto reader = std::make_unique<QImageReader>(path, format);
    ThumbnailImage result;

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
        result.originalSize = reader->size();
        result.image = std::make_unique<QImage>();
        if(!reader->read(result.image.get())) {
            // If read() returns false there's no guarantee that size conversion worked properly.
            // So we fallback to manual.
            // Se far I've seen this happen only on some weird (corrupted?) jpeg saved from camera
            manualResize = true;
            result.image.reset();
            result.originalSize = QSize();
            // Force reset reader because it is really finicky
            // and can fail on the second read attempt (yeah wtf)
            reader = std::make_unique<QImageReader>(path, format);
        }
    }
    if(manualResize) { // manual resize & crop. slower but should just work
        auto fullSize = std::make_unique<QImage>();
        if(!reader->read(fullSize.get())) {
            // nothing to scale: report a failed thumbnail instead of an empty
            // one that would still be handed to the view
            return ThumbnailImage();
        }
        if(indexed) {
            auto newFmt = QImage::Format_RGB32;
            if(fullSize->hasAlphaChannel())
                newFmt = QImage::Format_ARGB32;
            fullSize = std::make_unique<QImage>(fullSize->convertToFormat(newFmt));
        }
        result.originalSize = fullSize->size();
        QSize scaledSize = fullSize->size().scaled(size, size, ARMode);
        if(squared) {
            QRect clip(0, 0, size, size);
            QRect scaledRect(QPoint(0,0), scaledSize);
            clip.moveCenter(scaledRect.center());
            QImage scaled = fullSize->scaled(scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            result.image = std::unique_ptr<QImage>(ImageLib::croppedRaw(&scaled, clip));
        } else {
            result.image = std::make_unique<QImage>(fullSize->scaled(scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        }
    }
    // the reader closes the file when it goes out of scope here
    return result;
}

#ifdef _WIN32
namespace {
// COINIT_APARTMENTTHREADED has to be balanced by CoUninitialize on every way out
// of the function, and there is more than one now.
class ScopedComInitialization {
public:
    ScopedComInitialization() :
        mInitialized(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)))
    {}
    ~ScopedComInitialization() {
        if(mInitialized)
            CoUninitialize();
    }
    ScopedComInitialization(const ScopedComInitialization &) = delete;
    ScopedComInitialization &operator=(const ScopedComInitialization &) = delete;
private:
    const bool mInitialized;
};

// IShellItemImageFactory is a COM interface, so it is released, not deleted.
struct ShellItemImageFactoryReleaser {
    void operator()(IShellItemImageFactory *factory) const {
        if(factory)
            factory->Release();
    }
};
} // namespace

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
ThumbnailerRunnable::ThumbnailImage ThumbnailerRunnable::createShellThumbnail(const QString &path, int size, bool squared) {
    const ScopedComInitialization com;

    IShellItemImageFactory *createdFactory = nullptr;
    const HRESULT createResult = SHCreateItemFromParsingName(reinterpret_cast<PCWSTR>(path.utf16()),
                                                             nullptr, IID_PPV_ARGS(&createdFactory));
    std::unique_ptr<IShellItemImageFactory, ShellItemImageFactoryReleaser> factory(createdFactory);
    if(FAILED(createResult) || !factory)
        return ThumbnailImage();

    SIZE requestedSize;
    requestedSize.cx = size;
    requestedSize.cy = size;

    HBITMAP bitmap = nullptr;
    // THUMBNAILONLY: never settle for a generic icon, a real frame is wanted
    const HRESULT fetchResult = factory->GetImage(requestedSize, SIIGBF_THUMBNAILONLY | SIIGBF_BIGGERSIZEOK, &bitmap);
    QImage image;
    if(SUCCEEDED(fetchResult) && bitmap) {
        image = imageFromHBITMAP(bitmap);
        DeleteObject(bitmap);
    }
    if(image.isNull())
        return ThumbnailImage();

    ThumbnailImage result;
    result.originalSize = image.size();
    if(squared) {
        const QImage scaledImage = image.scaled(size, size, Qt::KeepAspectRatioByExpanding,
                                                Qt::SmoothTransformation);
        QRect clip(0, 0, size, size);
        clip.moveCenter(QRect(QPoint(0, 0), scaledImage.size()).center());
        result.image = std::make_unique<QImage>(scaledImage.copy(clip));
    } else {
        const QSize scaledSize = result.originalSize.scaled(size, size, Qt::KeepAspectRatio);
        result.image = std::make_unique<QImage>(image.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    return result;
}
#endif

ThumbnailerRunnable::ThumbnailImage ThumbnailerRunnable::createVideoThumbnail(const QString &path, int size, bool squared) {
    // One second in, so the frame shows something instead of a fade-in or a black
    // lead-in. Clips shorter than that have no frame there, so fall back to the
    // very beginning. The frame comes back as an image: the plugin used to write a
    // temporary png, which failed wherever the temp directory was not writable.
    QImage frame;
    if(!MpvFrameGrabber::grabFrame(path, 1.0, &frame)
       && !MpvFrameGrabber::grabFrame(path, 0.0, &frame)) {
        // without this a missing frame looks exactly like a video that has none
        qWarning() << "Thumbnailer: could not grab a frame from" << path;
        return ThumbnailImage();
    }

    ThumbnailImage result;
    result.originalSize = frame.size();
    const Qt::AspectRatioMode ARMode = squared ? Qt::KeepAspectRatioByExpanding
                                               : Qt::KeepAspectRatio;
    QImage scaled = frame.scaled(QSize(size, size), ARMode, Qt::SmoothTransformation);
    if(squared && (scaled.width() > size || scaled.height() > size)) {
        const int x = qMax(0, (scaled.width() - size) / 2);
        const int y = qMax(0, (scaled.height() - size) / 2);
        scaled = scaled.copy(x, y, qMin(size, scaled.width()), qMin(size, scaled.height()));
    }
    result.image = std::make_unique<QImage>(scaled);
    return result;
}
