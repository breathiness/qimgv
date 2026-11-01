#include "formatshandler.h"

#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QStringList>

#include "decoders/heifdecoder.h"
#include "decoders/jxldecoder.h"
#include "decoders/psddecoder.h"
#include "decoders/rawdecoder.h"

namespace {

const QStringList &heifSuffixes()
{
    static const QStringList list = {"heic", "heif", "hif", "avif"};
    return list;
}

const QStringList &rawSuffixes()
{
    static const QStringList list = {
        "cr2", "cr3", "crw", "nef", "nrw", "arw", "srf", "sr2", "raf", "orf",
        "rw2", "rwl", "dng", "pef", "srw", "x3f", "3fr", "fff", "kdc", "dcr",
        "mrw", "mos", "raw", "iiq", "erf", "mef", "ptx", "r3d", "nkd", "pxn"
    };
    return list;
}

QString suffixOf(const QString &path)
{
    return QFileInfo(path).suffix().toLower();
}

// Embedded previews are much cheaper than a full decode. They are used only when
// the caller asked for something at most as large as the preview itself.
QImage scaledFromPreview(const QImage &preview, const QSize &target)
{
    if(preview.isNull() || !target.isValid())
        return QImage();
    if(preview.width() < target.width() && preview.height() < target.height())
        return QImage();
    return preview.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

} // namespace

FormatsHandler::FormatsHandler() = default;

QStringList FormatsHandler::supportedSuffixes()
{
    QStringList all = heifSuffixes();
    all << "jxl" << "psd" << "psb";
    all << rawSuffixes();
    return all;
}

bool FormatsHandler::isSupportedSuffix(const QString &suffix)
{
    const QString s = suffix.toLower();
    return heifSuffixes().contains(s) || s == QLatin1String("jxl")
           || s == QLatin1String("psd") || s == QLatin1String("psb")
           || rawSuffixes().contains(s);
}

QString FormatsHandler::filePath() const
{
    if(auto *fileDevice = qobject_cast<QFileDevice *>(device()))
        return fileDevice->fileName();
    return QString();
}

QString FormatsHandler::resolvedSuffix() const
{
    // the format the reader asked for is the suffix, unless it is empty or "?"
    const QString requested = format().toLower();
    if(isSupportedSuffix(requested))
        return requested;
    return suffixOf(filePath());
}

bool FormatsHandler::canRead() const
{
    const QString suffix = resolvedSuffix();
    if(!isSupportedSuffix(suffix))
        return false;
    if(!filePath().isEmpty())
        return QFileInfo::exists(filePath());
    return device() != nullptr;
}

bool FormatsHandler::supportsOption(ImageOption option) const
{
    switch(option) {
    case Size:
    case ScaledSize:
    case ImageFormat:
    case Animation:
        return true;
    default:
        return QImageIOHandler::supportsOption(option);
    }
}

QVariant FormatsHandler::option(ImageOption option) const
{
    const QString path = filePath();
    const QString suffix = resolvedSuffix();
    switch(option) {
    case ScaledSize:
        // what read() is going to produce
        return mScaledSize.isValid() ? QVariant(mScaledSize) : QVariant();
    case Animation:
        // only JPEG XL can animate here; for everything else this stays false so
        // QMovie does not take over a still image
        return ensureAnimationLoaded();
    case Size: {
        if(path.isEmpty())
            return QVariant();
        if(suffix == QLatin1String("psd") || suffix == QLatin1String("psb"))
            return PsdDecoder::size(path);
        if(JxlImage::isJxlSuffix(suffix))
            return JxlImage::size(path);
        if(RawImage::isRawSuffix(suffix))
            return RawImage::size(path);
        if(HeifDecoder::isHeifSuffix(suffix))
            return HeifDecoder::size(path);
        return QVariant();
    }
    case ImageFormat:
        return suffix;
    default:
        return QVariant();
    }
}

bool FormatsHandler::ensureAnimationLoaded() const
{
    if(mAnimProbed)
        return mAnimIsAnimated;
    mAnimProbed = true;
    mAnimIsAnimated = false;

    const QString path = filePath();
    const QString suffix = resolvedSuffix();
    if(path.isEmpty() || !JxlImage::isJxlSuffix(suffix) || !JxlImage::isAvailable())
        return false;
    // the header says whether this is an animation at all; asking readAnimation()
    // would decode a still image twice (once here, once for the actual read)
    if(!JxlImage::isAnimated(path))
        return false;

    JxlImage::Animation animation;
    if(!JxlImage::readAnimation(path, &animation) || animation.frames.size() < 2)
        return false;   // a single frame does not need the animation path

    mAnimFrames = animation.frames;
    mAnimNumLoops = animation.numLoops;
    mAnimNext = 0;
    mAnimCurrent = 0;
    mAnimIsAnimated = true;
    return true;
}

void FormatsHandler::setOption(ImageOption option, const QVariant &value)
{
    // QImageReader::setScaledSize() arrives here. Without storing it the reader
    // gets a full size image for every thumbnail: a handler that advertises
    // ScaledSize support is expected to scale, Qt will not rescale the result.
    if(option == ScaledSize)
        mScaledSize = value.toSize();
    else
        QImageIOHandler::setOption(option, value);
}

QImage FormatsHandler::decode(const QString &path, const QString &suffix, const QSize &target) const
{
    if(path.isEmpty())
        return QImage();

    if(HeifDecoder::isHeifSuffix(suffix) && HeifDecoder::isAvailable()) {
        if(target.isValid()) {
            const QImage preview = HeifDecoder::readThumbnail(path, qMin(target.width(), target.height()));
            const QImage scaled = scaledFromPreview(preview, target);
            if(!scaled.isNull())
                return scaled;
        }
        return HeifDecoder::read(path, target);
    }

    if(RawImage::isRawSuffix(suffix) && RawImage::isAvailable()) {
        if(target.isValid()) {
            // the preview the camera stored in the file is a jpeg: much cheaper
            // than developing the sensor data for a thumbnail
            const QImage preview = RawImage::readPreview(path);
            const QImage scaled = scaledFromPreview(preview, target);
            if(!scaled.isNull())
                return scaled;
        }
        return RawImage::read(path, target);
    }

    if(JxlImage::isJxlSuffix(suffix) && JxlImage::isAvailable())
        return JxlImage::read(path, target);

    if(PsdDecoder::isPsdSuffix(suffix)) {
        if(target.isValid()) {
            const QImage preview = PsdDecoder::readThumbnail(path, qMin(target.width(), target.height()));
            const QImage scaled = scaledFromPreview(preview, target);
            if(!scaled.isNull())
                return scaled;
        }
        return PsdDecoder::read(path, target);
    }

    return QImage();
}

bool FormatsHandler::read(QImage *image)
{
    if(!image)
        return false;
    const QString path = filePath();
    const QString suffix = resolvedSuffix();
    if(path.isEmpty() || !isSupportedSuffix(suffix))
        return false;

    if(ensureAnimationLoaded() && !mAnimFrames.isEmpty()) {
        // QMovie reads one frame per call, like the APNG handler does
        const int index = qBound(0, mAnimNext, mAnimFrames.size() - 1);
        QImage frame = mAnimFrames[index].image;
        if(mScaledSize.isValid() && (mScaledSize.width() < frame.width()
                                     || mScaledSize.height() < frame.height())) {
            frame = frame.scaled(mScaledSize, Qt::KeepAspectRatio,
                                 Qt::SmoothTransformation);
        }
        *image = frame;
        mAnimCurrent = index;
        mAnimNext = (index + 1) % mAnimFrames.size();
        return true;
    }

    const QSize requested = mScaledSize;
    const QSize target = requested.isValid() ? requested : QSize();
    const QImage decoded = decode(path, suffix, target);
    if(decoded.isNull())
        return false;
    *image = decoded;
    return true;
}

int FormatsHandler::imageCount() const
{
    if(ensureAnimationLoaded())
        return mAnimFrames.size();
    return 1;
}

int FormatsHandler::currentImageNumber() const
{
    return mAnimCurrent;
}

bool FormatsHandler::jumpToImage(int imageNumber)
{
    if(!ensureAnimationLoaded() || mAnimFrames.isEmpty())
        return false;
    if(imageNumber < 0 || imageNumber >= mAnimFrames.size())
        return false;
    mAnimNext = imageNumber;
    mAnimCurrent = imageNumber;
    return true;
}

bool FormatsHandler::jumpToNextImage()
{
    if(!ensureAnimationLoaded() || mAnimFrames.isEmpty())
        return false;
    mAnimNext = (mAnimNext + 1) % mAnimFrames.size();
    mAnimCurrent = mAnimNext;
    return true;
}

int FormatsHandler::loopCount() const
{
    if(!ensureAnimationLoaded())
        return 1;   // QImageIOHandler's default for a still image
    // JXL num_loops is 0 for "forever" and n for "play n times"; Qt wants the
    // number of *repeats*, with -1 for forever (same as the APNG handler).
    if(mAnimNumLoops == 0)
        return -1;
    return qMax(0, mAnimNumLoops - 1);
}

int FormatsHandler::nextImageDelay() const
{
    if(ensureAnimationLoaded() && mAnimCurrent >= 0
       && mAnimCurrent < mAnimFrames.size()) {
        return mAnimFrames[mAnimCurrent].delayMs;
    }
    return 0;
}
