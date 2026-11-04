#include "pdfpagemodel.h"

#include "utils/pdfprovider.h"

#include <QMutexLocker>
#include <algorithm>

PdfPageModel::~PdfPageModel() {
    close();
}

bool PdfPageModel::open(const QString &path, QString *error) {
    close();
    QString message;
    mDocument = PdfProvider::instance().open(path, &message);
    if(!mDocument) {
        if(error)
            *error = message;
        return false;
    }
    mNeedsPassword = PdfProvider::instance().isEncrypted(mDocument.get());
    if(mNeedsPassword)
        return true;   // poppler reads a locked document's pages unsafely
    readPageSizes();
    if(mPageCount <= 0 && !mNeedsPassword) {
        if(error)
            *error = QStringLiteral("no pages");
        close();
        return false;
    }
    return true;
}

void PdfPageModel::close() {
    mDocument.reset();
    mPageCount = 0;
    mNeedsPassword = false;
    mPageSizes.clear();
    mMaxPageSize = QSizeF();
    QMutexLocker lock(&mCacheMutex);
    mCache.clear();
}

void PdfPageModel::readPageSizes() {
    // Poppler hands out a document whose page tree is empty until it is unlocked,
    // and reading pages through it crashes: wait for the password.
    if(mNeedsPassword)
        return;
    mPageSizes.clear();
    mMaxPageSize = QSizeF();
    mPageCount = mDocument ? PdfProvider::instance().pageCount(mDocument.get()) : 0;
    if(mPageCount <= 0)
        return;
    mPageSizes.reserve(mPageCount);
    for(int page = 0; page < mPageCount; page++) {
        const QSizeF size = PdfProvider::instance().pageSize(mDocument.get(), page);
        mPageSizes.append(size);
        mMaxPageSize = QSizeF(qMax(mMaxPageSize.width(), size.width()),
                              qMax(mMaxPageSize.height(), size.height()));
    }
}

QSizeF PdfPageModel::pageSize(int page) const {
    if(page < 0 || page >= mPageSizes.size())
        return QSizeF();
    return mPageSizes.at(page);
}

bool PdfPageModel::unlock(const QString &password) {
    if(!mDocument || !mNeedsPassword)
        return false;
    if(!PdfProvider::instance().unlock(mDocument.get(), password))
        return false;
    mNeedsPassword = false;
    readPageSizes();
    QMutexLocker lock(&mCacheMutex);
    mCache.clear();
    return mPageCount > 0;
}

QImage PdfPageModel::renderPage(int page, QSize target) const {
    if(!mDocument || target.isEmpty())
        return QImage();
    // Poppler wants one document used from one thread at a time
    QMutexLocker lock(&mRenderMutex);
    return PdfProvider::instance().renderPage(mDocument.get(), page, target);
}

QImage PdfPageModel::cachedPage(int page, QSize target) const {
    QMutexLocker lock(&mCacheMutex);
    auto it = mCache.find(page);
    if(it == mCache.end())
        return QImage();
    if(it->target != target)
        return QImage();
    it->stamp = ++mStamp;
    return it->image;
}

void PdfPageModel::cachePage(int page, const QImage &image, QSize target) const {
    if(image.isNull())
        return;
    QMutexLocker lock(&mCacheMutex);
    CachedPage entry;
    entry.image = image;
    entry.target = target;
    entry.stamp = ++mStamp;
    mCache.insert(page, entry);
    // drop the pages that were looked at longest ago
    while(mCache.size() > mCacheLimit) {
        auto oldest = mCache.begin();
        for(auto it = mCache.begin(); it != mCache.end(); ++it) {
            if(it->stamp < oldest->stamp)
                oldest = it;
        }
        mCache.erase(oldest);
    }
}
