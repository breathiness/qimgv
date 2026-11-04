#pragma once

#include "gui/customwidgets/floatingwidgetcontainer.h"
#include <QVBoxLayout>
#include "gui/viewers/imageviewerv2.h"
#include "gui/viewers/videoplayerinitproxy.h"
#include "pdf/pdfview.h"
#include "gui/overlays/videocontrolsproxy.h"
#include "gui/overlays/zoomindicatoroverlayproxy.h"
#include "gui/overlays/clickzoneoverlay.h"
#include "gui/contextmenu.h"

enum CurrentWidget {
    IMAGEVIEWER,
    VIDEOPLAYER,
    PDFVIEW,
    UNSET
};

class ViewerWidget : public FloatingWidgetContainer
{
    Q_OBJECT
public:
    // what fit-window should size itself by, when the whole image is not the unit
    void setFitUnitSize(const QSize &size);
    // explicit scroll, used when a page step moves the pdf view
    void scrollBy(int dx, int dy);
    // paged documents (pdf) get their own viewer, which renders on demand
    bool showPdf(std::shared_ptr<PdfPageModel> model);
    bool isPdf() const { return currentWidget == PDFVIEW; }
    void pdfNextPage();
    void pdfPrevPage();
    int pdfCurrentPage() const;
    int pdfPageCount() const;
    explicit ViewerWidget(QWidget *parent = nullptr);
    QRect imageRect();
    float currentScale();
    QSize sourceSize();

    void setInteractionEnabled(bool mode);
    bool interactionEnabled();
    // true while the video player is the active viewer
    bool isVideo();
    // video playback: scale the picture to the widget (fit) or show it at its
    // own size, only downscaling when it does not fit
    void setVideoScaled(bool scaled);
    bool videoScaled();

    bool showImage(std::unique_ptr<QPixmap> pixmap);
    void refreshImage(std::unique_ptr<QPixmap> pixmap);
    bool showAnimation(std::shared_ptr<QMovie> movie);
    void onScalingFinished(std::unique_ptr<QPixmap> scaled);
    bool isDisplaying();
    bool lockZoomEnabled();
    bool lockViewEnabled();
    ScalingFilter scalingFilter();

private:
    QVBoxLayout layout;
    std::unique_ptr<ImageViewerV2> imageViewer;
    std::unique_ptr<PdfView> pdfView;
    std::unique_ptr<VideoPlayerInitProxy> videoPlayer;
    std::unique_ptr<ContextMenu> contextMenu;
    VideoControlsProxyWrapper *videoControls;
    ZoomIndicatorOverlayProxy *zoomIndicator;
    ClickZoneOverlay *clickZoneOverlay;

    void connectPdfSignals();
    void enableImageViewer();
    void enableVideoPlayer();
    void enablePdfView();

    CurrentWidget currentWidget;
    // what setVideoUnscaled() was last given; the video player has no getter
    bool mVideoScaled = true;
    bool mInteractionEnabled, mWaylandCursorWorkaround;
    QTimer cursorTimer;
    const int CURSOR_HIDE_TIMEOUT_MS = 1000;
    bool mIsFullscreen;

    void disableImageViewer();
    void disableVideoPlayer();
    void disablePdfView();

    QRect videoControlsArea();

    bool eventFilter(QObject *object, QEvent *event);

private slots:
    void onScaleChanged(qreal);
    void onVideoPlaybackFinished();
    void onAnimationPlaybackFinished();

signals:
    void scalingRequested(QSize, ScalingFilter);
    void fullResolutionNeeded();
    void zoomIn();
    void zoomOut();
    void zoomInCursor();
    void zoomOutCursor();
    void scrollUp();
    void scrollDown();
    void scrollLeft();
    void scrollRight();
    void fitWindow();
    void fitWidth();
    void fitOriginal();
    void fitWindowStretch();
    void toggleTransparencyGrid();
    void draggedOut();
    void setFilterNearest();
    void setFilterBilinear();
    void setScalingFilter(ScalingFilter filter);
    void playbackFinished();
    void currentPageChanged();
    void toggleLockZoom();
    void toggleLockView();
    void showScriptSettings();

public slots:
    bool showVideo(QString file);
    void stopPlayback();
    void setFitMode(ImageFitMode mode);
    ImageFitMode fitMode();
    void closeImage();
    void hideCursor();
    void showCursor();
    void hideCursorTimed(bool restartTimer);

    // video control
    void pauseResumePlayback();
    void seek(int pos);
    void seekRelative(int pos);
    void seekBackward();
    void seekForward();
    void frameStep();
    void frameStepBack();
    void toggleMute();
    void volumeUp();
    void volumeDown();

    void startPlayback();
    void showContextMenu();
    void hideContextMenu();
    void showContextMenu(QPoint pos);
    void onFullscreenModeChanged(bool);
    void readSettings();
    void setLoopPlayback(bool mode);

protected:
    void mouseMoveEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void hideEvent(QHideEvent *event);

    void keyPressEvent(QKeyEvent *event);
    void leaveEvent(QEvent *event);
    bool focusNextPrevChild(bool mode);
};
