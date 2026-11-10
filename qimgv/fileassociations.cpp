#include "fileassociations.h"
#include "fileformats.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#endif

namespace {

int categoryOf(const QString &label) {
    if(label.contains(QLatin1String("video"), Qt::CaseInsensitive)
       || label.contains(QLatin1String("stream"), Qt::CaseInsensitive))
        return FileAssociations::CategoryVideo;
    if(label.contains(QLatin1String("model"), Qt::CaseInsensitive))
        return FileAssociations::CategoryModels;
    if(label.contains(QLatin1String("document"), Qt::CaseInsensitive)
       || label.contains(QLatin1String("drawing"), Qt::CaseInsensitive)
       || label.contains(QLatin1String("comic"), Qt::CaseInsensitive))
        return FileAssociations::CategoryDocuments;
    return FileAssociations::CategoryImages;
}

QString labelForExt(const QString &ext) {
    for(int i = 0; i < kFileFormatCount; ++i) {
        if(ext == QLatin1String(kFileFormats[i].ext))
            return QString::fromLatin1(kFileFormats[i].label);
    }
    return QString();
}

int iconIdForExt(const QString &ext) {
    for(int i = 0; i < kFileFormatCount; ++i) {
        if(ext == QLatin1String(kFileFormats[i].ext))
            return kFileFormats[i].iconId;
    }
    return 0;
}

} // namespace

namespace FileAssociations {

QVector<FileFormatInfo> formats() {
    QVector<FileFormatInfo> list;
    list.reserve(kFileFormatCount);
    for(int i = 0; i < kFileFormatCount; ++i) {
        FileFormatInfo info;
        info.ext = QString::fromLatin1(kFileFormats[i].ext);
        info.label = QString::fromLatin1(kFileFormats[i].label);
        info.iconId = kFileFormats[i].iconId;
        info.category = categoryOf(info.label);
        list.append(info);
    }
    return list;
}

bool isSupported() {
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

bool isAdmin() {
#ifdef Q_OS_WIN
    return IsUserAnAdmin() != FALSE;
#else
    return false;
#endif
}

bool videoAvailable() {
    const QString dir = QCoreApplication::applicationDirPath();
    return QFileInfo(dir + QStringLiteral("/plugins/player_mpv.dll")).isFile()
           || QFileInfo(dir + QStringLiteral("/player_mpv.dll")).isFile()
           || QFileInfo(dir + QStringLiteral("/plugins/player_mpv.so")).isFile();
}

#ifdef Q_OS_WIN

namespace {

HKEY rootKey(bool machineWide) {
    return machineWide ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
}

QString nativePath(const QString &path) {
    return QDir::toNativeSeparators(path);
}

// The exe path as the command line wants it: quoted, with a "%1" for the file.
QString commandForExe() {
    return QStringLiteral("\"%1\" \"%2\"")
        .arg(nativePath(QCoreApplication::applicationFilePath()),
             QStringLiteral("%1"));
}

QString iconForId(int id) {
    return QStringLiteral("\"%1\",-%2")
        .arg(nativePath(QCoreApplication::applicationFilePath()))
        .arg(id);
}

bool createKey(HKEY root, const QString &subKey, HKEY *out) {
    const std::wstring path = subKey.toStdWString();
    HKEY key = nullptr;
    const LONG result = RegCreateKeyExW(root, path.c_str(), 0, nullptr, 0,
                                        KEY_WRITE, nullptr, &key, nullptr);
    if(result != ERROR_SUCCESS)
        return false;
    *out = key;
    return true;
}

bool setString(HKEY root, const QString &subKey, const wchar_t *name, const QString &value) {
    HKEY key = nullptr;
    if(!createKey(root, subKey, &key))
        return false;
    const std::wstring data = value.toStdWString();
    const LONG result = RegSetValueExW(key, name, 0, REG_SZ,
                                       reinterpret_cast<const BYTE *>(data.c_str()),
                                       static_cast<DWORD>((data.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

// OpenWithProgids entries are REG_NONE with no data, which QSettings cannot
// express; write it by hand.
bool setNone(HKEY root, const QString &subKey, const wchar_t *name) {
    HKEY key = nullptr;
    if(!createKey(root, subKey, &key))
        return false;
    const LONG result = RegSetValueExW(key, name, 0, REG_NONE, nullptr, 0);
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

void deleteValue(HKEY root, const QString &subKey, const wchar_t *name) {
    HKEY key = nullptr;
    if(RegOpenKeyExW(root, subKey.toStdWString().c_str(), 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
        return;
    RegDeleteValueW(key, name);
    RegCloseKey(key);
}

void deleteTree(HKEY root, const QString &subKey) {
    RegDeleteTreeW(root, subKey.toStdWString().c_str());
}

} // namespace

bool apply(const QStringList &enable, const QStringList &disable, bool machineWide) {
    const HKEY root = rootKey(machineWide);
    bool ok = true;

    for(const QString &ext : enable) {
        const QString progId = QStringLiteral("qimgv.") + ext;
        const QString classes = QStringLiteral("Software\\Classes\\") + progId;
        const QString label = labelForExt(ext);
        ok &= setString(root, classes, nullptr, label.isEmpty() ? ext.toUpper() : label);
        ok &= setString(root, classes + QStringLiteral("\\DefaultIcon"), nullptr,
                        iconForId(iconIdForExt(ext)));
        ok &= setString(root, classes + QStringLiteral("\\shell\\open\\command"), nullptr,
                        commandForExe());
        ok &= setNone(root, QStringLiteral("Software\\Classes\\.") + ext
                                + QStringLiteral("\\OpenWithProgids"),
                      reinterpret_cast<const wchar_t *>(progId.utf16()));
        ok &= setString(root, QStringLiteral("Software\\qimgv\\Capabilities\\FileAssociations"),
                        reinterpret_cast<const wchar_t *>((QStringLiteral(".") + ext).utf16()),
                        progId);
    }

    if(!enable.isEmpty() || !disable.isEmpty()) {
        // The capabilities block and the registration that publishes it.
        ok &= setString(root, QStringLiteral("Software\\qimgv\\Capabilities"), nullptr,
                        QStringLiteral("qimgv image viewer"));
        ok &= setString(root, QStringLiteral("Software\\qimgv\\Capabilities"),
                        L"ApplicationName", QStringLiteral("qimgv"));
        ok &= setString(root, QStringLiteral("Software\\qimgv\\Capabilities"),
                        L"ApplicationDescription",
                        QStringLiteral("Fast and configurable image viewer."));
        ok &= setString(root, QStringLiteral("Software\\RegisteredApplications"),
                        L"qimgv", QStringLiteral("Software\\qimgv\\Capabilities"));
    }

    for(const QString &ext : disable) {
        deleteValue(root, QStringLiteral("Software\\qimgv\\Capabilities\\FileAssociations"),
                    reinterpret_cast<const wchar_t *>((QStringLiteral(".") + ext).utf16()));
        deleteValue(root, QStringLiteral("Software\\Classes\\.") + ext
                              + QStringLiteral("\\OpenWithProgids"),
                    reinterpret_cast<const wchar_t *>((QStringLiteral("qimgv.") + ext).utf16()));
        // Leaves the extension itself alone: only the ProgId this app owns goes.
        deleteTree(root, QStringLiteral("Software\\Classes\\qimgv.") + ext);
    }

    return ok;
}

QStringList registered(bool machineWide) {
    const HKEY root = rootKey(machineWide);
    QStringList result;
    for(int i = 0; i < kFileFormatCount; ++i) {
        const QString ext = QString::fromLatin1(kFileFormats[i].ext);
        const QString subKey = QStringLiteral("Software\\Classes\\qimgv.") + ext;
        HKEY key = nullptr;
        if(RegOpenKeyExW(root, subKey.toStdWString().c_str(), 0, KEY_READ, &key) == ERROR_SUCCESS) {
            RegCloseKey(key);
            if(!result.contains(ext))
                result.append(ext);
        }
    }
    return result;
}

void notifyShell() {
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

#else

bool apply(const QStringList &, const QStringList &, bool) {
    return false;
}

QStringList registered(bool) {
    return QStringList();
}

void notifyShell() {}

#endif

} // namespace FileAssociations
