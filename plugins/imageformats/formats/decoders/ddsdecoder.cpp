#include "ddsdecoder.h"

#include <QFile>

#include <cmath>
#include <cstring>
#include <vector>

#define BCDEC_IMPLEMENTATION
#include "bcdec.h"

namespace {

enum class BlockFormat {
    None,
    BC1,
    BC2,
    BC3,
    BC4,
    BC5,
    BC6H,
    BC7,
    Uncompressed
};

struct DdsHeader {
    bool ok = false;
    quint32 width = 0;
    quint32 height = 0;
    BlockFormat format = BlockFormat::None;
    int dataOffset = 128;
    int bitCount = 0;
    quint32 redMask = 0;
    quint32 greenMask = 0;
    quint32 blueMask = 0;
    quint32 alphaMask = 0;
    bool signedBlocks = false;
};

quint32 readLe32(const uchar *data)
{
    return quint32(data[0]) | (quint32(data[1]) << 8) | (quint32(data[2]) << 16)
           | (quint32(data[3]) << 24);
}

BlockFormat formatFromDxgi(quint32 format, bool *signedBlocks)
{
    *signedBlocks = false;
    switch(format) {
    case 70: case 71: return BlockFormat::BC1;
    case 73: case 74: return BlockFormat::BC2;
    case 76: case 77: return BlockFormat::BC3;
    case 79: *signedBlocks = true; return BlockFormat::BC4;
    case 80: return BlockFormat::BC4;
    case 82: *signedBlocks = true; return BlockFormat::BC5;
    case 83: return BlockFormat::BC5;
    case 94: *signedBlocks = true; return BlockFormat::BC6H;
    case 95: return BlockFormat::BC6H;
    case 97: case 98: return BlockFormat::BC7;
    default: return BlockFormat::None;
    }
}

DdsHeader parseHeader(const QByteArray &data)
{
    DdsHeader header;
    if(data.size() < 128 || !data.startsWith("DDS "))
        return header;
    const uchar *bytes = reinterpret_cast<const uchar *>(data.constData());
    header.height = readLe32(bytes + 12);
    header.width = readLe32(bytes + 16);
    if(header.width == 0 || header.height == 0 || header.width > 65536 || header.height > 65536)
        return header;

    const QByteArray fourCc = data.mid(84, 4);
    if(fourCc == "DXT1") {
        header.format = BlockFormat::BC1;
    } else if(fourCc == "DXT2" || fourCc == "DXT3") {
        header.format = BlockFormat::BC2;
    } else if(fourCc == "DXT4" || fourCc == "DXT5") {
        header.format = BlockFormat::BC3;
    } else if(fourCc == "ATI1" || fourCc == "BC4U") {
        header.format = BlockFormat::BC4;
    } else if(fourCc == "BC4S") {
        header.format = BlockFormat::BC4;
        header.signedBlocks = true;
    } else if(fourCc == "ATI2" || fourCc == "BC5U") {
        header.format = BlockFormat::BC5;
    } else if(fourCc == "BC5S") {
        header.format = BlockFormat::BC5;
        header.signedBlocks = true;
    } else if(fourCc == "DX10") {
        if(data.size() < 148)
            return header;
        bool signedBlocks = false;
        header.format = formatFromDxgi(readLe32(bytes + 128), &signedBlocks);
        header.signedBlocks = signedBlocks;
        header.dataOffset = 148;
    } else {
        // no known four character code: a plain surface described by masks
        header.bitCount = int(readLe32(bytes + 88));
        header.redMask = readLe32(bytes + 92);
        header.greenMask = readLe32(bytes + 96);
        header.blueMask = readLe32(bytes + 100);
        header.alphaMask = readLe32(bytes + 104);
        if((header.bitCount == 24 || header.bitCount == 32) && header.redMask != 0)
            header.format = BlockFormat::Uncompressed;
    }

    header.ok = header.format != BlockFormat::None;
    return header;
}

int blockBytes(BlockFormat format)
{
    switch(format) {
    case BlockFormat::BC1:
    case BlockFormat::BC4:
        return 8;
    case BlockFormat::BC2:
    case BlockFormat::BC3:
    case BlockFormat::BC5:
    case BlockFormat::BC6H:
    case BlockFormat::BC7:
        return 16;
    default:
        return 0;
    }
}

inline uchar maskChannel(quint32 value, quint32 mask)
{
    if(mask == 0)
        return 0;
    int shift = 0;
    while(((mask >> shift) & 1u) == 0)
        shift++;
    quint32 bits = mask >> shift;
    int count = 0;
    while((bits >> count) & 1u)
        count++;
    const quint32 raw = (value & mask) >> shift;
    const quint32 maximum = (1u << count) - 1u;
    if(maximum == 0)
        return 0;
    return uchar((raw * 255u + maximum / 2u) / maximum);
}

inline uchar toneMap(float value)
{
    if(!(value > 0.0f))
        return 0;
    const float clamped = value > 1.0f ? 1.0f : value;
    return uchar(qBound(0, int(std::pow(clamped, 1.0f / 2.2f) * 255.0f + 0.5f), 255));
}

void copyBlock(const uchar *source, int sourceStride, QImage *image, int blockX, int blockY)
{
    const int width = image->width();
    const int height = image->height();
    for(int y = 0; y < 4; y++) {
        const int targetY = blockY * 4 + y;
        if(targetY >= height)
            break;
        uchar *dest = image->scanLine(targetY);
        for(int x = 0; x < 4; x++) {
            const int targetX = blockX * 4 + x;
            if(targetX >= width)
                break;
            std::memcpy(dest + targetX * 4, source + y * sourceStride + x * 4, 4);
        }
    }
}

} // namespace

namespace DdsImage {

bool isDdsSuffix(const QString &suffix)
{
    return suffix.compare(QLatin1String("dds"), Qt::CaseInsensitive) == 0;
}

QSize size(const QString &path)
{
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
        return QSize();
    const DdsHeader header = parseHeader(file.read(160));
    return header.ok ? QSize(int(header.width), int(header.height)) : QSize();
}

QImage read(const QString &path, const QSize &targetSize)
{
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
        return QImage();
    const QByteArray data = file.readAll();
    const DdsHeader header = parseHeader(data);
    if(!header.ok)
        return QImage();

    const int width = int(header.width);
    const int height = int(header.height);
    QImage image(width, height, QImage::Format_RGBA8888);
    if(image.isNull())
        return QImage();

    if(header.format == BlockFormat::Uncompressed) {
        const int bytesPerPixel = header.bitCount / 8;
        const qint64 needed = qint64(width) * height * bytesPerPixel;
        if(data.size() - header.dataOffset < needed)
            return QImage();
        const uchar *source = reinterpret_cast<const uchar *>(data.constData()) + header.dataOffset;
        for(int y = 0; y < height; y++) {
            const uchar *row = source + qint64(y) * width * bytesPerPixel;
            uchar *dest = image.scanLine(y);
            for(int x = 0; x < width; x++) {
                quint32 value = 0;
                std::memcpy(&value, row + x * bytesPerPixel, size_t(bytesPerPixel));
                dest[x * 4 + 0] = maskChannel(value, header.redMask);
                dest[x * 4 + 1] = maskChannel(value, header.greenMask);
                dest[x * 4 + 2] = maskChannel(value, header.blueMask);
                dest[x * 4 + 3] = maskChannel(value, header.alphaMask);
            }
        }
    } else {
        const int bytesPerBlock = blockBytes(header.format);
        if(bytesPerBlock == 0)
            return QImage();
        const int blocksX = (width + 3) / 4;
        const int blocksY = (height + 3) / 4;
        const qint64 needed = qint64(blocksX) * blocksY * bytesPerBlock;
        if(data.size() - header.dataOffset < needed)
            return QImage();

        const uchar *blocks = reinterpret_cast<const uchar *>(data.constData()) + header.dataOffset;
        if(header.format == BlockFormat::BC4 || header.format == BlockFormat::BC5) {
            // These carry single channels (height maps, normal maps). Show R (and
            // G) as colour; a normal map's Z is not reconstructed.
            const bool twoChannels = header.format == BlockFormat::BC5;
            std::vector<uchar> block(twoChannels ? 32 : 16);
            QImage channelImage(width, height, QImage::Format_RGBA8888);
            channelImage.fill(Qt::transparent);
            for(int by = 0; by < blocksY; by++) {
                for(int bx = 0; bx < blocksX; bx++) {
                    const uchar *source = blocks + (qint64(by) * blocksX + bx) * bytesPerBlock;
                    if(twoChannels)
                        bcdec_bc5(source, block.data(), 8);
                    else
                        bcdec_bc4(source, block.data(), 4);
                    for(int y = 0; y < 4; y++) {
                        const int targetY = by * 4 + y;
                        if(targetY >= height)
                            break;
                        uchar *dest = channelImage.scanLine(targetY);
                        for(int x = 0; x < 4; x++) {
                            const int targetX = bx * 4 + x;
                            if(targetX >= width)
                                break;
                            uchar r = 0;
                            uchar g = 0;
                            if(twoChannels) {
                                const quint16 packed = quint16(block[(y * 4 + x) * 2])
                                                       | (quint16(block[(y * 4 + x) * 2 + 1]) << 8);
                                r = uchar(packed & 0xff);
                                g = uchar(packed >> 8);
                            } else {
                                r = g = block[y * 4 + x];
                            }
                            dest[targetX * 4 + 0] = r;
                            dest[targetX * 4 + 1] = g;
                            dest[targetX * 4 + 2] = 0;
                            dest[targetX * 4 + 3] = 255;
                        }
                    }
                }
            }
            image = channelImage;
        } else {
            const int stride = 4 * 4;
            std::vector<uchar> rgba(size_t(stride) * 4);
            std::vector<float> rgbFloat(size_t(4) * 4 * 3);
            for(int by = 0; by < blocksY; by++) {
                for(int bx = 0; bx < blocksX; bx++) {
                    const uchar *source = blocks + (qint64(by) * blocksX + bx) * bytesPerBlock;
                    switch(header.format) {
                    case BlockFormat::BC1:
                        bcdec_bc1(source, rgba.data(), stride);
                        copyBlock(rgba.data(), stride, &image, bx, by);
                        break;
                    case BlockFormat::BC2:
                        bcdec_bc2(source, rgba.data(), stride);
                        copyBlock(rgba.data(), stride, &image, bx, by);
                        break;
                    case BlockFormat::BC3:
                        bcdec_bc3(source, rgba.data(), stride);
                        copyBlock(rgba.data(), stride, &image, bx, by);
                        break;
                    case BlockFormat::BC7:
                        bcdec_bc7(source, rgba.data(), stride);
                        copyBlock(rgba.data(), stride, &image, bx, by);
                        break;
                    case BlockFormat::BC6H: {
                        bcdec_bc6h_float(source, rgbFloat.data(), 12, header.signedBlocks ? 1 : 0);
                        for(int y = 0; y < 4; y++) {
                            const int targetY = by * 4 + y;
                            if(targetY >= height)
                                break;
                            uchar *dest = image.scanLine(targetY);
                            for(int x = 0; x < 4; x++) {
                                const int targetX = bx * 4 + x;
                                if(targetX >= width)
                                    break;
                                const float *pixel = &rgbFloat[(y * 4 + x) * 3];
                                dest[targetX * 4 + 0] = toneMap(pixel[0]);
                                dest[targetX * 4 + 1] = toneMap(pixel[1]);
                                dest[targetX * 4 + 2] = toneMap(pixel[2]);
                                dest[targetX * 4 + 3] = 255;
                            }
                        }
                        break;
                    }
                    default:
                        break;
                    }
                }
            }
        }
    }

    if(targetSize.isValid()
       && (targetSize.width() < image.width() || targetSize.height() < image.height())) {
        return image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return image;
}

} // namespace DdsImage
