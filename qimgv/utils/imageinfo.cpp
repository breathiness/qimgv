#include "imageinfo.h"

#include <QColorSpace>
#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QImageReader>
#include <QLocale>

namespace {

QString tr(const char *text) {
    return QCoreApplication::translate("ImageInfo", text);
}

// Written out here rather than through QLocale: the panel is translated, but the
// units would otherwise follow the system language and mix with it ("479 字节"
// under an english heading on a chinese system).
QString formatBytes(qint64 bytes) {
    if(bytes <= 0)
        return tr("unknown");
    static const char *units[] = { "B", "KB", "MB", "GB", "TB" };
    double value = double(bytes);
    int unit = 0;
    while(value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        unit++;
    }
    const int precision = (unit == 0) ? 0 : (value < 10.0 ? 2 : 1);
    return QStringLiteral("%1 %2").arg(QString::number(value, 'f', precision))
                                  .arg(QLatin1String(units[unit]));
}

QString formatDateTime(const QDateTime &time) {
    if(!time.isValid())
        return QString();
    return QLocale().toString(time, QLocale::ShortFormat);
}

// "4:3" while a small pair describes the shape closely enough, a decimal
// otherwise (nobody reads a panorama as 121:160).
QString formatAspectRatio(const QSize &size) {
    if(size.isEmpty())
        return QString();
    const qreal ratio = qreal(size.width()) / size.height();
    for(int w = 1; w <= 40; w++) {
        const int h = qRound(w / ratio);
        if(h <= 0)
            continue;
        const qreal delta = qAbs((qreal(w) / h) - ratio) / ratio;
        if(delta < 0.01)
            return QStringLiteral("%1:%2").arg(w).arg(h);
    }
    return ratio >= 1 ? QStringLiteral("%1:1").arg(QString::number(ratio, 'f', 2))
                      : QStringLiteral("1:%1").arg(QString::number(1 / ratio, 'f', 2));
}

QString formatPixels(const QSize &size) {
    return QStringLiteral("%1 x %2").arg(size.width()).arg(size.height());
}

QString formatMegapixels(const QSize &size) {
    const qreal mp = (qreal(size.width()) * size.height()) / 1000000.0;
    return QStringLiteral("%1 MP").arg(QString::number(mp, 'f', 1));
}

// What the pixels are, in words rather than a Qt enum.
QString describeFormat(const QImage &image) {
    switch(image.format()) {
    case QImage::Format_Mono:
    case QImage::Format_MonoLSB:
        return tr("1-bit monochrome");
    case QImage::Format_Indexed8:
        return tr("%1-colour palette").arg(image.colorCount());
    case QImage::Format_Alpha8:
        return tr("8-bit alpha only");
    case QImage::Format_RGB32:
    case QImage::Format_RGBX8888:
        return tr("24-bit RGB");
    case QImage::Format_ARGB32:
    case QImage::Format_ARGB32_Premultiplied:
    case QImage::Format_RGBA8888:
    case QImage::Format_RGBA8888_Premultiplied:
        return tr("32-bit RGBA");
    case QImage::Format_RGB888:
        return tr("24-bit RGB, no padding");
    case QImage::Format_RGB16:
        return tr("16-bit RGB");
    case QImage::Format_Grayscale8:
        return tr("8-bit grayscale");
    case QImage::Format_Grayscale16:
        return tr("16-bit grayscale");
    case QImage::Format_RGBX64:
    case QImage::Format_RGBA64:
    case QImage::Format_RGBA64_Premultiplied:
        return tr("16-bit per channel");
    default:
        return QStringLiteral("%1-bit").arg(image.depth());
    }
}

QString describeColorSpace(const QImage &image) {
    const QColorSpace space = image.colorSpace();
    if(!space.isValid())
        return QString();
    if(space == QColorSpace::SRgb || space == QColorSpace::SRgbLinear)
        return QStringLiteral("sRGB");
    const QString name = space.description();
    return name.isEmpty() ? QStringLiteral("ICC") : name;
}

QString formatDpi(const QImage &image) {
    const int x = qRound(image.dotsPerMeterX() * 0.0254);
    const int y = qRound(image.dotsPerMeterY() * 0.0254);
    if(x <= 0 && y <= 0)
        return QString();
    if(x == y)
        return QStringLiteral("%1 dpi").arg(x);
    return QStringLiteral("%1 x %2 dpi").arg(x).arg(y);
}

// A long value would push the overlay out of shape; the entry keeps the full text
// as its tooltip.
QString shorten(const QString &text) {
    const int limit = 300;
    if(text.size() <= limit)
        return text;
    return text.left(limit - 1) + QChar(0x2026);
}

} // namespace

QList<ImageInfoSection> gatherImageInfo(const QString &filePath,
                                        const QImage &image,
                                        const QMap<QString, QString> &extraTags) {
    QList<ImageInfoSection> sections;

    const QFileInfo fileInfo(filePath);

    // --- the file on disk ---
    {
        ImageInfoSection section;
        section.title = tr("File");
        section.entries.append({tr("Name"), fileInfo.fileName()});
        section.entries.append({tr("Folder"), fileInfo.absolutePath()});
        section.entries.append({tr("Size"), formatBytes(fileInfo.size())});
        const QString modified = formatDateTime(fileInfo.lastModified());
        if(!modified.isEmpty())
            section.entries.append({tr("Modified"), modified});
        const QString created = formatDateTime(fileInfo.birthTime());
        if(!created.isEmpty())
            section.entries.append({tr("Created"), created});
        sections.append(section);
    }

    // --- the pixels ---
    QImageReader reader(filePath);
    const QByteArray fileFormat = reader.canRead() ? reader.format() : QByteArray();

    if(!image.isNull()) {
        ImageInfoSection section;
        section.title = tr("Image");
        section.entries.append({tr("Dimensions"), formatPixels(image.size())});
        section.entries.append({tr("Megapixels"), formatMegapixels(image.size())});
        const QString aspect = formatAspectRatio(image.size());
        if(!aspect.isEmpty())
            section.entries.append({tr("Aspect ratio"), aspect});
        if(!fileFormat.isEmpty())
            section.entries.append({tr("Format"), QString::fromLatin1(fileFormat).toUpper()});
        // "Pixels" rather than "Colour": this describes what the viewer decoded,
        // which for an indexed file is not the bit depth the file stores
        section.entries.append({tr("Pixels"), describeFormat(image)});
        if(image.hasAlphaChannel())
            section.entries.append({tr("Alpha"), tr("yes")});
        const QString space = describeColorSpace(image);
        if(!space.isEmpty())
            section.entries.append({tr("Colour space"), space});
        const QString dpi = formatDpi(image);
        if(dpi.isEmpty())
            section.entries.append({tr("Resolution"), tr("not set")});
        else
            section.entries.append({tr("Resolution"), dpi});
        if(reader.canRead()) {
            if(reader.imageCount() > 1)
                section.entries.append({tr("Frames"), QString::number(reader.imageCount())});
            else if(reader.supportsAnimation())
                section.entries.append({tr("Frames"), tr("animated")});
        }
        sections.append(section);
    }

    // --- text the file carries (png tEXt, jpeg comment, ...) ---
    if(reader.canRead()) {
        QMap<QString, QString> text;
        for(const QString &key : reader.textKeys())
            text.insert(key, reader.text(key));
        if(!text.isEmpty()) {
            ImageInfoSection section;
            section.title = tr("Embedded text");
            for(auto it = text.constBegin(); it != text.constEnd(); ++it) {
                const QString value = shorten(it.value().trimmed());
                if(value.isEmpty())
                    continue;
                section.entries.append({it.key(), value});
            }
            if(!section.entries.isEmpty())
                sections.append(section);
        }
    }

    // --- whatever a metadata library could add (exif) ---
    if(!extraTags.isEmpty()) {
        ImageInfoSection section;
        section.title = tr("Camera");
        for(auto it = extraTags.constBegin(); it != extraTags.constEnd(); ++it)
            section.entries.append({it.key(), shorten(it.value().trimmed())});
        sections.append(section);
    }

    return sections;
}
