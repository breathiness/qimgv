#include "exifreader.h"

#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QStringList>

#ifdef USE_LIBEXIF
#include <libexif/exif-data.h>
#include <libexif/exif-entry.h>
#include <libexif/exif-tag.h>
#endif

namespace {

QString tr(const char *text) {
    return QCoreApplication::translate("ExifReader", text);
}

#ifdef USE_LIBEXIF

// libexif formats most values itself ("1/250 sec.", "f/4.0", "Flash did not
// fire"), which keeps this file about labels rather than numbers.
QString valueOf(ExifData *data, ExifIfd ifd, ExifTag tag) {
    ExifEntry *entry = exif_content_get_entry(data->ifd[ifd], tag);
    if(!entry)
        return QString();
    char buffer[1024];
    if(!exif_entry_get_value(entry, buffer, sizeof(buffer)))
        return QString();
    return QString::fromUtf8(buffer).trimmed();
}

// libexif gives gps coordinates as "31, 13, 4905/100": degrees, minutes, and
// seconds as a rational. Turn that into the decimal form people paste into a map.
QString decimalDegrees(const QString &value, const QString &ref) {
    const QStringList parts = value.split(',', Qt::SkipEmptyParts);
    if(parts.isEmpty())
        return QString();
    auto toNumber = [](QString text) {
        text = text.trimmed();
        const int slash = text.indexOf('/');
        if(slash > 0) {
            bool okNum = false, okDen = false;
            const double num = text.left(slash).toDouble(&okNum);
            const double den = text.mid(slash + 1).toDouble(&okDen);
            if(okNum && okDen && den != 0)
                return num / den;
            return 0.0;
        }
        return text.toDouble();
    };
    double degrees = toNumber(parts.value(0));
    if(parts.size() > 1)
        degrees += toNumber(parts.value(1)) / 60.0;
    if(parts.size() > 2)
        degrees += toNumber(parts.value(2)) / 3600.0;
    if(ref.startsWith('S') || ref.startsWith('W'))
        degrees = -degrees;
    return QString::number(degrees, 'f', 6);
}

// libexif writes a datetime as "2024:03:15 14:22:31"; the exif standard uses the
// same separator everywhere, people read it as a date.
QString tidyDate(QString value) {
    if(value.size() > 10 && value.at(4) == QLatin1Char(':'))
        value[4] = QLatin1Char('-');
    if(value.size() > 10 && value.at(7) == QLatin1Char(':'))
        value[7] = QLatin1Char('-');
    return value;
}

// libexif prints the copyright block with its sub-fields in brackets; keep what an
// author would recognise as the notice.
QString tidyCopyright(QString value) {
    value.remove(QStringLiteral(" (Photographer)"));
    value.remove(QStringLiteral(" - [None] (Editor)"));
    value.remove(QStringLiteral(" (Editor)"));
    return value.trimmed();
}

// libexif hands back english words for the enumerated tags. Say them in the
// application's own language instead.
QString localiseValue(QString value) {
    // exposure time: "1/250 sec." / "2 sec."
    if(value.endsWith(QStringLiteral(" sec.")))
        return value.left(value.size() - 5) + QLatin1Char(' ') + tr("sec");

    static const QHash<QString, QString> words = {
        { QStringLiteral("Pattern"),                  tr("Pattern") },
        { QStringLiteral("Center-weighted average"),  tr("Center-weighted average") },
        { QStringLiteral("Spot"),                     tr("Spot") },
        { QStringLiteral("Multi-spot"),               tr("Multi-spot") },
        { QStringLiteral("Average"),                  tr("Average") },
        { QStringLiteral("Partial"),                  tr("Partial") },
        { QStringLiteral("Auto white balance"),       tr("Auto white balance") },
        { QStringLiteral("Manual white balance"),     tr("Manual white balance") },
        { QStringLiteral("Flash did not fire"),       tr("Flash did not fire") },
        { QStringLiteral("Flash did not fire, compulsory flash mode"),
                                                      tr("Flash did not fire") },
        { QStringLiteral("Flash fired"),              tr("Flash fired") },
        { QStringLiteral("Flash fired, compulsory flash mode"),
                                                      tr("Flash fired") },
        { QStringLiteral("Flash fired, auto mode"),   tr("Flash fired, auto mode") },
    };
    const auto it = words.constFind(value);
    return it == words.constEnd() ? value : it.value();
}

// "1/250" reads better than "0.004", but a long exposure reads worse: keep both
// forms sensible. libexif already prints the usual one, so only the sign changes.
QString exposureBias(const QString &value) {
    if(value.isEmpty())
        return QString();
    bool ok = false;
    const double ev = value.split(' ').first().toDouble(&ok);
    if(!ok)
        return value;
    if(qFuzzyIsNull(ev))
        return QStringLiteral("0 EV");
    return QStringLiteral("%1%2 EV").arg(ev > 0 ? QStringLiteral("+") : QString())
                                   .arg(QString::number(ev, 'g', 2));
}

#endif // USE_LIBEXIF

} // namespace

QMap<QString, QString> readExifTags(const QString &filePath) {
    QMap<QString, QString> tags;
#ifdef USE_LIBEXIF
    ExifData *data = exif_data_new_from_file(filePath.toUtf8().constData());
    if(!data)
        return tags;

    auto add = [&tags](const QString &label, const QString &value) {
        const QString trimmed = localiseValue(value.trimmed());
        if(!trimmed.isEmpty())
            tags.insert(label, trimmed);
    };

    add(tr("Make"),       valueOf(data, EXIF_IFD_0, EXIF_TAG_MAKE));
    add(tr("Model"),      valueOf(data, EXIF_IFD_0, EXIF_TAG_MODEL));
    add(tr("Lens"),       valueOf(data, EXIF_IFD_EXIF, EXIF_TAG_LENS_MODEL));
    add(tr("Software"),   valueOf(data, EXIF_IFD_0, EXIF_TAG_SOFTWARE));
    add(tr("Taken"),      tidyDate(valueOf(data, EXIF_IFD_EXIF, EXIF_TAG_DATE_TIME_ORIGINAL)));

    add(tr("Exposure"),   valueOf(data, EXIF_IFD_EXIF, EXIF_TAG_EXPOSURE_TIME));
    add(tr("Aperture"),   valueOf(data, EXIF_IFD_EXIF, EXIF_TAG_FNUMBER));
    add(tr("ISO"),        valueOf(data, EXIF_IFD_EXIF, EXIF_TAG_ISO_SPEED_RATINGS));
    add(tr("Exposure bias"),
        exposureBias(valueOf(data, EXIF_IFD_EXIF, EXIF_TAG_EXPOSURE_BIAS_VALUE)));
    add(tr("Metering"),   valueOf(data, EXIF_IFD_EXIF, EXIF_TAG_METERING_MODE));
    add(tr("Flash"),      valueOf(data, EXIF_IFD_EXIF, EXIF_TAG_FLASH));
    add(tr("White balance"), valueOf(data, EXIF_IFD_EXIF, EXIF_TAG_WHITE_BALANCE));
    add(tr("Focal length"), valueOf(data, EXIF_IFD_EXIF, EXIF_TAG_FOCAL_LENGTH));
    add(tr("35mm equivalent"),
        valueOf(data, EXIF_IFD_EXIF, EXIF_TAG_FOCAL_LENGTH_IN_35MM_FILM));

    add(tr("Artist"),     valueOf(data, EXIF_IFD_0, EXIF_TAG_ARTIST));
    add(tr("Copyright"),  tidyCopyright(valueOf(data, EXIF_IFD_0, EXIF_TAG_COPYRIGHT)));
    add(tr("Description"), valueOf(data, EXIF_IFD_0, EXIF_TAG_IMAGE_DESCRIPTION));

    const QString latitude = decimalDegrees(valueOf(data, EXIF_IFD_GPS, static_cast<ExifTag>(EXIF_TAG_GPS_LATITUDE)),
                                            valueOf(data, EXIF_IFD_GPS, static_cast<ExifTag>(EXIF_TAG_GPS_LATITUDE_REF)));
    const QString longitude = decimalDegrees(valueOf(data, EXIF_IFD_GPS, static_cast<ExifTag>(EXIF_TAG_GPS_LONGITUDE)),
                                             valueOf(data, EXIF_IFD_GPS, static_cast<ExifTag>(EXIF_TAG_GPS_LONGITUDE_REF)));
    if(!latitude.isEmpty() && !longitude.isEmpty()) {
        tags.insert(tr("Location"), QStringLiteral("%1, %2").arg(latitude, longitude));
        const QString altitude = valueOf(data, EXIF_IFD_GPS, static_cast<ExifTag>(EXIF_TAG_GPS_ALTITUDE));
        if(!altitude.isEmpty()) {
            // libexif prints the rational ("12/1"); the meters are what matters
            const QString meters = altitude.split(',').first().split('/').first().trimmed();
            tags.insert(tr("Altitude"), tr("%1 m").arg(meters));
        }
    }

    exif_data_unref(data);
#else
    Q_UNUSED(filePath)
#endif
    return tags;
}
