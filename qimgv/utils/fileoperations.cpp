#include "fileoperations.h"

#include <QThread>

#ifdef Q_OS_WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

QString FileOperations::generateHash(const QString &str) {
    return QString(QCryptographicHash::hash(str.toUtf8(), QCryptographicHash::Md5).toHex());
}

void FileOperations::removeFile(const QString &filePath, FileOpResult &result) {
    QFileInfo file(filePath);
    if(!file.exists()) {
        result = FileOpResult::SOURCE_DOES_NOT_EXIST;
#ifdef Q_OS_WIN32
    } else if(!file.isWritable()) {
        result = FileOpResult::SOURCE_NOT_WRITABLE;
#endif
    } else {
        if(QFile::remove(filePath))
            result = FileOpResult::SUCCESS;
        else
            result = FileOpResult::OTHER_ERROR;
    }
    return;
}

// non-recursive
void FileOperations::removeDir(const QString &dirPath, bool recursive, FileOpResult &result) {
    QDir dir(dirPath);
    if(!dir.exists()) {
        result = FileOpResult::SOURCE_DOES_NOT_EXIST;
    } else {
        if(recursive ? dir.removeRecursively() : dir.rmdir(dirPath))
            result = FileOpResult::SUCCESS;
        else if(!recursive && !dir.isEmpty())
            result = FileOpResult::DIRECTORY_NOT_EMPTY;
        else
            result = FileOpResult::OTHER_ERROR;
    }
    return;
}

QString FileOperations::decodeResult(const FileOpResult &result) {
    switch(result) {
    case FileOpResult::SUCCESS:
        return QObject::tr("Operation completed succesfully.");
    case FileOpResult::DESTINATION_FILE_EXISTS:
        return QObject::tr("Destination file exists.");
    case FileOpResult::DESTINATION_DIR_EXISTS:
        return QObject::tr("Destination directory exists.");
    case FileOpResult::SOURCE_NOT_WRITABLE:
        return QObject::tr("Source file is not writable.");
    case FileOpResult::DESTINATION_NOT_WRITABLE:
        return QObject::tr("Destination is not writable.");
    case FileOpResult::SOURCE_DOES_NOT_EXIST:
        return QObject::tr("Source file does not exist.");
    case FileOpResult::DESTINATION_DOES_NOT_EXIST:
        return QObject::tr("Destination does not exist.");
    case FileOpResult::DIRECTORY_NOT_EMPTY:
        return QObject::tr("Directory is not empty.");
    case FileOpResult::NOTHING_TO_DO:
        return QObject::tr("Nothing to do.");
    case FileOpResult::OTHER_ERROR:
        return QObject::tr("Other error.");
    }
    return nullptr;
}

void FileOperations::copyFileTo(const QString &srcFilePath, const QString &destDirPath, bool force, FileOpResult &result) {
    QFileInfo srcFile(srcFilePath);
    QString tmpPath;
    bool exists = false;
    // error checks
    if(destDirPath == srcFile.absolutePath()) {
        result = FileOpResult::NOTHING_TO_DO;
        return;
    }
    if(!srcFile.exists()) {
        result = FileOpResult::SOURCE_DOES_NOT_EXIST;
        return;
    }
    QFileInfo destDir(destDirPath);
    if(!destDir.exists()) {
        result = FileOpResult::DESTINATION_DOES_NOT_EXIST;
        return;
    }
    if(!destDir.isWritable()) {
        result = FileOpResult::DESTINATION_NOT_WRITABLE;
        return;
    }
    QFileInfo destFile(destDirPath + "/" + srcFile.fileName());
    if(destFile.exists()) {
#ifdef Q_OS_WIN32
        if(!destFile.isWritable()) {
            result = FileOpResult::DESTINATION_NOT_WRITABLE;
            return;
        }
#endif
        if(destFile.isDir()) {
            result = FileOpResult::DESTINATION_DIR_EXISTS;
            return;
        }
        if(!force) {
            result = FileOpResult::DESTINATION_FILE_EXISTS;
            return;
        }
        // remove just in case it exists
        tmpPath = destFile.absoluteFilePath() + "_" + generateHash(destFile.absoluteFilePath());
        QFile::remove(tmpPath);
        // move backup
        QFile::rename(destFile.absoluteFilePath(), tmpPath);
        exists = true;
    }
    // copy
    auto srcModTime = srcFile.lastModified();
    auto srcReadTime = srcFile.lastRead();
    if(QFile::copy(srcFile.absoluteFilePath(), destFile.absoluteFilePath())) {
        result = FileOpResult::SUCCESS;
        // restore timestamps
        QFile dstF(destFile.absoluteFilePath());
        dstF.open(QIODevice::ReadWrite);
        dstF.setFileTime(srcModTime, QFileDevice::FileModificationTime);
        dstF.setFileTime(srcReadTime, QFileDevice::FileAccessTime);
        dstF.close();
        // ok; remove the backup
        if(exists)
            QFile::remove(tmpPath);
    } else {
        result = FileOpResult::OTHER_ERROR;
        // fail; revert
        QFile::rename(tmpPath, destFile.absoluteFilePath());
    }
    return;
}

void FileOperations::moveFileTo(const QString &srcFilePath, const QString &destDirPath, bool force, FileOpResult &result) {
    QFileInfo srcFile(srcFilePath);
    QString tmpPath;
    bool exists = false;
    // error checks
    if(destDirPath == srcFile.absolutePath()) {
        result = FileOpResult::NOTHING_TO_DO;
        return;
    }
    if(!srcFile.exists()) {
        result = FileOpResult::SOURCE_DOES_NOT_EXIST;
        return;
    }
    #ifdef Q_OS_WIN32
    if(!srcFile.isWritable()) {
        result = FileOpResult::SOURCE_NOT_WRITABLE;
        return;
    }
    #endif
    QFileInfo destDir(destDirPath);
    if(!destDir.exists()) {
        result = FileOpResult::DESTINATION_DOES_NOT_EXIST;
        return;
    }
    if(!destDir.isWritable()) {
        result = FileOpResult::DESTINATION_NOT_WRITABLE;
        return;
    }
    QFileInfo destFile(destDirPath + "/" + srcFile.fileName());
    if(destFile.exists()) {
#ifdef Q_OS_WIN32
        if(!destFile.isWritable()) {
            result = FileOpResult::DESTINATION_NOT_WRITABLE;
            return;
        }
#endif
        if(destFile.isDir()) {
            result = FileOpResult::DESTINATION_DIR_EXISTS;
            return;
        }
        if(!force) {
            result = FileOpResult::DESTINATION_FILE_EXISTS;
            return;
        }
        tmpPath = destFile.absoluteFilePath() + "_" + generateHash(destFile.absoluteFilePath());
        QFile::remove(tmpPath);
        // move backup
        QFile::rename(destFile.absoluteFilePath(), tmpPath);
        exists = true;
    }
    // move
    auto srcModTime = srcFile.lastModified();
    auto srcReadTime = srcFile.lastRead();
    if(QFile::copy(srcFile.absoluteFilePath(), destFile.absoluteFilePath())) {
        // remove original file
        FileOpResult removeResult;
        removeFile(srcFile.absoluteFilePath(), removeResult);
        if(removeResult == FileOpResult::SUCCESS) {
            // OK
            result = FileOpResult::SUCCESS;
            // restore timestamps
            QFile dstF(destFile.absoluteFilePath());
            dstF.open(QIODevice::ReadWrite);
            // dstF.setFileTime(srcBirthTime, QFileDevice::FileBirthTime); // TODO: does not work (linux)
            dstF.setFileTime(srcModTime, QFileDevice::FileModificationTime);
            dstF.setFileTime(srcReadTime, QFileDevice::FileAccessTime);
            dstF.close();
            // remove backup
            if(exists)
                QFile::remove(tmpPath);
            return;
        }
        // revert on failure
        result = FileOpResult::SOURCE_NOT_WRITABLE;
        if(QFile::remove(destFile.absoluteFilePath()))
            result = FileOpResult::OTHER_ERROR;
    } else {
        // could not COPY
        result = FileOpResult::OTHER_ERROR;
    }
    if(exists) // failed; revert backup
        QFile::rename(tmpPath, destFile.absoluteFilePath());
    return;
}

void FileOperations::rename(const QString &srcFilePath, const QString &newName, bool force, FileOpResult &result) {
    QFileInfo srcFile(srcFilePath);
    QString tmpPath;
    // error checks
    if(!srcFile.exists()) {
        result = FileOpResult::SOURCE_DOES_NOT_EXIST;
        return;
    }
#ifdef Q_OS_WIN32
    if(!srcFile.isWritable()) {
        result = FileOpResult::SOURCE_NOT_WRITABLE;
        return;
    }
#endif
    if(newName.isEmpty() || newName == srcFile.fileName()) {
        result = FileOpResult::NOTHING_TO_DO;
        return;
    }
    QString newFilePath = srcFile.absolutePath() + "/" + newName;
    QFileInfo destFile(newFilePath);
    if(destFile.exists()) {
#ifdef Q_OS_WIN32
        if(!destFile.isWritable())
            result = FileOpResult::DESTINATION_NOT_WRITABLE;
#endif
        if(destFile.isDir()) {
            result = FileOpResult::DESTINATION_DIR_EXISTS;
            return;
        }
        if(!force) {
            result = FileOpResult::DESTINATION_FILE_EXISTS;
            return;
        }
        tmpPath = newFilePath + "_" + generateHash(newFilePath);
        QFile::remove(tmpPath);
        // move dest file
        QFile::rename(newFilePath, tmpPath);
    }
    if(QFile::rename(srcFile.filePath(), newFilePath)) {
        result = FileOpResult::SUCCESS;
        if(QFile::exists(tmpPath))
            QFile::remove(tmpPath);
    } else {
        result = FileOpResult::OTHER_ERROR;
        // restore dest file
        QFile::rename(tmpPath, newFilePath);
    }
}

void FileOperations::moveToTrash(const QString &filePath, FileOpResult &result) {
    QFileInfo file(filePath);
    if(!file.exists()) {
        result = FileOpResult::SOURCE_DOES_NOT_EXIST;
#ifdef Q_OS_WIN32
    } else if(!file.isWritable()) {
        result = FileOpResult::SOURCE_NOT_WRITABLE;
#endif
    } else {
        // A shell delete can report success while the item is still on disk - an
        // IFileOperation that is only queued, a volume whose Recycle Bin hands the
        // item back. Trusting the return value alone drops the row from the view
        // and the file reappears on the next listing, so the outcome is checked
        // against the file system instead.
        const bool moved = moveToTrashImpl(filePath);
        bool gone = false;
        for(int attempt = 0; moved && attempt < 4 && !gone; ++attempt) {
            gone = !QFileInfo::exists(filePath);
            if(!gone)
                QThread::msleep(50);
        }
        result = gone ? FileOpResult::SUCCESS : FileOpResult::OTHER_ERROR;
    }
    return;
}

#ifdef Q_OS_WIN32
// Shell delete with FOF_ALLOWUNDO: the item goes to the Recycle Bin (or is
// removed for good on a volume that has none) and no COM object of our own is
// needed. SHFileOperationW reports a per-item code, so a refused delete is not
// mistaken for a successful one.
static bool recycleWithShell(const QString &filePath) {
    std::wstring from = QDir::toNativeSeparators(filePath).toStdWString();
    from.push_back(L'\0');  // SHFileOperation wants a double terminated list
    from.push_back(L'\0');

    SHFILEOPSTRUCTW operation = {};
    operation.wFunc = FO_DELETE;
    operation.pFrom = from.c_str();
    operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
    return SHFileOperationW(&operation) == 0 && !operation.fAnyOperationsAborted;
}

// QFile::moveToTrash() is tried first; the shell call is the second attempt for
// the cases where Qt's helper reports failure on a file the shell can delete.
// Neither path ever falls back to a permanent delete: if the shell refuses, the
// caller reports an error.
bool FileOperations::moveToTrashImpl(const QString &filePath) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    if(QFile::moveToTrash(filePath))
        return true;
#endif
    return recycleWithShell(filePath);
}
#elif QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
bool FileOperations::moveToTrashImpl(const QString &filePath) {
    return QFile::moveToTrash(filePath);
}
#else
#ifdef Q_OS_LINUX
bool FileOperations::moveToTrashImpl(const QString &filePath) {
    #ifdef QT_GUI_LIB
    bool TrashInitialized = false;
    QString TrashPath;
    QString TrashPathInfo;
    QString TrashPathFiles;
    if(!TrashInitialized) {
        QStringList paths;
        const char* xdg_data_home = getenv( "XDG_DATA_HOME" );
        if(xdg_data_home) {
            qDebug() << "XDG_DATA_HOME not yet tested";
            QString xdgTrash( xdg_data_home );
            paths.append(xdgTrash + "/Trash");
        }
        QString home = QStandardPaths::writableLocation( QStandardPaths::HomeLocation );
        paths.append( home + "/.local/share/Trash" );
        paths.append( home + "/.trash" );
        foreach( QString path, paths ){
            if( TrashPath.isEmpty() ){
                QDir dir( path );
                if( dir.exists() ){
                    TrashPath = path;
                }
            }
        }
        if( TrashPath.isEmpty() )
            qDebug() << "Can`t detect trash folder";
        TrashPathInfo = TrashPath + "/info";
        TrashPathFiles = TrashPath + "/files";
        if( !QDir( TrashPathInfo ).exists() || !QDir( TrashPathFiles ).exists() )
            qDebug() << "Trash doesn`t look like FreeDesktop.org Trash specification";
        TrashInitialized = true;
    }
    QFileInfo original( filePath );
    if( !original.exists() )
        qDebug() << "File doesn`t exist, cant move to trash";
    QString info;
    info += "[Trash Info]\nPath=";
    info += original.absoluteFilePath();
    info += "\nDeletionDate=";
    info += QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss");
    info += "\n";
    QString trashname = original.fileName();
    QString infopath = TrashPathInfo + "/" + trashname + ".trashinfo";
    QString filepath = TrashPathFiles + "/" + trashname;
    int nr = 1;
    while( QFileInfo( infopath ).exists() || QFileInfo( filepath ).exists() ){
        nr++;
        trashname = original.baseName() + "." + QString::number( nr );
        if( !original.completeSuffix().isEmpty() ){
            trashname += QString( "." ) + original.completeSuffix();
        }
        infopath = TrashPathInfo + "/" + trashname + ".trashinfo";
        filepath = TrashPathFiles + "/" + trashname;
    }
    QDir dir;
    if( !dir.rename( original.absoluteFilePath(), filepath ) ){
        qDebug() << "move to trash failed";
    }
    QFile infoFile(infopath);
    infoFile.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&infoFile);
    out.setCodec("UTF-8");
    out.setGenerateByteOrderMark(false);
    out << info;
    infoFile.close();
    #else
    Q_UNUSED( file );
    qDebug() << "Trash in server-mode not supported";
    #endif
    return true;
}
#endif

#ifdef Q_OS_WIN32
bool FileOperations::moveToTrashImpl(const QString &file) {
    QFileInfo fileinfo( file );
    if( !fileinfo.exists() )
        return false;
    WCHAR* from = (WCHAR*) calloc((size_t)fileinfo.absoluteFilePath().length() + 2, sizeof(WCHAR));
    fileinfo.absoluteFilePath().toWCharArray(from);    
    SHFILEOPSTRUCTW fileop;
    memset( &fileop, 0, sizeof( fileop ) );
    fileop.wFunc = FO_DELETE;
    fileop.pFrom = from;
    fileop.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    int rv = SHFileOperationW( &fileop );
    free(from);
    if( 0 != rv ){
        qDebug() << rv << QString::number( rv ).toInt( nullptr, 8 );
        qDebug() << "move to trash failed";
        return false;
    }
    return true;
}
#endif

#ifdef Q_OS_MAC
bool FileOperations::moveToTrashImpl(const QString &file) { // todo
    return false;
}
#endif
#endif
