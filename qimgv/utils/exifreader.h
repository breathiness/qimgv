#ifndef EXIFREADER_H
#define EXIFREADER_H

#include <QMap>
#include <QString>

// Reads the tags a camera writes into a photo through libexif, and hands them over
// already formatted: "1/250 sec.", "f/4.0", "31.230416, 121.473701".
// Empty when the file carries no exif block, or when the build has no libexif.
QMap<QString, QString> readExifTags(const QString &filePath);

#endif // EXIFREADER_H
