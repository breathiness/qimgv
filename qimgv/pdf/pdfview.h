#pragma once

#include <QAbstractScrollArea>

#include "settings.h"
#include <QImage>
#include <QList>
#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>
#include <memory>

class PdfPageModel;

// Shows a pdf the way a reader does: the pages sit one below the other in a virtual
// column, the scroll bar spans all of them, and only the pages on screen are drawn -
// at the size they are drawn, so zooming stays sharp instead of upscaling a bitmap.
//
// The rendering itself runs on one worker thread. It keeps a "what is wanted right
// now" list rather than a queue of everything ever asked for: scrolling fast, or
// zooming, replaces the list instead of building a backlog.
class PdfView : public QAbstractScrollArea {
    Q_OBJECT
public:
    enum class FitMode { None, Page, Width };


    explicit PdfView(QWidget *parent = nullptr);
    ~PdfView() override;

    void setModel(std::shared_ptr<PdfPageModel> model);
    std::shared_ptr<PdfPageModel> model() const { return mModel; }

    qreal zoom() const { return mZoom; }
    void setZoom(qreal zoom);
    void fitPage();
    void fitWidth();
    FitMode fitMode() const { return mFitMode; }
    // the mode in the vocabulary the window and the nav bar use
    ImageFitMode imageFitMode() const;
    // the page in the middle of the view, rendered at the current zoom
    QImage currentPageImage();
    // fullscreen uses the theme's fullscreen background, like the image viewer
    void setFullscreenMode(bool mode);
    void refreshColors();

    int pageCount() const;
    int currentPage() const;
    void goToPage(int page);
    void nextPage() { goToPage(currentPage() + 1); }
    void prevPage() { goToPage(currentPage() - 1); }

    // vertical offset of a page inside the document, in device pixels
    int pageTop(int page) const;

public slots:
    // the same vocabulary the image viewer uses, so the actions bind to either
    void setFitWindow() { fitPage(); }
    void setFitWidth() { fitWidth(); }
    void setFitOriginal() { setZoom(1.0); }
    void zoomIn() { setZoom(mZoom * 1.15); }
    void zoomOut() { setZoom(mZoom / 1.15); }
    void scrollUp();
    void scrollDown();

signals:
    void zoomChanged(qreal zoom);
    void currentPageChanged(int page);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    class RenderThread;

    QSize pageRenderSize(int page) const;
    QRect pageRect(int page) const;
    int contentHeight() const;
    int contentWidth() const;
    void updateScrollBars();
    void requestVisiblePages();
    void applyPendingImage(int page, const QImage &image, QSize target);

    std::shared_ptr<PdfPageModel> mModel;
    RenderThread *mWorker = nullptr;
    qreal mZoom = 1.0;
    FitMode mFitMode = FitMode::Page;
    int mGap = 12;
    int mMargin = 8;
    int mLastPage = -1;
    bool mFullscreen = false;
    // the first frame after opening starts at the first page
    bool mResetScrollOnNextPaint = false;
    // pages that were asked for but not delivered yet
    QList<int> mPending;
};
