#include "rawdecoder.h"

#include <QFileInfo>
#include <cstring>

#ifdef USE_LIBRAW
#include <libraw/libraw.h>
#endif

namespace {

#ifdef USE_LIBRAW

// libraw needs the file name in the local 8 bit encoding
QByteArray nativePath(const QString &path)
{
    return QFile::encodeName(path);
}

QImage fromProcessed(libraw_processed_image_t *processed)
{
    if(!processed || processed->width == 0 || processed->height == 0)
        return QImage();
    QImage::Format format = processed->colors == 4 ? QImage::Format_RGBA8888
                                                   : QImage::Format_RGB888;
    QImage image(int(processed->width), int(processed->height), format);
    if(image.isNull())
        return QImage();
    const int rowBytes = image.width() * (processed->colors == 4 ? 4 : 3);
    for(int y = 0; y < image.height(); y++) {
        std::memcpy(image.scanLine(y), processed->data + size_t(y) * size_t(rowBytes), size_t(rowBytes));
    }
    return image;
}

#endif // USE_LIBRAW

} // namespace

namespace RawImage {

bool isAvailable()
{
#ifdef USE_LIBRAW
    return true;
#else
    return false;
#endif
}

bool isRawSuffix(const QString &suffix)
{
    // the extensions libraw documents as camera raw; "tif"/"jpg" are excluded on
    // purpose, a plain TIFF or JPEG must keep going through Qt
    static const char *const suffixes[] = {
        "cr2", "cr3", "crw", "nef", "nrw", "arw", "srf", "sr2", "raf", "orf",
        "rw2", "rwl", "dng", "pef", "srw", "x3f", "3fr", "fff", "kdc", "dcr",
        "mrw", "mos", "raw", "iiq", "erf", "mef", "ptx", "r3d", "nkd", "pxn"
    };
    const QString s = suffix.toLower();
    for(const char *candidate : suffixes) {
        if(s == QLatin1String(candidate))
            return true;
    }
    return false;
}

QSize size(const QString &path)
{
#ifdef USE_LIBRAW
    LibRaw raw;
    if(raw.open_file(nativePath(path).constData()) != LIBRAW_SUCCESS)
        return QSize();
    const int w = raw.imgdata.sizes.iwidth ? raw.imgdata.sizes.iwidth
                                           : raw.imgdata.sizes.width;
    const int h = raw.imgdata.sizes.iheight ? raw.imgdata.sizes.iheight
                                            : raw.imgdata.sizes.height;
    return QSize(w, h);
#else
    Q_UNUSED(path)
    return QSize();
#endif
}

QImage read(const QString &path, const QSize &targetSize)
{
#ifdef USE_LIBRAW
    LibRaw raw;
    if(raw.open_file(nativePath(path).constData()) != LIBRAW_SUCCESS)
        return QImage();

    // half size development is roughly twice as fast; worth it when the screen
    // shows far less than the sensor resolution
    const int rawWidth = raw.imgdata.sizes.iwidth ? raw.imgdata.sizes.iwidth
                                                  : raw.imgdata.sizes.width;
    if(targetSize.isValid() && rawWidth > targetSize.width() * 3)
        raw.imgdata.params.half_size = 1;

    if(raw.unpack() != LIBRAW_SUCCESS)
        return QImage();
    if(raw.dcraw_process() != LIBRAW_SUCCESS)
        return QImage();

    int status = LIBRAW_SUCCESS;
    libraw_processed_image_t *processed = raw.dcraw_make_mem_image(&status);
    if(status != LIBRAW_SUCCESS) {
        if(processed)
            LibRaw::dcraw_clear_mem(processed);
        return QImage();
    }
    QImage image = fromProcessed(processed);
    LibRaw::dcraw_clear_mem(processed);

    if(image.isNull())
        return image;
    if(targetSize.isValid() && (targetSize.width() < image.width()
                                || targetSize.height() < image.height())) {
        image = image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return image;
#else
    Q_UNUSED(path)
    Q_UNUSED(targetSize)
    return QImage();
#endif
}

QImage readPreview(const QString &path)
{
#ifdef USE_LIBRAW
    LibRaw raw;
    if(raw.open_file(nativePath(path).constData()) != LIBRAW_SUCCESS)
        return QImage();
    if(raw.unpack_thumb() != LIBRAW_SUCCESS)
        return QImage();
    const libraw_thumbnail_t &thumb = raw.imgdata.thumbnail;
    if(!thumb.thumb || thumb.tlength <= 0)
        return QImage();
    // cameras store this as JPEG, a few formats use a plain bitmap
    if(thumb.tformat == LIBRAW_THUMBNAIL_JPEG)
        return QImage::fromData(reinterpret_cast<const uchar *>(thumb.thumb), int(thumb.tlength));
    if(thumb.tformat == LIBRAW_THUMBNAIL_BITMAP)
        return QImage(reinterpret_cast<const uchar *>(thumb.thumb), thumb.twidth, thumb.theight,
                      QImage::Format_RGB888);
    return QImage();
#else
    Q_UNUSED(path)
    return QImage();
#endif
}

} // namespace RawImage
