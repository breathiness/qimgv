#include "jxldecoder.h"

#include <QFile>
#include <QFileInfo>
#include <QByteArray>
#include <cstring>

#ifdef USE_LIBJXL
#include <jxl/decode.h>
#include <jxl/thread_parallel_runner.h>
#endif

namespace {

#ifdef USE_LIBJXL

struct Decoder {
    JxlDecoder *dec = nullptr;
    void *runner = nullptr;
    Decoder() { dec = JxlDecoderCreate(nullptr); }
    ~Decoder()
    {
        if(dec)
            JxlDecoderDestroy(dec);
        if(runner)
            JxlThreadParallelRunnerDestroy(runner);
    }
};

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if(!file.open(QFile::ReadOnly))
        return QByteArray();
    return file.readAll();
}

// Reads only the codestream header: no pixels, no demosaic, just the fields the
// animation support needs (dimensions, alpha, loop count, tick rate).
bool readBasicInfo(const QByteArray &data, JxlBasicInfo *info)
{
    if(data.isEmpty() || !info)
        return false;
    Decoder d;
    if(!d.dec)
        return false;
    if(JxlDecoderSubscribeEvents(d.dec, JXL_DEC_BASIC_INFO) != JXL_DEC_SUCCESS)
        return false;
    JxlDecoderSetInput(d.dec, reinterpret_cast<const uint8_t *>(data.constData()),
                       size_t(data.size()));
    JxlDecoderCloseInput(d.dec);

    for(;;) {
        const JxlDecoderStatus status = JxlDecoderProcessInput(d.dec);
        if(status == JXL_DEC_BASIC_INFO)
            return JxlDecoderGetBasicInfo(d.dec, info) == JXL_DEC_SUCCESS;
        if(status == JXL_DEC_ERROR || status == JXL_DEC_NEED_MORE_INPUT
           || status == JXL_DEC_SUCCESS || status == JXL_DEC_NEED_IMAGE_OUT_BUFFER)
            return false;
    }
}

// Runs the decoder until the first frame is complete. Returns an invalid image
// on any error; `infoOnly` stops as soon as the header is known.
QImage decode(const QByteArray &data, const QSize &targetSize, QSize *infoSize)
{
    if(data.isEmpty())
        return QImage();

    Decoder d;
    if(!d.dec)
        return QImage();

    // decoding uses all cores where libjxl allows it; a 12 MP image takes well
    // over a second single threaded
    d.runner = JxlThreadParallelRunnerCreate(nullptr, JxlThreadParallelRunnerDefaultNumWorkerThreads());
    if(d.runner)
        JxlDecoderSetParallelRunner(d.dec, JxlThreadParallelRunner, d.runner);

    // only the informative events go here: JXL_DEC_NEED_IMAGE_OUT_BUFFER is a
    // status, subscribing to it makes JxlDecoderSubscribeEvents fail
    if(JxlDecoderSubscribeEvents(d.dec, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE) != JXL_DEC_SUCCESS)
        return QImage();
    JxlDecoderSetInput(d.dec, reinterpret_cast<const uint8_t *>(data.constData()),
                       size_t(data.size()));
    JxlDecoderCloseInput(d.dec);

    JxlBasicInfo info{};
    QImage image;
    JxlPixelFormat format = {4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};

    for(;;) {
        const JxlDecoderStatus status = JxlDecoderProcessInput(d.dec);
        switch(status) {
        case JXL_DEC_BASIC_INFO:
            if(JxlDecoderGetBasicInfo(d.dec, &info) != JXL_DEC_SUCCESS)
                return QImage();
            if(infoSize)
                *infoSize = QSize(int(info.xsize), int(info.ysize));
            if(infoSize)
                return QImage();
            break;
        case JXL_DEC_NEED_IMAGE_OUT_BUFFER: {
            size_t needed = 0;
            if(JxlDecoderImageOutBufferSize(d.dec, &format, &needed) != JXL_DEC_SUCCESS)
                return QImage();
            image = QImage(int(info.xsize), int(info.ysize), QImage::Format_RGBA8888);
            if(image.isNull())
                return QImage();
            if(uintptr_t(needed) > uintptr_t(image.sizeInBytes()))
                return QImage();
            if(JxlDecoderSetImageOutBuffer(d.dec, &format, image.bits(),
                                           size_t(image.sizeInBytes())) != JXL_DEC_SUCCESS)
                return QImage();
        } break;
        case JXL_DEC_FULL_IMAGE:
            // first frame is enough
            return image;
        case JXL_DEC_SUCCESS:
            return image;
        case JXL_DEC_ERROR:
        case JXL_DEC_NEED_MORE_INPUT:
            return QImage();
        default:
            break;
        }
    }
}

#endif // USE_LIBJXL

} // namespace

namespace JxlImage {

bool isAvailable()
{
#ifdef USE_LIBJXL
    return true;
#else
    return false;
#endif
}

bool isJxlSuffix(const QString &suffix)
{
    return suffix.compare(QLatin1String("jxl"), Qt::CaseInsensitive) == 0;
}

QSize size(const QString &path)
{
#ifdef USE_LIBJXL
    QSize result;
    decode(readFile(path), QSize(), &result);
    return result;
#else
    Q_UNUSED(path)
    return QSize();
#endif
}

bool isAnimated(const QString &path)
{
#ifdef USE_LIBJXL
    JxlBasicInfo info{};
    if(!readBasicInfo(readFile(path), &info))
        return false;
    return info.have_animation == JXL_TRUE;
#else
    Q_UNUSED(path)
    return false;
#endif
}

QImage read(const QString &path, const QSize &targetSize)
{
#ifdef USE_LIBJXL
    QImage image = decode(readFile(path), targetSize, nullptr);
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

bool readAnimation(const QString &path, Animation *out, const QSize &targetSize)
{
    if(!out)
        return false;
    *out = Animation();
#ifdef USE_LIBJXL
    const QByteArray data = readFile(path);
    if(data.isEmpty())
        return false;

    Decoder d;
    if(!d.dec)
        return false;
    d.runner = JxlThreadParallelRunnerCreate(nullptr, JxlThreadParallelRunnerDefaultNumWorkerThreads());
    if(d.runner)
        JxlDecoderSetParallelRunner(d.dec, JxlThreadParallelRunner, d.runner);

    if(JxlDecoderSubscribeEvents(d.dec, JXL_DEC_BASIC_INFO | JXL_DEC_FRAME
                                          | JXL_DEC_FULL_IMAGE) != JXL_DEC_SUCCESS)
        return false;
    JxlDecoderSetInput(d.dec, reinterpret_cast<const uint8_t *>(data.constData()),
                       size_t(data.size()));
    JxlDecoderCloseInput(d.dec);

    // Every frame is a full canvas, so the retained size is the canvas times the
    // frames. Above this the animation plays what fits instead of exhausting
    // memory; the plugin has no way to stream frames on demand here.
    constexpr qint64 maxAnimationBytes = 384LL * 1024 * 1024;
    constexpr int maxFrames = 2000;

    JxlBasicInfo info{};
    JxlPixelFormat format = {4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
    QImage canvas;
    int delayMs = 100;
    qint64 retainedBytes = 0;

    for(;;) {
        const JxlDecoderStatus status = JxlDecoderProcessInput(d.dec);
        switch(status) {
        case JXL_DEC_BASIC_INFO:
            if(JxlDecoderGetBasicInfo(d.dec, &info) != JXL_DEC_SUCCESS)
                return !out->frames.isEmpty();
            out->numLoops = int(info.animation.num_loops);
            break;
        case JXL_DEC_FRAME: {
            JxlFrameHeader header{};
            if(JxlDecoderGetFrameHeader(d.dec, &header) == JXL_DEC_SUCCESS) {
                const quint32 num = info.animation.tps_numerator ? info.animation.tps_numerator : 1;
                const quint32 den = info.animation.tps_denominator ? info.animation.tps_denominator : 1;
                const qint64 ms = qint64(header.duration) * 1000 * qint64(den) / qint64(num);
                delayMs = int(qBound(qint64(1), ms, qint64(60000)));            }
        } break;
        case JXL_DEC_NEED_IMAGE_OUT_BUFFER: {
            size_t needed = 0;
            if(JxlDecoderImageOutBufferSize(d.dec, &format, &needed) != JXL_DEC_SUCCESS || needed == 0)
                return !out->frames.isEmpty();
            if(canvas.isNull() || size_t(canvas.sizeInBytes()) < needed) {
                canvas = QImage(int(info.xsize), int(info.ysize), QImage::Format_RGBA8888);
                if(canvas.isNull())
                    return !out->frames.isEmpty();
            }
            if(JxlDecoderSetImageOutBuffer(d.dec, &format, canvas.bits(),
                                           size_t(canvas.sizeInBytes())) != JXL_DEC_SUCCESS)
                return !out->frames.isEmpty();
        } break;
        case JXL_DEC_FULL_IMAGE: {
            QImage frame = canvas.copy();
            if(frame.isNull())
                return !out->frames.isEmpty();
            if(targetSize.isValid() && (targetSize.width() < frame.width()
                                        || targetSize.height() < frame.height())) {
                frame = frame.scaled(targetSize, Qt::KeepAspectRatio,
                                     Qt::SmoothTransformation);
            }
            retainedBytes += qint64(frame.sizeInBytes());
            if(!out->frames.isEmpty()
               && (retainedBytes > maxAnimationBytes || out->frames.size() >= maxFrames)) {
                out->truncated = true;
                return true;
            }
            out->frames.append(Frame{frame, delayMs});
        } break;
        case JXL_DEC_SUCCESS:
            return !out->frames.isEmpty();
        case JXL_DEC_ERROR:
        case JXL_DEC_NEED_MORE_INPUT:
            return !out->frames.isEmpty();
        default:
            break;
        }
    }
#else
    Q_UNUSED(path)
    Q_UNUSED(targetSize)
    return false;
#endif
}

} // namespace JxlImage
