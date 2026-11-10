#include "exrdecoder.h"

#ifdef USE_OPENEXR
#include <ImfRgbaFile.h>
#include <ImathBox.h>
#include <half.h>

#include <cmath>
#include <cstddef>
#include <string>
#include <vector>
#endif

namespace {

#ifdef USE_OPENEXR

// The file holds linear light, the screen shows 8 bit sRGB-ish values: clamp,
// then apply gamma. Values above 1 clip, which is the usual "raw" look of an
// image viewer.
inline uchar linearToByte(float value)
{
    if(!(value > 0.0f))
        return 0;
    const float clamped = value > 1.0f ? 1.0f : value;
    return uchar(qBound(0, int(std::pow(clamped, 1.0f / 2.2f) * 255.0f + 0.5f), 255));
}

inline void writePixel(uchar *dest, float r, float g, float b, float a)
{
    dest[0] = linearToByte(r);
    dest[1] = linearToByte(g);
    dest[2] = linearToByte(b);
    dest[3] = uchar(qBound(0, int(a * 255.0f + 0.5f), 255));
}

// OpenEXR documents its file name arguments as UTF-8 on every platform,
// including Windows; QFile::encodeName() would hand over the local code page
// there, so a non-ASCII path (中文 名称.exr) failed to open.
std::string exrPath(const QString &path)
{
    return path.toUtf8().toStdString();
}

#endif // USE_OPENEXR

} // namespace

namespace ExrDecoder {

bool isAvailable()
{
#ifdef USE_OPENEXR
    return true;
#else
    return false;
#endif
}

bool isExrSuffix(const QString &suffix)
{
    return suffix.compare(QLatin1String("exr"), Qt::CaseInsensitive) == 0;
}

QSize size(const QString &path)
{
#ifdef USE_OPENEXR
    try {
        Imf::RgbaInputFile file(exrPath(path).c_str());
        const Imath::Box2i window = file.dataWindow();
        const int width = window.max.x - window.min.x + 1;
        const int height = window.max.y - window.min.y + 1;
        if(width > 0 && height > 0)
            return QSize(width, height);
    } catch(...) {
        // not an EXR, or a variant OpenEXR refuses to open
    }
#else
    Q_UNUSED(path)
#endif
    return QSize();
}

QImage read(const QString &path, const QSize &targetSize)
{
#ifdef USE_OPENEXR
    try {
        Imf::RgbaInputFile file(exrPath(path).c_str());
        const Imath::Box2i window = file.dataWindow();
        const int width = window.max.x - window.min.x + 1;
        const int height = window.max.y - window.min.y + 1;
        if(width <= 0 || height <= 0)
            return QImage();

        QSize outputSize(width, height);
        if(targetSize.isValid()
           && (targetSize.width() < width || targetSize.height() < height)) {
            outputSize = QSize(width, height).scaled(targetSize, Qt::KeepAspectRatio);
        }
        if(outputSize.isEmpty())
            return QImage();

        // sample the file instead of reading everything: a thumbnail of an 8K
        // plate then costs a couple of megabytes instead of ~250
        const int stepX = qMax(1, width / outputSize.width());
        const int stepY = qMax(1, height / outputSize.height());
        const int outW = (width + stepX - 1) / stepX;
        const int outH = (height + stepY - 1) / stepY;

        QImage image(outW, outH, QImage::Format_RGBA8888);
        if(image.isNull())
            return QImage();

        // Rows are read in blocks. At full resolution that is 64 rows per read
        // call; when sampling, one output row's worth of source rows is read and
        // averaged, so the buffer stays small.
        const int blockRows = qMax(stepY, 64);
        std::vector<Imf::Rgba> block(size_t(width) * size_t(blockRows));

        for(int y = 0; y < outH; y++) {
            const int sourceY = window.min.y + y * stepY;
            const int rows = qMin(stepY, qMax(1, window.max.y - sourceY + 1));
            Imf::Rgba *base = block.data()
                              + (ptrdiff_t(-window.min.x) - ptrdiff_t(sourceY) * ptrdiff_t(width));
            file.setFrameBuffer(base, 1, width);
            file.readPixels(sourceY, sourceY + rows - 1);

            uchar *line = image.scanLine(y);
            for(int x = 0; x < outW; x++) {
                const int startX = x * stepX;
                const int samplesX = qMin(stepX, qMax(1, window.max.x - (window.min.x + startX) + 1));
                float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;
                int samples = 0;
                for(int sy = 0; sy < rows; sy++) {
                    const Imf::Rgba *srcRow = block.data() + size_t(sy) * size_t(width);
                    for(int sx = 0; sx < samplesX; sx++) {
                        const Imf::Rgba &pixel = srcRow[size_t(startX + sx)];
                        r += float(pixel.r);
                        g += float(pixel.g);
                        b += float(pixel.b);
                        a += float(pixel.a);
                        samples++;
                    }
                }
                if(samples > 0) {
                    r /= samples;
                    g /= samples;
                    b /= samples;
                    a /= samples;
                }
                writePixel(line + size_t(x) * 4, r, g, b, a);
            }
        }
        return image;
    } catch(...) {
        return QImage();
    }
#else
    Q_UNUSED(path)
    Q_UNUSED(targetSize)
    return QImage();
#endif
}

} // namespace ExrDecoder
