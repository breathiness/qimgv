#include "pdfview.h"

#include "pdfpagemodel.h"

#include <QPainter>
#include <QTimer>
#include <QPaintEvent>
#include <QScrollBar>
#include <QWheelEvent>
#include <algorithm>

// One worker thread, one document: Poppler cannot render the same document from
// two threads, so a queue of pending pages is what keeps the gui responsive.
class PdfView::RenderThread : public QThread {
    Q_OBJECT
public:
    explicit RenderThread(std::shared_ptr<PdfPageModel> model)
        : mModel(std::move(model)) {}

    // Replaces what is wanted; called from the gui thread.
    void setWanted(const QList<QPair<int, QSize>> &wanted) {
        QMutexLocker lock(&mMutex);
        mWanted = wanted;
        mGeneration++;
        mWait.wakeAll();
    }

    void stop() {
        QMutexLocker lock(&mMutex);
        mStopping = true;
        mWait.wakeAll();
    }

signals:
    void pageReady(int page, const QImage &image, QSize target, quint64 generation);

protected:
    void run() override {
        forever {
            QPair<int, QSize> job;
            quint64 generation = 0;
            {
                QMutexLocker lock(&mMutex);
                while(mWanted.isEmpty() && !mStopping)
                    mWait.wait(&mMutex);
                if(mStopping)
                    return;
                job = mWanted.takeFirst();
                generation = mGeneration;
            }
            const QImage image = mModel->renderPage(job.first, job.second);
            QMutexLocker lock(&mMutex);
            // a newer request came in while this page rendered: the result is stale
            if(generation != mGeneration)
                continue;
            emit pageReady(job.first, image, job.second, generation);
        }
    }

private:
    std::shared_ptr<PdfPageModel> mModel;
    QMutex mMutex;
    QWaitCondition mWait;
    QList<QPair<int, QSize>> mWanted;
    quint64 mGeneration = 0;
    bool mStopping = false;
};

PdfView::PdfView(QWidget *parent)
    : QAbstractScrollArea(parent)
{
    setFrameShape(QFrame::NoFrame);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    viewport()->setAutoFillBackground(true);
    setFocusPolicy(Qt::StrongFocus);
    setBackgroundRole(QPalette::Dark);
    refreshColors();
}

PdfView::~PdfView() {
    if(mWorker) {
        mWorker->stop();
        mWorker->wait(3000);
        delete mWorker;
        mWorker = nullptr;
    }
}

void PdfView::refreshColors() {
    // same background as the image viewer, so switching between a picture and a
    // document does not change the look of the window
    QColor background = mFullscreen ? settings->colorScheme().background_fullscreen
                                    : settings->colorScheme().background;
    background.setAlphaF(1.0);
    QPalette pal = palette();
    pal.setColor(QPalette::Dark, background);
    pal.setColor(QPalette::Window, background);
    pal.setColor(QPalette::Text, settings->colorScheme().text);
    setPalette(pal);
    viewport()->setAutoFillBackground(true);
    viewport()->update();
}

void PdfView::setFullscreenMode(bool mode) {
    if(mFullscreen == mode)
        return;
    mFullscreen = mode;
    refreshColors();
}

void PdfView::setModel(std::shared_ptr<PdfPageModel> model) {
    if(mWorker) {
        mWorker->stop();
        mWorker->wait(3000);
        delete mWorker;
        mWorker = nullptr;
    }
    mModel = std::move(model);
    mPending.clear();
    mLastPage = -1;
    if(mModel && mModel->isOpen()) {
        mWorker = new RenderThread(mModel);
        connect(mWorker, &RenderThread::pageReady, this,
                [this](int page, const QImage &image, QSize target, quint64) {
                    applyPendingImage(page, image, target);
                });
        mWorker->start();
        mResetScrollOnNextPaint = true;
        fitPage();
        verticalScrollBar()->setValue(0);
        // the widget may not have its final size yet; fit again once it does
        QTimer::singleShot(0, this, [this] {
            if(mFitMode == FitMode::Page)
                fitPage();
            else if(mFitMode == FitMode::Width)
                fitWidth();
            verticalScrollBar()->setValue(0);
        });
    }
    updateScrollBars();
    viewport()->update();
}

ImageFitMode PdfView::imageFitMode() const {
    switch(mFitMode) {
    case FitMode::Page:  return FIT_WINDOW;
    case FitMode::Width: return FIT_WIDTH;
    default: break;
    }
    return qFuzzyCompare(mZoom, qreal(1.0)) ? FIT_ORIGINAL : FIT_FREE;
}

QImage PdfView::currentPageImage() {
    if(!mModel || mModel->pageCount() <= 0)
        return QImage();
    const int page = currentPage();
    const QSize target = pageRenderSize(page);
    QImage image = mModel->cachedPage(page, target);
    if(image.isNull()) {
        image = mModel->renderPage(page, target);
        mModel->cachePage(page, image, target);
    }
    return image;
}

int PdfView::pageCount() const {
    return mModel ? mModel->pageCount() : 0;
}

int PdfView::pageTop(int page) const {
    if(!mModel)
        return 0;
    int top = mMargin;
    for(int i = 0; i < page && i < mModel->pageCount(); i++) {
        const QSizeF size = mModel->pageSize(i);
        top += int(size.height() * mZoom) + mGap;
    }
    return top;
}

QRect PdfView::pageRect(int page) const {
    if(!mModel)
        return QRect();
    const QSizeF size = mModel->pageSize(page);
    const int width = int(size.width() * mZoom);
    const int height = int(size.height() * mZoom);
    // pages are centered when the window is wider than the sheet
    const int x = qMax(mMargin, (qMax(contentWidth(), viewport()->width()) - width) / 2);
    return QRect(x, pageTop(page), width, height);
}

int PdfView::contentHeight() const {
    if(!mModel || mModel->pageCount() <= 0)
        return 0;
    int height = mMargin * 2 + mGap * (mModel->pageCount() - 1);
    for(int page = 0; page < mModel->pageCount(); page++)
        height += int(mModel->pageSize(page).height() * mZoom);
    return height;
}

int PdfView::contentWidth() const {
    if(!mModel)
        return 0;
    return int(mModel->maxPageSize().width() * mZoom) + mMargin * 2;
}

QSize PdfView::pageRenderSize(int page) const {
    if(!mModel)
        return QSize();
    const QSizeF size = mModel->pageSize(page);
    const qreal dpr = devicePixelRatioF();
    return QSize(qMax(1, int(size.width() * mZoom * dpr)),
                 qMax(1, int(size.height() * mZoom * dpr)));
}

void PdfView::updateScrollBars() {
    const int viewportHeight = viewport()->height();
    const int viewportWidth = viewport()->width();
    verticalScrollBar()->setRange(0, qMax(0, contentHeight() - viewportHeight));
    verticalScrollBar()->setPageStep(viewportHeight);
    verticalScrollBar()->setSingleStep(qMax(1, viewportHeight / 10));
    horizontalScrollBar()->setRange(0, qMax(0, contentWidth() - viewportWidth));
    horizontalScrollBar()->setPageStep(viewportWidth);
    horizontalScrollBar()->setSingleStep(qMax(1, viewportWidth / 10));
}

void PdfView::setZoom(qreal zoom) {
    const qreal clamped = qBound(qreal(0.1), zoom, qreal(8.0));
    if(qFuzzyCompare(clamped, mZoom))
        return;
    // keep the middle of the view on the same spot
    const qreal oldZoom = mZoom;
    const int oldOffset = verticalScrollBar()->value() + viewport()->height() / 2;
    mZoom = clamped;
    mFitMode = FitMode::None;
    mPending.clear();
    updateScrollBars();
    verticalScrollBar()->setValue(int(oldOffset * (mZoom / oldZoom)) - viewport()->height() / 2);
    emit zoomChanged(mZoom);
    viewport()->update();
    requestVisiblePages();
}

void PdfView::fitPage() {
    if(!mModel || mModel->pageCount() <= 0)
        return;
    const QSizeF size = mModel->maxPageSize();
    if(size.isEmpty())
        return;
    const qreal fitX = (viewport()->width() - mMargin * 2) / size.width();
    const qreal fitY = (viewport()->height() - mMargin * 2) / size.height();
    mZoom = qBound(qreal(0.1), qMin(fitX, fitY), qreal(8.0));
    mFitMode = FitMode::Page;
    mPending.clear();
    updateScrollBars();
    emit zoomChanged(mZoom);
    viewport()->update();
    requestVisiblePages();
}

void PdfView::fitWidth() {
    if(!mModel || mModel->pageCount() <= 0)
        return;
    const QSizeF size = mModel->maxPageSize();
    if(size.isEmpty())
        return;
    mZoom = qBound(qreal(0.1), (viewport()->width() - mMargin * 2) / size.width(), qreal(8.0));
    mFitMode = FitMode::Width;
    mPending.clear();
    updateScrollBars();
    emit zoomChanged(mZoom);
    viewport()->update();
    requestVisiblePages();
}

int PdfView::currentPage() const {
    if(!mModel || mModel->pageCount() <= 0)
        return 0;
    const int center = verticalScrollBar()->value() + viewport()->height() / 2;
    for(int page = 0; page < mModel->pageCount(); page++) {
        const QRect rect = pageRect(page);
        if(center < rect.bottom())
            return page;
    }
    return mModel->pageCount() - 1;
}

void PdfView::goToPage(int page) {
    if(!mModel || mModel->pageCount() <= 0)
        return;
    const int target = qBound(0, page, mModel->pageCount() - 1);
    verticalScrollBar()->setValue(pageTop(target) - mMargin);
    viewport()->update();
    requestVisiblePages();
}

void PdfView::applyPendingImage(int page, const QImage &image, QSize target) {
    mPending.removeAll(page);
    if(image.isNull()) {
        viewport()->update();
        return;
    }
    // an image drawn for a different size (zoom changed meanwhile) is of no use
    if(target != pageRenderSize(page))
        return;
    mModel->cachePage(page, image, target);
    viewport()->update(pageRect(page).translated(-horizontalScrollBar()->value(),
                                                -verticalScrollBar()->value()));
}

void PdfView::requestVisiblePages() {
    if(!mWorker || !mModel || mModel->pageCount() <= 0)
        return;
    const int top = verticalScrollBar()->value();
    const int bottom = top + viewport()->height();
    QList<QPair<int, QSize>> wanted;
    for(int page = 0; page < mModel->pageCount(); page++) {
        const QRect rect = pageRect(page);
        if(rect.bottom() < top - viewport()->height() / 2)
            continue;
        if(rect.top() > bottom + viewport()->height() / 2)
            break;
        const QSize target = pageRenderSize(page);
        if(target.isEmpty())
            continue;
        if(!mModel->cachedPage(page, target).isNull())
            continue;
        wanted.append({page, target});
    }
    // the page under the pointer first, then downwards
    std::sort(wanted.begin(), wanted.end(), [top](const QPair<int, QSize> &a, const QPair<int, QSize> &b) {
        return a.first < b.first;
    });
    mPending.clear();
    for(const auto &job : wanted)
        mPending.append(job.first);
    mWorker->setWanted(wanted);
}

void PdfView::paintEvent(QPaintEvent *event) {
    QPainter painter(viewport());
    painter.fillRect(event->rect(), palette().color(QPalette::Dark));

    // Encrypted but still locked: there are no pages to count, so this has to be
    // checked before the empty-document branch or the window says "no pages".
    if(mModel && mModel->isEncrypted()) {
        painter.setPen(palette().color(QPalette::Text));
        painter.drawText(viewport()->rect(), Qt::AlignCenter, tr("Password required"));
        return;
    }
    if(!mModel || mModel->pageCount() <= 0) {
        painter.setPen(palette().color(QPalette::Text));
        painter.drawText(viewport()->rect(), Qt::AlignCenter, tr("No pages"));
        return;
    }

    if(mResetScrollOnNextPaint) {
        mResetScrollOnNextPaint = false;
        verticalScrollBar()->setValue(0);
        horizontalScrollBar()->setValue(0);
    }
    const int dx = horizontalScrollBar()->value();
    const int dy = verticalScrollBar()->value();
    const QRect viewRect = event->rect();

    for(int page = 0; page < mModel->pageCount(); page++) {
        const QRect rect = pageRect(page).translated(-dx, -dy);
        if(!rect.intersects(viewRect))
            continue;
        // paper, so a page that is still rendering does not look like a hole
        painter.fillRect(rect, Qt::white);
        const QSize target = pageRenderSize(page);
        const QImage cached = mModel->cachedPage(page, target);
        if(!cached.isNull()) {
            painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
            painter.drawImage(rect, cached);
        }
    }
}

void PdfView::resizeEvent(QResizeEvent *event) {
    QAbstractScrollArea::resizeEvent(event);
    if(mModel && mModel->pageCount() > 0) {
        if(mFitMode == FitMode::Page)
            fitPage();
        else if(mFitMode == FitMode::Width)
            fitWidth();
        else {
            updateScrollBars();
            requestVisiblePages();
        }
    }
    viewport()->update();
}

void PdfView::scrollContentsBy(int dx, int dy) {
    QAbstractScrollArea::scrollContentsBy(dx, dy);
    const int page = currentPage();
    if(page != mLastPage) {
        mLastPage = page;
        emit currentPageChanged(page);
    }
    requestVisiblePages();
}

void PdfView::scrollUp() {
    verticalScrollBar()->setValue(verticalScrollBar()->value() - viewport()->height() * 4 / 5);
}

void PdfView::scrollDown() {
    verticalScrollBar()->setValue(verticalScrollBar()->value() + viewport()->height() * 4 / 5);
}

void PdfView::wheelEvent(QWheelEvent *event) {
    if(event->modifiers() & Qt::ControlModifier) {
        const int delta = event->angleDelta().y();
        if(delta)
            setZoom(mZoom * (delta > 0 ? 1.15 : 1.0 / 1.15));
        event->accept();
        return;
    }
    QAbstractScrollArea::wheelEvent(event);
}

#include "pdfview.moc"
