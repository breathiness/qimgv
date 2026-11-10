#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// One row of the generated table in fileformats.h, plus the category the
// preferences page groups it under.
struct FileFormatInfo {
    QString ext;
    QString label;   // "JPEG image"
    int iconId = 0;  // resource id of the icon embedded by fileformats.rc
    int category = 0;
};

namespace FileAssociations {

enum Category {
    CategoryImages = 0,
    CategoryVideo,
    CategoryDocuments,
    CategoryModels,
    CategoryCount
};

// Everything the app can open, in table order.
QVector<FileFormatInfo> formats();

// The registry bits only exist on Windows; the table above does not depend on
// them, so the preferences page can still show what the build supports.
bool isSupported();
bool isAdmin();

// Writes (enable) or removes (disable) the qimgv ProgId for each extension.
// machineWide targets HKLM and needs administrator rights; the caller is
// expected to have warned the user about that.
bool apply(const QStringList &enable, const QStringList &disable, bool machineWide);

// Extensions that currently have a qimgv ProgId in that hive.
QStringList registered(bool machineWide);

// True when the optional video plugin is installed next to the executable.
// The build is compiled with video support either way, so this is a runtime
// check of what the installation actually ships.
bool videoAvailable();

// Tells Explorer that file associations changed, so it re-reads the icons.
void notifyShell();

} // namespace FileAssociations
