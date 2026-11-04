#ifndef IMAGEINFO_H
#define IMAGEINFO_H

#include <QImage>
#include <QList>
#include <QMap>
#include <QString>

struct ImageInfoEntry {
    QString label;
    QString value;
};

struct ImageInfoSection {
    QString title;
    QList<ImageInfoEntry> entries;
};

// Everything the application can say about a picture without a metadata library:
// the file on disk, the decoded pixels, and the text some formats carry inside
// (png tEXt chunks, a jpeg comment). "extraTags" takes whatever else a build can
// read - exif, when the library for it is available - and becomes the last section.
QList<ImageInfoSection> gatherImageInfo(const QString &filePath,
                                        const QImage &image,
                                        const QMap<QString, QString> &extraTags = {});

#endif // IMAGEINFO_H
