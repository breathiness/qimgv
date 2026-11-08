#include "heifdecoder.h"

#include <QtGlobal>
#include <QFile>
#include <cstring>

#ifdef USE_LIBHEIF
#include <libheif/heif.h>
#include <libheif/heif_sequences.h>
#endif

namespace {

#ifdef USE_LIBHEIF

// Owns a heif_context/handle pair and frees them on every exit path, including
// the early returns below.
struct HeifContext {
    heif_context *ctx = nullptr;
    HeifContext() : ctx(heif_context_alloc()) {}
    ~HeifContext() { if(ctx) heif_context_free(ctx); }
};

// Wraps the decoded libheif image so the plane memory is released.
struct HeifImage {
    heif_image *img = nullptr;
    ~HeifImage() { if(img) heif_image_release(img); }
};

struct HeifHandle {
    heif_image_handle *handle = nullptr;
    ~HeifHandle() { if(handle) heif_image_handle_release(handle); }
};

bool openPrimary(const QString &path, HeifContext &ctx, HeifHandle &out)
{
    if(!ctx.ctx)
        return false;
    const QByteArray nativePath = QFile::encodeName(path);
    if(heif_context_read_from_file(ctx.ctx, nativePath.constData(), nullptr).code != heif_error_Ok)
        return false;
    if(heif_context_get_primary_image_handle(ctx.ctx, &out.handle).code != heif_error_Ok)
        return false;
    return out.handle != nullptr;
}

// libheif already applies the container's own rotations and mirrors while
// decoding, so nothing has to be rotated afterwards.
QImage toQImage(HeifImage &decoded)
{
    if(!decoded.img)
        return QImage();
    const heif_chroma chroma = heif_image_get_chroma_format(decoded.img);
    int stride = 0;
    const uint8_t *plane = heif_image_get_plane_readonly(decoded.img,
                                                         heif_channel_interleaved,
                                                         &stride);
    const int w = heif_image_get_width(decoded.img, heif_channel_interleaved);
    const int h = heif_image_get_height(decoded.img, heif_channel_interleaved);
    if(!plane || w <= 0 || h <= 0 || stride <= 0)
        return QImage();

    QImage::Format format = (chroma == heif_chroma_interleaved_RGBA)
                                ? QImage::Format_RGBA8888
                                : QImage::Format_RGB888;
    QImage image(w, h, format);
    if(image.isNull())
        return QImage();
    const int rowBytes = w * (format == QImage::Format_RGBA8888 ? 4 : 3);
    for(int y = 0; y < h; y++) {
        std::memcpy(image.scanLine(y), plane + qsizetype(y) * stride, size_t(rowBytes));
    }
    return image;
}

QImage decodeHandle(heif_image_handle *handle)
{
    if(!handle)
        return QImage();
    HeifImage decoded;
    heif_decoding_options *options = heif_decoding_options_alloc();
    // 8 bit RGB is what the viewer and the scaler want; HDR originals get
    // converted rather than showing up as garbage.
    if(options)
        options->convert_hdr_to_8bit = 1;
    const bool ok = heif_decode_image(handle, &decoded.img, heif_colorspace_RGB,
                                      heif_chroma_interleaved_RGBA,
                                      options).code == heif_error_Ok;
    if(options)
        heif_decoding_options_free(options);
    if(!ok)
        return QImage();
    return toQImage(decoded);
}

#endif // USE_LIBHEIF

} // namespace

namespace HeifDecoder {

bool isAvailable()
{
#ifdef USE_LIBHEIF
    return true;
#else
    return false;
#endif
}

bool isHeifSuffix(const QString &suffix)
{
    const QString s = suffix.toLower();
    return s == QLatin1String("heic") || s == QLatin1String("heif")
           || s == QLatin1String("hif") || s == QLatin1String("avif");
}

QSize size(const QString &path)
{
#ifdef USE_LIBHEIF
    HeifContext ctx;
    HeifHandle handle;
    if(!openPrimary(path, ctx, handle))
        return QSize();
    return QSize(heif_image_handle_get_width(handle.handle),
                 heif_image_handle_get_height(handle.handle));
#else
    Q_UNUSED(path)
    return QSize();
#endif
}

QImage read(const QString &path, const QSize &targetSize)
{
#ifdef USE_LIBHEIF
    HeifContext ctx;
    HeifHandle handle;
    if(!openPrimary(path, ctx, handle))
        return QImage();
    QImage image = decodeHandle(handle.handle);
    if(image.isNull())
        return image;
    if(targetSize.isValid() && (targetSize.width() < image.width()
                                || targetSize.height() < image.height())) {
        // keep the aspect ratio, the caller only knows the pixel budget
        image = image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return image;
#else
    Q_UNUSED(path)
    Q_UNUSED(targetSize)
    return QImage();
#endif
}

QImage readThumbnail(const QString &path, int minSize)
{
#ifdef USE_LIBHEIF
    HeifContext ctx;
    HeifHandle handle;
    if(!openPrimary(path, ctx, handle))
        return QImage();
    if(heif_image_handle_get_number_of_thumbnails(handle.handle) < 1)
        return QImage();
    HeifHandle thumb;
    if(heif_image_handle_get_thumbnail(handle.handle, 0, &thumb.handle).code != heif_error_Ok)
        return QImage();
    const int thumbWidth = heif_image_handle_get_width(thumb.handle);
    if(thumbWidth < minSize)
        return QImage();
    return decodeHandle(thumb.handle);
#else
    Q_UNUSED(path)
    Q_UNUSED(minSize)
    return QImage();
#endif
}

bool isAnimated(const QString &path)
{
#ifdef USE_LIBHEIF
    HeifContext ctx;
    if(!ctx.ctx)
        return false;
    const QByteArray nativePath = QFile::encodeName(path);
    if(heif_context_read_from_file(ctx.ctx, nativePath.constData(), nullptr).code != heif_error_Ok)
        return false;
    return heif_context_has_sequence(ctx.ctx) != 0;
#else
    Q_UNUSED(path)
    return false;
#endif
}

bool readAnimation(const QString &path, Animation *animation)
{
    if(!animation)
        return false;
    animation->frames.clear();
    animation->numLoops = 1;
#ifdef USE_LIBHEIF
    HeifContext ctx;
    if(!ctx.ctx)
        return false;
    const QByteArray nativePath = QFile::encodeName(path);
    if(heif_context_read_from_file(ctx.ctx, nativePath.constData(), nullptr).code != heif_error_Ok)
        return false;
    if(!heif_context_has_sequence(ctx.ctx))
        return false;

    // 0 asks for the first visual track
    heif_track *track = heif_context_get_track(ctx.ctx, 0);
    if(!track)
        return false;

    const uint32_t timescale = heif_track_get_timescale(track);
    heif_decoding_options *options = heif_decoding_options_alloc();
    if(options) {
        // 8 bit RGB is what the viewer wants, and the edit list is applied by
        // the viewer instead: with it, a file that loops forever never ends.
        options->convert_hdr_to_8bit = 1;
        options->ignore_sequence_editlist = 1;
    }

    bool ok = false;
    while(animation->frames.size() < 2000) {
        HeifImage decoded;
        const heif_error error = heif_track_decode_next_image(
            track, &decoded.img, heif_colorspace_RGB, heif_chroma_interleaved_RGBA, options);
        if(error.code != heif_error_Ok)
            break;   // heif_error_End_of_sequence on a complete file
        QImage image = toQImage(decoded);
        if(image.isNull())
            break;
        Frame frame;
        frame.image = image;
        const uint32_t ticks = heif_image_get_duration(decoded.img);
        frame.delayMs = timescale > 0 ? int(qint64(ticks) * 1000 / qint64(timescale)) : 100;
        if(frame.delayMs <= 0)
            frame.delayMs = 100;
        animation->frames.append(frame);
        ok = true;
    }

    if(options)
        heif_decoding_options_free(options);

    // 0 is "the edit list was not understood", 1 is "play once", and the maximum
    // value means it repeats for as long as the viewer lets it
    const uint32_t repetitions = heif_track_get_number_of_repetitions(track);
    heif_track_release(track);
    if(repetitions == 0)
        animation->numLoops = 1;
    else if(repetitions == heif_sequence_track_number_of_repetitions_infinite)
        animation->numLoops = 0;
    else
        animation->numLoops = int(repetitions);

    return ok && animation->frames.size() >= 2;
#else
    Q_UNUSED(path)
    return false;
#endif
}

} // namespace HeifDecoder
