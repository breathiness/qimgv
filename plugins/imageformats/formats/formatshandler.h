#pragma once

#include <QImageIOHandler>
#include <QImage>
#include <QSize>
#include <QVariant>
#include <QString>

// Reads the formats qimgv cannot get from Qt itself: HEIC/HEIF/AVIF (libheif),
// JPEG XL (libjxl), camera raw (libraw) and PSD/PSB (parsed here).
//
// All of them live in this plugin so the application itself stays free of those
// dependencies: dropping the plugin out of imageformats/ only removes the
// formats, the viewer still starts and runs.
class FormatsHandler : public QImageIOHandler
{
public:
    FormatsHandler();

    bool canRead() const override;
    bool read(QImage *image) override;
    bool supportsOption(ImageOption option) const override;
    QVariant option(ImageOption option) const override;

    int imageCount() const override { return 1; }
    int currentImageNumber() const override { return 0; }

    // in-memory decode is not supported, the decoders work on files
    static QStringList supportedSuffixes();
    static bool isSupportedSuffix(const QString &suffix);

private:
    QString filePath() const;
    QString resolvedSuffix() const;
    QImage decode(const QString &path, const QString &suffix, const QSize &target) const;
};
