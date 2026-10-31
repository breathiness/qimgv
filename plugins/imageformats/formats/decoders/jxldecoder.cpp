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

} // namespace JxlImage
