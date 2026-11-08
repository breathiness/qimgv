#pragma once

#include <QByteArray>
#include <QFile>
#include <QString>
#include <QStringList>
#include <QVector>

// Minimal zip reader for comic archives (CBZ). Only what a viewer needs: list
// the entries and pull one out. Deflated and stored entries are supported,
// zip64 archives are not (a comic that large is not a thing).
class ZipArchive
{
public:
    bool open(const QString &path);
    bool isOpen() const { return mOpen; }

    // names in natural order: page2 comes before page10
    QStringList names() const;

    // decompressed contents of one entry, empty on error
    QByteArray read(const QString &name) const;

private:
    struct Entry {
        QString name;
        quint32 localOffset = 0;
        quint32 compressedSize = 0;
        quint16 method = 0;
    };

    bool mOpen = false;
    mutable QFile mFile;
    QVector<Entry> mEntries;
};

// the image entries of a comic, in natural reading order
QStringList comicImageEntries(const QStringList &names);
