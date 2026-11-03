#pragma once

#include <QImage>
#include <QSizeF>
#include <QString>
#include <QLibrary>
#include <memory>

// Front end for the optional PDF provider plugin (plugins/pdf, or the "pdf" folder
// next to the executable).
//
// The plugin links Poppler and exposes a small C interface; nothing of Poppler
// leaks into the app. When the plugin is not installed every call here simply
// reports "not available", and pdf files are not treated as openable.
class PdfProvider {
public:
    static PdfProvider &instance();

    // true when the plugin was found and its ABI matches
    bool available() const;
    // where the plugin was looked for, for error messages
    QString searchPath() const;

    // Opens a document. The returned handle owns the Poppler document and closes
    // it when released; nullptr means it could not be opened, and error says why.
    std::shared_ptr<void> open(const QString &path, QString *error) const;
    int pageCount(void *document) const;
    QSizeF pageSize(void *document, int page) const;
    bool isEncrypted(void *document) const;
    bool unlock(void *document, const QString &password) const;
    // Renders one page scaled into target; returns a null image on failure
    QImage renderPage(void *document, int page, QSize target) const;

private:
    PdfProvider();
    void loadPluginlib();
    bool resolveSymbols();

    mutable QLibrary mLibrary;
    mutable bool mLoadAttempted = false;
    mutable bool mReady = false;
    QString mSearchPath;

    // plugin entry points
    int (*mAbiVersion)() = nullptr;
    void *(*mOpen)(const char *, const char **) = nullptr;
    void (*mClose)(void *) = nullptr;
    int (*mPageCount)(void *) = nullptr;
    int (*mPageSize)(void *, int, double *, double *) = nullptr;
    int (*mIsEncrypted)(void *) = nullptr;
    int (*mUnlock)(void *, const char *) = nullptr;
    int (*mRender)(void *, int, int, int, unsigned char *) = nullptr;
};
