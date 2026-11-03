#pragma once

#include <QHash>
#include <QImage>
#include <QMutex>
#include <QSizeF>
#include <QString>
#include <QVector>
#include <memory>

// The shape of a pdf for a viewer that paints pages one by one: how many there
// are, how big each one is, and a small cache of pages that were already drawn.
//
// Rendering goes through the optional provider plugin. Poppler is not thread safe
// for a single document, so every render is serialised here; callers may ask from
// several threads and the calls simply queue up.
class PdfPageModel {
public:
    PdfPageModel() = default;
    ~PdfPageModel();

    bool open(const QString &path, QString *error);
    void close();
    bool isOpen() const { return mDocument != nullptr; }

    int pageCount() const { return mPageCount; }
    // page size in points; empty when the page does not exist
    QSizeF pageSize(int page) const;
    // the largest page, used for fit calculations
    QSizeF maxPageSize() const { return mMaxPageSize; }

    bool isEncrypted() const { return mNeedsPassword; }
    bool unlock(const QString &password);

    // Renders `page` into `target` device pixels. Returns a null image on failure.
    // Thread safe; the work is serialised inside.
    QImage renderPage(int page, QSize target) const;

    // Cache access, used by the view on the gui thread. A cached page only counts
    // when it was drawn for the same target size.
    QImage cachedPage(int page, QSize target) const;
    void cachePage(int page, const QImage &image, QSize target) const;

private:
    void readPageSizes();

    std::shared_ptr<void> mDocument;
    int mPageCount = 0;
    bool mNeedsPassword = false;
    QVector<QSizeF> mPageSizes;
    QSizeF mMaxPageSize;

    mutable QMutex mRenderMutex;

    struct CachedPage {
        QImage image;
        QSize target;
        quint64 stamp = 0;
    };
    mutable QMutex mCacheMutex;
    mutable QHash<int, CachedPage> mCache;
    mutable quint64 mStamp = 0;
    // pages kept around; a few screens of reading plus the neighbours
    int mCacheLimit = 8;
};
