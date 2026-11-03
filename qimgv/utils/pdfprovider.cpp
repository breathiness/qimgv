#include "pdfprovider.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QThread>
#include <cstring>

namespace {

constexpr int kExpectedAbi = 1;

#ifdef Q_OS_WIN
const QString kPluginName = QStringLiteral("pdfprovider.dll");
#else
const QString kPluginName = QStringLiteral("libpdfprovider.so");
#endif

}  // namespace

PdfProvider &PdfProvider::instance() {
    static PdfProvider provider;
    return provider;
}

PdfProvider::PdfProvider() {
    loadPluginlib();
}

void PdfProvider::loadPluginlib() {
    if(mLoadAttempted)
        return;
    mLoadAttempted = true;

    // The pack is a folder next to the executable, like the video plugin; the
    // plugin's own libraries live beside it so the loader finds them through the
    // DLL search path (the app adds that folder before calling in).
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + QStringLiteral("/pdf/") + kPluginName,
        appDir + QStringLiteral("/plugins/") + kPluginName,
        appDir + QStringLiteral("/") + kPluginName,
    };
    mSearchPath = candidates.join(QStringLiteral(", "));

    for(const QString &candidate : candidates) {
        if(!QFileInfo::exists(candidate))
            continue;
        // Windows does not look in the plugin's own folder for its dependencies,
        // and the pack keeps poppler and friends beside the plugin, so put that
        // folder on the dll search path for this process
        const QString packDir = QFileInfo(candidate).absolutePath();
#ifdef Q_OS_WIN
        const QString currentPath = qEnvironmentVariable("PATH");
        if(!currentPath.startsWith(packDir, Qt::CaseInsensitive))
            qputenv("PATH", (packDir + QLatin1Char(';') + currentPath).toUtf8());
#else
        Q_UNUSED(packDir)
#endif
        mLibrary.setFileName(candidate);
        if(!mLibrary.load())
            continue;
        if(resolveSymbols())
            mReady = true;
        if(mReady)
            return;
        mLibrary.unload();
    }
}

bool PdfProvider::resolveSymbols() {
    mAbiVersion = reinterpret_cast<int (*)()>(mLibrary.resolve("qimgv_pdf_abi_version"));
    mOpen = reinterpret_cast<void *(*)(const char *, const char **)>(mLibrary.resolve("qimgv_pdf_open"));
    mClose = reinterpret_cast<void (*)(void *)>(mLibrary.resolve("qimgv_pdf_close"));
    mPageCount = reinterpret_cast<int (*)(void *)>(mLibrary.resolve("qimgv_pdf_page_count"));
    mPageSize = reinterpret_cast<int (*)(void *, int, double *, double *)>(mLibrary.resolve("qimgv_pdf_page_size"));
    mIsEncrypted = reinterpret_cast<int (*)(void *)>(mLibrary.resolve("qimgv_pdf_is_encrypted"));
    mUnlock = reinterpret_cast<int (*)(void *, const char *)>(mLibrary.resolve("qimgv_pdf_unlock"));
    mRender = reinterpret_cast<int (*)(void *, int, int, int, unsigned char *)>(mLibrary.resolve("qimgv_pdf_render"));
    if(!mAbiVersion || !mOpen || !mClose || !mPageCount || !mPageSize || !mIsEncrypted
       || !mUnlock || !mRender) {
        return false;
    }
    return mAbiVersion() == kExpectedAbi;
}

bool PdfProvider::available() const {
    return mReady;
}

QString PdfProvider::searchPath() const {
    return mSearchPath;
}

std::shared_ptr<void> PdfProvider::open(const QString &path, QString *error) const {
    if(error)
        error->clear();
    if(!mReady) {
        if(error)
            *error = QStringLiteral("pdf plugin not installed");
        return nullptr;
    }
    const QByteArray utf8 = path.toUtf8();
    const char *pluginError = nullptr;
    void *document = mOpen(utf8.constData(), &pluginError);
    if(!document) {
        if(error)
            *error = pluginError ? QString::fromUtf8(pluginError) : QStringLiteral("cannot open");
        return nullptr;
    }
    // the deleter closes the Poppler document with the plugin still loaded
    return std::shared_ptr<void>(document, [this](void *handle) {
        if(handle)
            mClose(handle);
    });
}

int PdfProvider::pageCount(void *document) const {
    if(!mReady || !document)
        return 0;
    return mPageCount(document);
}

QSizeF PdfProvider::pageSize(void *document, int page) const {
    if(!mReady || !document)
        return QSizeF();
    double width = 0;
    double height = 0;
    if(mPageSize(document, page, &width, &height) != 0)
        return QSizeF();
    return QSizeF(width, height);
}

bool PdfProvider::isEncrypted(void *document) const {
    if(!mReady || !document)
        return false;
    return mIsEncrypted(document) != 0;
}

bool PdfProvider::unlock(void *document, const QString &password) const {
    if(!mReady || !document)
        return false;
    return mUnlock(document, password.toUtf8().constData()) == 0;
}

QImage PdfProvider::renderPage(void *document, int page, QSize target) const {
    if(!mReady || !document || target.width() <= 0 || target.height() <= 0)
        return QImage();
    // the plugin writes R,G,B,A bytes, which is Format_RGBA8888's layout
    QImage image(target, QImage::Format_RGBA8888);
    if(image.isNull())
        return QImage();
    if(mRender(document, page, target.width(), target.height(),
               reinterpret_cast<unsigned char *>(image.bits())) != 0) {
        return QImage();
    }
    return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}
