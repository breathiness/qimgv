#include "mainwindow.h"


#include <QDialogButtonBox>
#include <QEventLoop>
#include <QLabel>
#include <QLineEdit>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>
#include <functional>
#include "gui/customwidgets/materialdialog.h"
#include "gui/customwidgets/windowresizegrip.h"
#include "gui/customwidgets/windowtitlebar.h"
#include "sourcecontainers/imagestatic.h"
#include "uistyle.h"

#ifdef _WIN32
#include <windows.h>
#endif

// Runs the event loop until one of the connections added by `arm` quits it, or
// until the timeout fires. Returns true when a condition fired first.
// The startup code used to spin on processEvents() with a sleep instead, which
// re-entered paint and layout every few milliseconds for as long as the wait
// lasted.
static bool waitForStartupCondition(int timeoutMs, const std::function<void(QEventLoop &)> &arm) {
    if(timeoutMs <= 0)
        return false;
    QEventLoop loop;
    QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    arm(loop);
    deadline.start(timeoutMs);
    loop.exec();
    // still active: the loop was quit by a condition, not by the deadline
    return deadline.isActive();
}

// TODO: nuke this and rewrite

MW::MW(QWidget *parent)
    : FloatingWidgetContainer(parent),
      currentDisplay(0),
      maximized(false),
      activeSidePanel(SIDEPANEL_NONE),
      copyOverlay(nullptr),
      saveOverlay(nullptr),
      renameOverlay(nullptr),
      infoBarFullscreen(nullptr),
      imageInfoOverlay(nullptr),
      floatingMessage(nullptr),
      navBar(nullptr),
      cropPanel(nullptr),
      cropOverlay(nullptr)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    layout.setContentsMargins(0,0,0,0);
    layout.setSpacing(0);

    setMinimumSize(10,10);

    // do not steal focus when clicked
    // this is just a container. accept key events only
    // via passthrough from child widgets
    setFocusPolicy(Qt::NoFocus);

    // Qlementine mode draws its own window frame: the system title bar goes
    // away and the bar below takes over dragging, the window buttons and the
    // title. The stock/classic styles keep the native one.
    if(UiStyle::useQlementine()) {
        setWindowFlag(Qt::FramelessWindowHint, true);
        titleBar = new WindowTitleBar(this);
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);
        root->addWidget(titleBar);
        root->addLayout(&layout, 1);
        setupResizeGrips();
    } else {
        this->setLayout(&layout);
    }

    setWindowTitle(QCoreApplication::applicationName() + " " +
                   QCoreApplication::applicationVersion());

    this->setMouseTracking(true);
    this->setAcceptDrops(true);
    this->setAccessibleName("mainwindow");
    windowGeometryChangeTimer.setSingleShot(true);
    windowGeometryChangeTimer.setInterval(30);
    setupUi();

    connect(settings, &Settings::settingsChanged, this, &MW::readSettings);
    connect(&windowGeometryChangeTimer, &QTimer::timeout, this, &MW::onWindowGeometryChanged);
    connect(this, &MW::fullscreenStateChanged, this, &MW::adaptToWindowState);

    readSettings();
    currentDisplay = settings->lastDisplay();
    maximized = settings->maximizedWindow();
    restoreWindowGeometry();
}

// A frameless window has no non-client border, so the resize targets have to be
// widgets: four edges plus four corners, each starting a system resize. They sit
// on top of everything and are hidden while maximized or in fullscreen.
void MW::setupResizeGrips() {
    const Qt::Edges edges[8] = {
        Qt::LeftEdge, Qt::RightEdge, Qt::TopEdge, Qt::BottomEdge,
        Qt::LeftEdge | Qt::TopEdge, Qt::RightEdge | Qt::TopEdge,
        Qt::LeftEdge | Qt::BottomEdge, Qt::RightEdge | Qt::BottomEdge
    };
    for(const Qt::Edges gripEdges : edges)
        resizeGrips.append(new WindowResizeGrip(gripEdges, this));
}

void MW::layoutResizeGrips() {
    if(resizeGrips.isEmpty())
        return;
    const int edge = 5;
    const int corner = 11;
    const int w = width();
    const int h = height();
    const QRect rects[8] = {
        QRect(0, 0, edge, h),
        QRect(w - edge, 0, edge, h),
        QRect(0, 0, w, edge),
        QRect(0, h - edge, w, edge),
        QRect(0, 0, corner, corner),
        QRect(w - corner, 0, corner, corner),
        QRect(0, h - corner, corner, corner),
        QRect(w - corner, h - corner, corner, corner)
    };
    const bool active = !isMaximized() && !isFullScreen();
    for(int i = 0; i < resizeGrips.size(); ++i) {
        WindowResizeGrip *grip = resizeGrips.at(i);
        grip->setGeometry(rects[i]);
        grip->setVisible(active);
        if(active)
            grip->raise();
    }
}

/*                                                             |--[ImageViewer]
 *                        |--[DocumentWidget]--[ViewerWidget]--|
 * [MW]--[CentralWidget]--|                                    |--[VideoPlayer]
 *                        |--[FolderView]
 *
 *  (not counting floating widgets)
 *  ViewerWidget exists for input handling reasons (correct overlay hover handling)
 */
void MW::setupUi() {
    viewerWidget.reset(new ViewerWidget(this));
    infoBarWindowed.reset(new InfoBarProxy(this));
    docWidget.reset(new DocumentWidget(viewerWidget, infoBarWindowed));
    folderView.reset(new FolderViewProxy(this));
    connect(folderView.get(), &FolderViewProxy::sortingSelected, this, &MW::sortingSelected);
    connect(folderView.get(), &FolderViewProxy::directorySelected, this, &MW::opened);
    connect(folderView.get(), &FolderViewProxy::copyUrlsRequested, this, &MW::copyUrlsRequested);
    connect(folderView.get(), &FolderViewProxy::moveUrlsRequested, this, &MW::moveUrlsRequested);
    connect(folderView.get(), &FolderViewProxy::showFoldersChanged, this, &MW::showFoldersChanged);
    connect(folderView.get(), &FolderViewProxy::dissolveFolderRequested, this, &MW::dissolveFolderRequested);

    centralWidget.reset(new CentralWidget(docWidget, folderView, this));
    layout.addWidget(centralWidget.get());
    controlsOverlay = new ControlsOverlay(docWidget.get());
    infoBarFullscreen = new FullscreenInfoOverlayProxy(viewerWidget.get());
    sidePanel = new SidePanel(this);
    layout.addWidget(sidePanel);
    imageInfoOverlay = new ImageInfoOverlayProxy(viewerWidget.get());
    floatingMessage = new FloatingMessageProxy(viewerWidget.get()); // todo: use additional one for folderview?
    connect(viewerWidget.get(), &ViewerWidget::scalingRequested, this, &MW::scalingRequested);
    connect(viewerWidget.get(), &ViewerWidget::fullResolutionNeeded, this, &MW::fullResolutionNeeded);
    connect(viewerWidget.get(), &ViewerWidget::modelDragged, this, &MW::modelDragged);
    connect(viewerWidget.get(), &ViewerWidget::modelDragFinished, this, &MW::modelDragFinished);
    connect(viewerWidget.get(), &ViewerWidget::draggedOut, this, qOverload<>(&MW::draggedOut));
    connect(viewerWidget.get(), &ViewerWidget::playbackFinished, this, &MW::playbackFinished);
    connect(viewerWidget.get(), &ViewerWidget::showScriptSettings, this, &MW::showScriptSettings);
    connect(this, &MW::zoomIn,        viewerWidget.get(), &ViewerWidget::zoomIn);
    connect(this, &MW::zoomOut,       viewerWidget.get(), &ViewerWidget::zoomOut);
    connect(this, &MW::zoomInCursor,  viewerWidget.get(), &ViewerWidget::zoomInCursor);
    connect(this, &MW::zoomOutCursor, viewerWidget.get(), &ViewerWidget::zoomOutCursor);
    connect(this, &MW::scrollUp,    viewerWidget.get(), &ViewerWidget::scrollUp);
    connect(this, &MW::scrollDown,  viewerWidget.get(), &ViewerWidget::scrollDown);
    connect(this, &MW::scrollLeft,  viewerWidget.get(), &ViewerWidget::scrollLeft);
    connect(this, &MW::scrollRight, viewerWidget.get(), &ViewerWidget::scrollRight);
    connect(this, &MW::pauseVideo,     viewerWidget.get(), &ViewerWidget::pauseResumePlayback);
    connect(this, &MW::stopPlayback,   viewerWidget.get(), &ViewerWidget::stopPlayback);
    connect(this, &MW::seekVideoForward, viewerWidget.get(), &ViewerWidget::seekForward);
    connect(this, &MW::seekVideoBackward,  viewerWidget.get(), &ViewerWidget::seekBackward);
    connect(this, &MW::frameStep,      viewerWidget.get(), &ViewerWidget::frameStep);
    connect(this, &MW::frameStepBack,  viewerWidget.get(), &ViewerWidget::frameStepBack);
    connect(this, &MW::toggleMute,  viewerWidget.get(), &ViewerWidget::toggleMute);
    connect(this, &MW::volumeUp,  viewerWidget.get(), &ViewerWidget::volumeUp);
    connect(this, &MW::volumeDown,  viewerWidget.get(), &ViewerWidget::volumeDown);
    connect(this, &MW::toggleTransparencyGrid, viewerWidget.get(), &ViewerWidget::toggleTransparencyGrid);
    connect(this, &MW::setLoopPlayback,  viewerWidget.get(), &ViewerWidget::setLoopPlayback);

    // Floating navigation bar. Deliberately kept out of "layout" so it can be
    // positioned freely on top of everything else and dragged around.
    // It is configured from Preferences -> "Navigation bar".
    navBar = new FloatingNavBar(this);
    // the fit-mode button shows the mode the viewer is actually in, not the last
    // one the bar set itself
    navBar->setFitModeProvider([this]() { return viewerWidget->fitMode(); });
    navBar->setPdfProvider([this]() { return viewerWidget && viewerWidget->isPdf(); });
    navBar->setVideoStateProvider([this]() { return viewerWidget->isVideo(); },
                                  [this]() { return viewerWidget->videoScaled(); });
    // add / remove / edit: the bar already rebuilt itself, just persist the list
    connect(navBar, &FloatingNavBar::shortcutsChanged, this, [this]() {
        settings->saveNavBar(navBar->shortcuts(), navBar->icons());
    });
    connect(this, &FloatingWidgetContainer::resized, navBar, &FloatingNavBar::onContainerResized);
    // in "always" mode the bar is part of the window instead of an overlay
    connect(navBar, &FloatingNavBar::layoutStateChanged, this, &MW::updateNavBarSpace);
}

void MW::setupFullUi() {
    setupCropPanel();
    docWidget->allowPanelInit();
    docWidget->setupMainPanel();
    infoBarWindowed->init();
    infoBarFullscreen->init();
}

void MW::setupCropPanel() {
    if(cropPanel)
        return;
    cropOverlay = new CropOverlay(viewerWidget.get());
    cropPanel = new CropPanel(cropOverlay, this);
    connect(cropPanel, &CropPanel::cancel, this, &MW::hideCropPanel);
    connect(cropPanel, &CropPanel::crop,   this, &MW::hideCropPanel);
    connect(cropPanel, &CropPanel::crop,   this, &MW::cropRequested);
    connect(cropPanel, &CropPanel::cropAndSave, this, &MW::hideCropPanel);
    connect(cropPanel, &CropPanel::cropAndSave, this, &MW::cropAndSaveRequested);
}

void MW::setupCopyOverlay() {
    copyOverlay = new CopyOverlay(viewerWidget.get());
    connect(copyOverlay, &CopyOverlay::copyRequested, this, &MW::copyRequested);
    connect(copyOverlay, &CopyOverlay::moveRequested, this, &MW::moveRequested);
}

void MW::setupSaveOverlay() {
    saveOverlay = new SaveConfirmOverlay(viewerWidget.get());
    connect(saveOverlay, &SaveConfirmOverlay::saveClicked,    this, &MW::saveRequested);
    connect(saveOverlay, &SaveConfirmOverlay::saveAsClicked,  this, &MW::saveAsClicked);
    connect(saveOverlay, &SaveConfirmOverlay::discardClicked, this, &MW::discardEditsRequested);
}

void MW::setupRenameOverlay() {
    renameOverlay = new RenameOverlay(this);
    renameOverlay->setName(info.fileName);
    connect(renameOverlay, &RenameOverlay::renameRequested, this, &MW::renameRequested);
}

void MW::toggleFolderView() {
    hideCropPanel();
    if(copyOverlay)
        copyOverlay->hide();
    if(renameOverlay)
        renameOverlay->hide();
    docWidget->hideFloatingPanel();
    imageInfoOverlay->hide();
    centralWidget->toggleViewMode();
    updateNavBarVisibility();
    onInfoUpdated();
}

void MW::enableFolderView() {
    hideCropPanel();
    if(copyOverlay)
        copyOverlay->hide();
    if(renameOverlay)
        renameOverlay->hide();
    docWidget->hideFloatingPanel();
    imageInfoOverlay->hide();
    centralWidget->showFolderView();
    updateNavBarVisibility();
    onInfoUpdated();
}

void MW::enableDocumentView() {
    centralWidget->showDocumentView();
    updateNavBarVisibility();
    onInfoUpdated();
}

// The navigation bar is part of the image viewer. On the other pages it is taken
// off rather than left hidden, so nothing of it can appear there.
void MW::updateNavBarVisibility() {
    if(!navBar)
        return;
    navBar->setViewActive(currentViewMode() != MODE_FOLDERVIEW);
    updateNavBarSpace();
}

// In "always" mode the bar is not an overlay: it keeps a strip of the window for
// itself, so it never sits on top of the bottom of the image. Auto mode hides it
// instead and needs no room, and a dragged bar floats like any other overlay.
void MW::updateNavBarSpace() {
    if(!navBar)
        return;
    const bool reserve = navBar->mode() == NAVBAR_ALWAYS
                         && navBar->isDocked()
                         && currentViewMode() != MODE_FOLDERVIEW;
    const int space = reserve ? FloatingNavBar::kBarHeight : 0;
    if(layout.contentsMargins().bottom() == space)
        return;
    layout.setContentsMargins(0, 0, 0, space);
}

ViewMode MW::currentViewMode() {
    return centralWidget->currentViewMode();
}

void MW::fitWindow() {
    // a video has no zoom of its own: fitting means letting the player scale the
    // picture to this widget instead of showing it at its own size
    if(viewerWidget->isVideo()) {
        viewerWidget->setVideoScaled(true);
        showMessageFitWindow();
        return;
    }
    if(viewerWidget->interactionEnabled()) {
        viewerWidget->fitWindow();
    } else {
        showMessage("Zoom temporary disabled");
    }
}

void MW::fitWidth() {
    if(viewerWidget->isVideo()) {
        // the player keeps the aspect ratio; there is nothing narrower to fit
        viewerWidget->setVideoScaled(true);
        showMessageFitWidth();
        return;
    }
    if(viewerWidget->interactionEnabled()) {
        viewerWidget->fitWidth();
    } else {
        showMessage("Zoom temporary disabled");
    }
}

void MW::fitOriginal() {
    if(viewerWidget->isVideo()) {
        // 1:1 - the player only downscales when the picture does not fit
        viewerWidget->setVideoScaled(false);
        showMessageFitOriginal();
        return;
    }
    if(viewerWidget->interactionEnabled()) {
        viewerWidget->fitOriginal();
    } else {
        showMessage("Zoom temporary disabled");
    }
}

void MW::fitWindowStretch() {
    if(viewerWidget->isVideo()) {
        viewerWidget->setVideoScaled(true);
        showMessageFitWindow();
        return;
    }
    if(viewerWidget->interactionEnabled()) {
        viewerWidget->fitWindowStretch();
    } else {
        showMessage("Zoom temporary disabled");
    }
}

// switch between 1:1 and Fit All
// TODO: move to viewerWidget?
void MW::switchFitMode() {
    if(viewerWidget->isVideo()) {
        viewerWidget->setVideoScaled(!viewerWidget->videoScaled());
        if(viewerWidget->videoScaled())
            showMessageFitWindow();
        else
            showMessageFitOriginal();
        return;
    }
    if(viewerWidget->fitMode() == FIT_WINDOW)
        viewerWidget->setFitMode(FIT_ORIGINAL);
    else
        viewerWidget->setFitMode(FIT_WINDOW);
}

void MW::closeImage() {
    info.fileName = "";
    info.filePath = "";
    viewerWidget->closeImage();
}

// todo: fix flicker somehow
// ideally it should change img & resize in one go
void MW::preShowResize(QSize sz) {
    auto screens = qApp->screens();
    if(this->windowState() != Qt::WindowNoState || !screens.count() || screens.count() <= currentDisplay)
        return;
    int decorationSize = frameGeometry().height() - height();
    float maxSzMulti = settings->autoResizeLimit() / 100.f;
    QRect availableGeom = screens.at(currentDisplay)->availableGeometry();
    QSize maxSz = availableGeom.size() * maxSzMulti;
    maxSz.setHeight(maxSz.height() - decorationSize);
    if(!sz.isEmpty()) {
        if(sz.width() > maxSz.width() || sz.height() > maxSz.height())
            sz.scale(maxSz, Qt::KeepAspectRatio);
    } else {
        sz = maxSz;
    }
    QRect newGeom(0,0, sz.width(), sz.height());
    newGeom.moveCenter(availableGeom.center());
    newGeom.translate(0, decorationSize / 2);

    if(this->isVisible())
        setGeometry(newGeom);
    else // setGeometry wont work on hidden windows, so we just save for it to be restored later
        settings->setWindowGeometry(newGeom);
    qApp->processEvents(); // not needed anymore with patched qt?
}

void MW::showImage(std::unique_ptr<QPixmap> pixmap, QSize imageSize) {
    mFirstImageReady = true;
    emit firstImageReady();
    if(navBar)
        navBar->refreshFitCycleIcon();
    if(settings->autoResizeWindow())
        preShowResize(imageSize.isEmpty() ? pixmap->size() : imageSize);
    viewerWidget->showImage(std::move(pixmap));
    updateCropPanelData();
}

void MW::refreshImage(std::unique_ptr<QPixmap> pixmap) {
    viewerWidget->refreshImage(std::move(pixmap));
}

void MW::showAnimation(std::shared_ptr<QMovie> movie) {
    if(settings->autoResizeWindow())
        preShowResize(movie->frameRect().size());
    viewerWidget->showAnimation(movie);
    updateCropPanelData();
}

void MW::showVideo(QString file) {
    if(settings->autoResizeWindow())
        preShowResize(QSize()); // tmp. find a way to get this though mpv BEFORE playback
    viewerWidget->showVideo(file);
    if(navBar)
        navBar->refreshFitCycleIcon();
}

void MW::showContextMenu() {
    viewerWidget->showContextMenu();
}

void MW::onSortingChanged(SortingMode mode) {
    folderView.get()->onSortingChanged(mode);
    if(centralWidget.get()->currentViewMode() == ViewMode::MODE_DOCUMENT) {
        switch(mode) {
            case SortingMode::SORT_NAME:      showMessage("Sorting: By Name");              break;
            case SortingMode::SORT_NAME_DESC: showMessage("Sorting: By Name (desc.)");      break;
            case SortingMode::SORT_TIME:      showMessage("Sorting: By Time");              break;
            case SortingMode::SORT_TIME_DESC: showMessage("Sorting: By Time (desc.)");      break;
            case SortingMode::SORT_SIZE:      showMessage("Sorting: By File Size");         break;
            case SortingMode::SORT_SIZE_DESC: showMessage("Sorting: By File Size (desc.)"); break;
        }
    }
}

void MW::setDirectoryPath(QString path) {
    //closeImage();
    settings->setLastFolder(path);
    info.directoryPath = path;
    info.directoryName = path.split("/").last();
    folderView->setDirectoryPath(path);
    // The bar belongs to the image viewer; a fit-mode change could leave it
    // active while the folder view is on screen.
    updateNavBarVisibility();
    onInfoUpdated();
}

void MW::toggleLockZoom() {
    viewerWidget->toggleLockZoom();
    if(viewerWidget->lockZoomEnabled())
        showMessage("Zoom lock: ON");
    else
        showMessage("Zoom lock: OFF");
    onInfoUpdated();
}

void MW::toggleLockView() {
    viewerWidget->toggleLockView();
    if(viewerWidget->lockViewEnabled())
        showMessage("View lock: ON");
    else
        showMessage("View lock: OFF");
    onInfoUpdated();
}

void MW::toggleFullscreenInfoBar() {
    if(!this->isFullScreen())
        return;
    showInfoBarFullscreen = !showInfoBarFullscreen;
    if(showInfoBarFullscreen)
        infoBarFullscreen->showWhenReady();
    else
        infoBarFullscreen->hide();
}

void MW::toggleImageInfoOverlay() {
    if(centralWidget->currentViewMode() == MODE_FOLDERVIEW)
        return;
    if(imageInfoOverlay->isHidden())
        imageInfoOverlay->show();
    else
        imageInfoOverlay->hide();
}

void MW::toggleRenameOverlay(QString currentName) {
    if(!renameOverlay)
        setupRenameOverlay();
    if(renameOverlay->isHidden()) {
        renameOverlay->setBackdropEnabled((centralWidget->currentViewMode() == MODE_FOLDERVIEW));
        renameOverlay->setName(currentName);
        renameOverlay->show();
    } else {
        renameOverlay->hide();
    }
}

void MW::toggleScalingFilter() {
    ScalingFilter configuredFilter = settings->scalingFilter();
    if(viewerWidget->scalingFilter() == configuredFilter) {
        setFilterNearest();
    }
    else {
        setFilter(configuredFilter);
    }
}

void MW::setFilterNearest() {
    showMessage("Filter: nearest", 600);
    viewerWidget->setFilterNearest();
}

void MW::setFilterBilinear() {
    showMessage("Filter: bilinear", 600);
    viewerWidget->setFilterBilinear();
}

void MW::setFilter(ScalingFilter filter) {
    QString filterName;
    switch (filter) {
        case QI_FILTER_NEAREST:
            filterName = "nearest";
            break;
        case ScalingFilter::QI_FILTER_BILINEAR:
            filterName = "bilinear";
            break;
        case QI_FILTER_CV_BILINEAR_SHARPEN:
            filterName = "bilinear + sharpen";
            break;
        case QI_FILTER_CV_CUBIC:
            filterName = "bicubic";
            break;
        case QI_FILTER_CV_CUBIC_SHARPEN:
            filterName = "bicubic + sharpen";
            break;
        default:
            filterName = "configured " + QString::number(static_cast<int>(filter));
            break;
    }
    showMessage("Filter " + filterName, 600);
    viewerWidget->setScalingFilter(filter);
}

bool MW::isCropPanelActive() {
    return (activeSidePanel == SIDEPANEL_CROP);
}

void MW::onScalingFinished(std::unique_ptr<QPixmap> scaled) {
    viewerWidget->onScalingFinished(std::move(scaled));
}

void MW::saveWindowGeometry() {
    if(this->windowState() == Qt::WindowNoState)
        settings->setWindowGeometry(geometry());
    settings->setMaximizedWindow(maximized);
}

// does not apply fullscreen; window size / maximized state only
void MW::restoreWindowGeometry() {
    QRect geom = settings->windowGeometry();
    // Never restore a window bigger than the usable screen area (e.g. after the
    // resolution changed or the config was copied from another machine).
    // Otherwise the bottom of the window - and anything docked to it, like the
    // navigation bar - ends up off-screen.
    QRect avail;
    if(QScreen *scr = qApp->primaryScreen())
        avail = scr->availableGeometry();
    if(avail.isValid()) {
        geom.setWidth(qMin(geom.width(), avail.width()));
        geom.setHeight(qMin(geom.height(), avail.height()));
        geom.moveLeft(qBound(avail.left(), geom.left(), avail.right() - geom.width() + 1));
        geom.moveTop(qBound(avail.top(), geom.top(), avail.bottom() - geom.height() + 1));
        // A "windowed" geometry that fills the usable area is what a maximized
        // window looks like when it was saved by a build that did not consider
        // itself maximized (the maximized state is tracked in a separate flag, so
        // the two can get out of step). Restoring that and then maximizing asks
        // the platform to resize the window twice for no visible reason, and the
        // first of those two changes leaves a light 9 pixel edge along the left
        // and top of the maximized window for about a second. When the window is
        // going to be maximized anyway, the restored size is only used once the
        // user restores the window, so make that an ordinary size.
        if(settings->maximizedWindow()) {
            const int slack = 2;
            if(geom.width() >= avail.width() - slack && geom.height() >= avail.height() - slack) {
                const QSize normal(qMin(1400, avail.width() * 4 / 5),
                                   qMin(900, avail.height() * 4 / 5));
                geom.setSize(normal);
                geom.moveCenter(avail.center());
            }
        }
    }
    windowedGeometry = geom;
    if(settings->maximizedWindow()) {
        // A window that is created already maximized spends about a second inside
        // show() before the platform hands it the maximized size, and the first
        // frames it paints are the windowed ones. Give a hidden window the size
        // the maximized one is going to use, so those first frames are already
        // full size - showWindowed() then waits for the first image before it
        // shows the window at all. The size to restore to is kept in
        // windowedGeometry, because the platform's own idea of it is now the
        // screen rectangle.
        // This applies to a folder view too. Skipping it there left the folder
        // path free to paint a windowed-sized frame before the platform maximized
        // it; the extra layout pass is a few milliseconds and buys the certainty.
        if(!isVisible() && avail.isValid())
            this->setGeometry(avail);
        this->setWindowState(Qt::WindowMaximized);
    } else {
        this->setGeometry(geom);
    }
    updateCurrentDisplay();
    updatePreviewSizeHint();
}

// The first image starts decoding before the window is on screen, so the loader
// has to be told how large the viewer will be. Without this the hint keeps its
// screen-sized default and a windowed session decodes the whole file instead of
// a preview, because no power-of-two divisor reaches that default any more.
void MW::updatePreviewSizeHint() {
    // the widget's own ratio is not available before the window is shown
    QScreen *scr = screen() ? screen() : qApp->primaryScreen();
    const qreal dpr = scr ? scr->devicePixelRatio()
                          : qMax(qreal(1.0), devicePixelRatioF());
    QSize pixels = size() * dpr;
    if((isMaximized() || isFullScreen()) && scr)
        pixels = scr->size() * dpr;
    if(!pixels.isEmpty())
        ImageStatic::setPreviewSizeHint(pixels);
}

void MW::updateCurrentDisplay() {
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    currentDisplay = desktopWidget.screenNumber(this);
#else
    auto screens = qApp->screens();
    currentDisplay = screens.indexOf(this->window()->screen());
#endif
}

void MW::onWindowGeometryChanged() {
    saveWindowGeometry();
    updateCurrentDisplay();
}

void MW::saveCurrentDisplay() {
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    settings->setLastDisplay(desktopWidget.screenNumber(this));
#else
    settings->setLastDisplay(qApp->screens().indexOf(this->window()->screen()));
#endif
}

//#############################################################
//######################### EVENTS ############################
//#############################################################

void MW::mouseMoveEvent(QMouseEvent *event) {
    event->ignore();
}

bool MW::event(QEvent *event) {
    if(event->type() == QEvent::WindowStateChange) {
        const bool nowMaximized = isMaximized();
        // The window was given the screen rectangle before it was shown so that its
        // first frames were already full size, which makes the platform restore to
        // that rectangle. Put the size the user actually had back.
        if(maximized && !nowMaximized && !isFullScreen() && windowedGeometry.isValid())
            this->setGeometry(windowedGeometry);
        // only save maximized state if we are already visible
        // this filter out out events while the window is still being set up
        if(this->isVisible() && !this->isFullScreen())
            maximized = nowMaximized;
        if(titleBar)
            titleBar->setMaximized(nowMaximized);
        layoutResizeGrips();
    }
    if(event->type() == QEvent::Move || event->type() == QEvent::Resize)
        windowGeometryChangeTimer.start();
    return QWidget::event(event);
}

void MW::changeEvent(QEvent *event) {
    if(event->type() == QEvent::WindowTitleChange && titleBar)
        titleBar->setTitle(windowTitle());
    QWidget::changeEvent(event);
}

bool MW::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
#ifdef _WIN32
    // Qt labels messages it pulls off the queue "windows_dispatcher_MSG" and ones
    // that arrive through SendMessage "windows_generic_MSG"; both carry a MSG.
    const bool windowsMessage = eventType == QByteArrayLiteral("windows_generic_MSG")
                                || eventType == QByteArrayLiteral("windows_dispatcher_MSG");
    if(titleBar && windowsMessage) {
        auto *msg = static_cast<MSG *>(message);
        // A frameless window is maximized to the whole screen by the platform,
        // which covers the taskbar. Clamp the maximized rectangle to the work
        // area of the monitor the window is on.
        if(msg->message == WM_GETMINMAXINFO) {
            auto *info = reinterpret_cast<MINMAXINFO *>(msg->lParam);
            const HMONITOR monitor = MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO monitorInfo{};
            monitorInfo.cbSize = sizeof(MONITORINFO);
            if(GetMonitorInfo(monitor, &monitorInfo)) {
                const RECT work = monitorInfo.rcWork;
                const RECT full = monitorInfo.rcMonitor;
                info->ptMaxPosition.x = work.left - full.left;
                info->ptMaxPosition.y = work.top - full.top;
                info->ptMaxSize.x = work.right - work.left;
                info->ptMaxSize.y = work.bottom - work.top;
                *result = 0;
                return true;
            }
        }
    }
#endif
    return QWidget::nativeEvent(eventType, message, result);
}

// hook up to actionManager
void MW::keyPressEvent(QKeyEvent *event) {
    event->accept();
    actionManager->processEvent(event);
}

void MW::wheelEvent(QWheelEvent *event) {
    event->accept();
    const int dy = event->angleDelta().y();
    // video playback and horizontal-only events keep the plain shortcut mapping
    if(!viewerWidget || dy == 0 || viewerWidget->isVideo()) {
        actionManager->processEvent(event);
        return;
    }
    // The wheel does one thing at a time, and which one is a setting the
    // navigation bar can cycle (see FloatingNavBar's wheel button). Ctrl+wheel is
    // always the counterpart: it switches images when the wheel zooms, and it
    // zooms when the wheel switches or scrolls.
    const bool ctrl = event->modifiers().testFlag(Qt::ControlModifier);
    const bool up = dy > 0;
    const WheelMode mode = settings->wheelMode();
    QString action;
    if(mode == WHEEL_ZOOM) {
        action = ctrl ? (up ? "prevImage" : "nextImage")
                      : (up ? "zoomInCursor" : "zoomOutCursor");
    } else {
        // WHEEL_SWITCH_IMAGES, and WHEEL_SCROLL once the image reached an edge:
        // the viewer scrolls by itself and only lets the event through there
        action = ctrl ? (up ? "zoomInCursor" : "zoomOutCursor")
                      : (up ? "prevImage" : "nextImage");
    }
    actionManager->invokeAction(action);
}

void MW::mousePressEvent(QMouseEvent *event) {
    event->accept();
    actionManager->processEvent(event);
}

void MW::mouseReleaseEvent(QMouseEvent *event) {
    event->accept();
    actionManager->processEvent(event);
}

void MW::mouseDoubleClickEvent(QMouseEvent *event) {
    event->accept();
    QMouseEvent *fakePressEvent = new QMouseEvent(
        QEvent::MouseButtonPress,
        event->pos(),
        event->button(),
        event->buttons(),
        event->modifiers()
    );
    actionManager->processEvent(fakePressEvent);
    actionManager->processEvent(event);
}

void MW::close() {
    saveWindowGeometry();
    saveCurrentDisplay();
    // try to close window sooner
    // since qt6.3 QWidget::close() no longer works on hidden windows (bug?)
#if QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
    this->hide();
#endif
    if(copyOverlay)
        copyOverlay->saveSettings();
    QWidget::close();
}

void MW::closeEvent(QCloseEvent *event) {
    // catch the close event when user presses X on the window itself
    event->accept();
    actionManager->invokeAction("exit");
}

void MW::dragEnterEvent(QDragEnterEvent *e) {
    if(e->mimeData()->hasUrls()) {
        e->acceptProposedAction();
    }
}

void MW::dropEvent(QDropEvent *event) {
    emit droppedIn(event->mimeData(), event->source());
}

void MW::resizeEvent(QResizeEvent *event) {
    if(activeSidePanel == SIDEPANEL_CROP) {
        cropOverlay->setImageScale(viewerWidget->currentScale());
        cropOverlay->setImageDrawRect(viewerWidget->imageRect());
    }
    // keep the loader's preview size in step with the window, so that a preview
    // is never smaller than what is on screen
    ImageStatic::setPreviewSizeHint(viewerWidget->size() * devicePixelRatioF());
    layoutResizeGrips();
    FloatingWidgetContainer::resizeEvent(event);
}

void MW::showDefault() {
    if(!this->isVisible()) {
        if(settings->fullscreenMode())
            showFullScreen();
        else
            showWindowed();
    }
}

void MW::showSaveDialog(QString filePath) {
    QString newFilePath = getSaveFileName(filePath);
    if(!newFilePath.isEmpty())
        emit saveAsRequested(newFilePath);
}

QString MW::getSaveFileName(QString filePath) {
    docWidget->hideFloatingPanel();
    QStringList filters;
    // generate filter for writable images
    // todo: some may need to be blacklisted
    auto writerFormats = QImageWriter::supportedImageFormats();
    if(writerFormats.contains("jpg"))  filters.append("JPEG (*.jpg *.jpeg *jpe *jfif)");
    if(writerFormats.contains("png"))  filters.append("PNG (*.png)");
    if(writerFormats.contains("webp")) filters.append("WebP (*.webp)");
    // may not work..
    if(writerFormats.contains("jp2"))  filters.append("JPEG 2000 (*.jp2 *.j2k *.jpf *.jpx *.jpm *.jpgx)");
    if(writerFormats.contains("jxl"))  filters.append("JPEG-XL (*.jxl)");
    if(writerFormats.contains("avif")) filters.append("AVIF (*.avif *.avifs)");
    if(writerFormats.contains("tif"))  filters.append("TIFF (*.tif *.tiff)");
    if(writerFormats.contains("bmp"))  filters.append("BMP (*.bmp)");
#ifdef _WIN32
    if(writerFormats.contains("ico"))  filters.append("Icon Files (*.ico)");
#endif
    if(writerFormats.contains("ppm"))  filters.append("PPM (*.ppm)");
    if(writerFormats.contains("xbm"))  filters.append("XBM (*.xbm)");
    if(writerFormats.contains("xpm"))  filters.append("XPM (*.xpm)");
    if(writerFormats.contains("dds"))  filters.append("DDS (*.dds)");
    if(writerFormats.contains("wbmp")) filters.append("WBMP (*.wbmp)");
    // add everything else from imagewriter
    for(auto fmt : writerFormats) {
        if(filters.filter(fmt).isEmpty())
            filters.append(fmt.toUpper() + " (*." + fmt + ")");
    }
    QString filterString = filters.join(";; ");

    // find matching filter for the current image
    QString selectedFilter = "JPEG (*.jpg *.jpeg *jpe *jfif)";
    QFileInfo fi(filePath);
    for(auto filter : filters) {
        if(filter.contains(fi.suffix().toLower())) {
            selectedFilter = filter;
            break;
        }
    }
    QString newFilePath = QFileDialog::getSaveFileName(this, tr("Save File as..."), filePath, filterString, &selectedFilter);
    return newFilePath;
}

void MW::showOpenDialog(QString path) {
    docWidget->hideFloatingPanel();

    QFileDialog dialog(this);
    QStringList imageFilter;
    imageFilter.append(settings->supportedFormatsFilter());
    imageFilter.append("All Files (*)");
    dialog.setDirectory(path);
    dialog.setNameFilters(imageFilter);
    dialog.setWindowTitle("Open image");
    dialog.setWindowModality(Qt::ApplicationModal);
    connect(&dialog, &QFileDialog::fileSelected, this, &MW::opened);
    dialog.exec();
}

void MW::showResizeDialog(QSize initialSize) {
    ResizeDialog dialog(initialSize, this);
    connect(&dialog, &ResizeDialog::sizeSelected, this, &MW::resizeRequested);
    dialog.exec();
}

DialogResult MW::fileReplaceDialog(QString src, QString dst, FileReplaceMode mode, bool multiple) {
    FileReplaceDialog dialog(this);
    dialog.setModal(true);
    dialog.setSource(src);
    dialog.setDestination(dst);
    dialog.setMode(mode);
    dialog.setMulti(multiple);

    dialog.exec();

    return dialog.getResult();
}

void MW::showSettings() {
    docWidget->hideFloatingPanel();
    SettingsDialog settingsDialog(this);
    settingsDialog.exec();
}

void MW::showScriptSettings() {
    docWidget->hideFloatingPanel();
    SettingsDialog settingsDialog(this);
    // "Scripts" moved to index 5 after the navigation bar page was inserted
    settingsDialog.switchToPage(5);
    settingsDialog.exec();
}

void MW::triggerFullScreen() {
    if(!isFullScreen()) {
        showFullScreen();
    } else {
        showWindowed();
    }
}

void MW::showFullScreen() {
    //do not save immediately on application start
    if(!isHidden())
        saveWindowGeometry();
    auto screens = qApp->screens();
    // todo: why check the screen again?
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    int _currentDisplay = desktopWidget.screenNumber(this);
#else
    int _currentDisplay = screens.indexOf(this->window()->screen());
#endif
    //move to target screen
    if(screens.count() > currentDisplay && currentDisplay != _currentDisplay) {
        this->move(screens.at(currentDisplay)->geometry().x(),
                   screens.at(currentDisplay)->geometry().y());
    }
    QWidget::showFullScreen();
    // try to repaint sooner
    qApp->processEvents();
    updateNavBarVisibility();
    emit fullscreenStateChanged(true);
}

void MW::setExpectFirstImage(bool expect) {
    mExpectFirstImage = expect;
}

void MW::showWindowed() {
    if(isFullScreen())
        QWidget::showNormal();
    const bool startMaximized = settings->maximizedWindow() && !isVisible();
    restoreWindowGeometry();
    // The window is about to be created at the maximized size; give the loader a
    // moment to hand over the first image first, so the window appears with the
    // picture in it instead of appearing empty (or showing a smaller picture that
    // is replaced a second later). The wait is bounded so a slow or missing first
    // image cannot keep the window hidden.
    if(startMaximized && mExpectFirstImage && !isVisible() && !mFirstImageReady) {
        waitForStartupCondition(900, [this](QEventLoop &loop) {
            connect(this, &MW::firstImageReady, &loop, &QEventLoop::quit);
        });
    }
    // Started without an image: a folder view has nothing to wait for. Mapping
    // the window is what makes the view paint and start asking for thumbnails, so
    // it is shown right away and the tiles fill in on their own. It used to be
    // mapped invisible (setWindowOpacity(0)) and revealed once the first
    // thumbnail arrived, bounded at 2.5 s: on a maximized start that kept the
    // window invisible for about another second after it was already mapped,
    // because most of that time is spent inside show() and was not covered by the
    // wait at all.
    QWidget::show();
    // try to repaint sooner
    qApp->processEvents();
    updateNavBarVisibility();
    emit fullscreenStateChanged(false);
}

void MW::updateCropPanelData() {
    if(cropPanel && activeSidePanel == SIDEPANEL_CROP) {
        cropPanel->setImageRealSize(viewerWidget->sourceSize());
        cropOverlay->setImageDrawRect(viewerWidget->imageRect());
        cropOverlay->setImageScale(viewerWidget->currentScale());
        cropOverlay->setImageRealSize(viewerWidget->sourceSize());
    }
}

void MW::showSaveOverlay() {
    if(!settings->showSaveOverlay())
        return;
    if(!saveOverlay)
        setupSaveOverlay();
    saveOverlay->show();
}

void MW::hideSaveOverlay() {
    if(!saveOverlay)
        return;
    saveOverlay->hide();
}

void MW::showChangelogWindow() {
    changelogWindow->show();
}

void MW::showChangelogWindow(QString text) {
    changelogWindow->setText(text);
    changelogWindow->show();
}

void MW::triggerCropPanel() {
    if(activeSidePanel != SIDEPANEL_CROP) {
        showCropPanel();
    } else {
        hideCropPanel();
    }
}

void MW::showCropPanel() {
    if(centralWidget->currentViewMode() == MODE_FOLDERVIEW)
        return;

    if(activeSidePanel != SIDEPANEL_CROP) {
        docWidget->hideFloatingPanel();
        sidePanel->setWidget(cropPanel);
        sidePanel->show();
        cropOverlay->show();
        activeSidePanel = SIDEPANEL_CROP;
        // reset & lock zoom so CropOverlay won't go crazy
        viewerWidget->fitWindow();
        setInteractionEnabled(false);
        // feed the panel current image info
        updateCropPanelData();
    }
}

void MW::setInteractionEnabled(bool mode) {
    docWidget->setInteractionEnabled(mode);
    viewerWidget->setInteractionEnabled(mode);
}

void MW::hideCropPanel() {
    sidePanel->hide();
    if(activeSidePanel == SIDEPANEL_CROP) {
        cropOverlay->hide();
        setInteractionEnabled(true);
    }
    activeSidePanel = SIDEPANEL_NONE;
}

void MW::triggerCopyOverlay() {
    if(!viewerWidget->isDisplaying())
        return;
    if(!copyOverlay)
        setupCopyOverlay();

    if(centralWidget->currentViewMode() == MODE_FOLDERVIEW)
        return;
    if(copyOverlay->operationMode() == OVERLAY_COPY) {
        copyOverlay->isHidden() ? copyOverlay->show() : copyOverlay->hide();
    } else {
        copyOverlay->setDialogMode(OVERLAY_COPY);
        copyOverlay->show();
    }
}

void MW::triggerMoveOverlay() {
    if(!viewerWidget->isDisplaying())
        return;
    if(!copyOverlay)
        setupCopyOverlay();

    if(centralWidget->currentViewMode() == MODE_FOLDERVIEW)
        return;
    if(copyOverlay->operationMode() == OVERLAY_MOVE) {
        copyOverlay->isHidden() ? copyOverlay->show() : copyOverlay->hide();
    } else {
        copyOverlay->setDialogMode(OVERLAY_MOVE);
        copyOverlay->show();
    }
}

// quit fullscreen or exit the program
void MW::closeFullScreenOrExit() {
    if(this->isFullScreen()) {
        this->showWindowed();
    } else {
        actionManager->invokeAction("exit");
    }
}

// todo: this is crap, use shared state object
void MW::setCurrentInfo(int _index, int _fileCount, QString _filePath, QString _fileName, QSize _imageSize, qint64 _fileSize, int _pdfPage, int _pdfPageCount, bool slideshow, bool shuffle, bool edited) {
    info.index = _index;
    info.fileCount = _fileCount;
    info.fileName = _fileName;
    info.filePath = _filePath;
    info.imageSize = _imageSize;
    info.fileSize = _fileSize;
    info.pdfPage = _pdfPage;
    info.pdfPageCount = _pdfPageCount;
    info.slideshow = slideshow;
    info.shuffle = shuffle;
    info.edited = edited;
    onInfoUpdated();
}

// todo: nuke and rewrite
void MW::onInfoUpdated() {
    QString posString;
    if(info.fileCount)
        posString = "[ " + QString::number(info.index + 1) + "/" + QString::number(info.fileCount) + " ]";
    QString resString;
    if(info.imageSize.width())
        resString = QString::number(info.imageSize.width()) + " x " + QString::number(info.imageSize.height());
    QString sizeString;
    if(info.fileSize)
        sizeString = this->locale().formattedDataSize(info.fileSize, 1);
    QString pageString;
    if(info.pdfPageCount > 1)
        pageString = tr("page %1/%2").arg(info.pdfPage + 1).arg(info.pdfPageCount);

    if(renameOverlay)
        renameOverlay->setName(info.fileName);

    QString windowTitle;
    if(centralWidget->currentViewMode() == MODE_FOLDERVIEW) {
        windowTitle = tr("Folder view");
        infoBarFullscreen->setInfo("", tr("No file opened."), "");
        infoBarWindowed->setInfo("", tr("No file opened."), "");
    } else if(info.fileName.isEmpty()) {
        windowTitle = qApp->applicationName();
        infoBarFullscreen->setInfo("", tr("No file opened."), "");
        infoBarWindowed->setInfo("", tr("No file opened."), "");
    } else {
        windowTitle = info.fileName;
        if(settings->windowTitleExtendedInfo()) {
            windowTitle.prepend(posString + "  ");
            if(!resString.isEmpty())
                windowTitle.append("  -  " + resString);
            if(!pageString.isEmpty())
                windowTitle.append("  -  " + pageString);
            if(!sizeString.isEmpty())
                windowTitle.append("  -  " + sizeString);
        }

        // toggleable states
        QString states;
        if(info.slideshow)
            states.append(" [slideshow]");
        if(info.shuffle)
            states.append(" [shuffle]");
        if(viewerWidget->lockZoomEnabled())
            states.append(" [zoom lock]");
        if(viewerWidget->lockViewEnabled())
            states.append(" [view lock]");

        if(!settings->infoBarWindowed() && !states.isEmpty())
            windowTitle.append(" -" + states);
        if(info.edited)
            windowTitle.prepend("* ");

        const QString pageSuffix = pageString.isEmpty() ? QString()
                                                        : QStringLiteral("  ") + pageString;
        infoBarFullscreen->setInfo(posString, info.fileName + (info.edited ? "  *" : ""),
                                   resString + pageSuffix + "  " + sizeString);
        infoBarWindowed->setInfo(posString, info.fileName + (info.edited ? "  *" : ""),
                                 resString + pageSuffix + "  " + sizeString + " " + states);
    }
    setWindowTitle(windowTitle);
}

// TODO!!! buffer this in mw
void MW::setImageInfo(const QList<ImageInfoSection> &info) {
    if(imageInfoOverlay)
        imageInfoOverlay->setInfo(info);
}

std::shared_ptr<FolderViewProxy> MW::getFolderView() {
    return folderView;
}

std::shared_ptr<ThumbnailStripProxy> MW::getThumbnailPanel() {
    return docWidget->thumbPanel();
}

// todo: this is crap
void MW::showMessageDirectory(QString dirName) {
    floatingMessage->showMessage(dirName, FloatingMessageIcon::ICON_DIRECTORY, 1700);
}

void MW::showMessageDirectoryEnd() {
    // TODO replace with something nicer (integrate with click overlay?)
    //floatingMessage->showMessage("", FloatingWidgetPosition::RIGHT, FloatingMessageIcon::ICON_RIGHT_EDGE, 400);
}

void MW::showMessageDirectoryStart() {
    // TODO replace with something nicer (integrate with click overlay?)
    //floatingMessage->showMessage("", FloatingWidgetPosition::LEFT, FloatingMessageIcon::ICON_LEFT_EDGE, 400);
}

void MW::showMessageFitWindow() {
    floatingMessage->showMessage(tr("Fit Window"), FloatingMessageIcon::NO_ICON, 350);
}

void MW::showMessageFitWidth() {
    floatingMessage->showMessage(tr("Fit Width"), FloatingMessageIcon::NO_ICON, 350);
}

void MW::showMessageFitOriginal() {
    floatingMessage->showMessage(tr("Fit 1:1"), FloatingMessageIcon::NO_ICON, 350);
}

void MW::showMessage(QString text) {
    floatingMessage->showMessage(text,  FloatingMessageIcon::NO_ICON, 1500);
}

void MW::showMessage(QString text, int duration) {
    floatingMessage->showMessage(text, FloatingMessageIcon::NO_ICON, duration);
}

void MW::showMessageSuccess(QString text) {
    floatingMessage->showMessage(text,  FloatingMessageIcon::ICON_SUCCESS, 1500);
}

void MW::showWarning(QString text) {
    floatingMessage->showMessage(text,  FloatingMessageIcon::ICON_WARNING, 1500);
}

void MW::showError(QString text) {
    floatingMessage->showMessage(text,  FloatingMessageIcon::ICON_ERROR, 2800);
}

// Both prompts below used QMessageBox/QInputDialog, which keep the native
// Windows frame and ignore the Material chrome: in a Material theme they looked
// like a different application (dark caption, English buttons). They are plain
// MaterialDialogs instead, so the stock themes still get native chrome and the
// Material ones get the same card as the preferences dialog.
static void buildPromptBody(QDialog *dialog, const QString &text, QWidget *field,
                            QDialogButtonBox::StandardButtons buttons) {
    auto *root = new QVBoxLayout(dialog);
    root->setContentsMargins(0, 0, 0, 0);
    auto *body = new QWidget(dialog);
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(16, 8, 16, 16);
    layout->setSpacing(14);

    auto *label = new QLabel(text, body);
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
    layout->addWidget(label);
    if(field)
        layout->addWidget(field);

    auto *box = new QDialogButtonBox(buttons, body);
    if(auto *ok = box->button(QDialogButtonBox::Ok))
        ok->setDefault(true);
    if(auto *yes = box->button(QDialogButtonBox::Yes))
        yes->setDefault(true);
    layout->addWidget(box, 0, Qt::AlignRight);
    root->addWidget(body);

    QObject::connect(box, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    QObject::connect(box, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
}

bool MW::showConfirmation(QString title, QString msg) {
    MaterialDialog dialog(this);
    dialog.setWindowTitle(title);
    buildPromptBody(&dialog, msg, nullptr, QDialogButtonBox::Yes | QDialogButtonBox::No);
    dialog.enableMaterialChrome();
    dialog.setMinimumWidth(400);
    return dialog.exec() == QDialog::Accepted;
}

QString MW::getPassword(QString title, QString prompt) {
    MaterialDialog dialog(this);
    dialog.setWindowTitle(title);
    auto *edit = new QLineEdit(&dialog);
    edit->setEchoMode(QLineEdit::Password);
    buildPromptBody(&dialog, prompt, edit, QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    dialog.enableMaterialChrome();
    dialog.setMinimumWidth(400);
    edit->setFocus();
    if(dialog.exec() != QDialog::Accepted)
        return QString();
    return edit->text();
}

void MW::readSettings() {
    showInfoBarFullscreen = settings->infoBarFullscreen();
    showInfoBarWindowed = settings->infoBarWindowed();
    adaptToWindowState();

    if(navBar) {
        navBar->setMode(settings->navBarMode());
        navBar->setButtons(settings->navBarShortcuts(), settings->navBarIcons());
        // takes the current page into account as well
        updateNavBarVisibility();
    }
}

// todo: remove/rename?
void MW::applyWindowedBackground() {
#ifdef USE_KDE_BLUR
    QWindow* window = this->windowHandle();
    if(window) {
        if(settings->backgroundOpacity() == 1.0)
            KWindowEffects::enableBlurBehind(window, false);
        else
            KWindowEffects::enableBlurBehind(window, settings->blurBackground());
    }
#endif
}

void MW::applyFullscreenBackground() {
#ifdef USE_KDE_BLUR
    QWindow* window = this->windowHandle();
    if(window)
        KWindowEffects::enableBlurBehind(window, false);
#endif
}

// changes ui elements according to fullscreen state
void MW::adaptToWindowState() {
    // the custom frame belongs to the windowed mode; fullscreen has no chrome
    if(titleBar)
        titleBar->setVisible(!isFullScreen());
    layoutResizeGrips();
    docWidget->hideFloatingPanel();
    if(isFullScreen()) { //-------------------------------------- fullscreen ---
        applyFullscreenBackground();
        infoBarWindowed->hide();

        if(showInfoBarFullscreen)
            infoBarFullscreen->showWhenReady();
        else
            infoBarFullscreen->hide();    

        auto pos = settings->panelPosition();
        if(!settings->panelEnabled() || pos == PANEL_BOTTOM || pos == PANEL_LEFT)
            controlsOverlay->show();
        else
            controlsOverlay->hide();
    } else { //------------------------------------------------------ window ---
        applyWindowedBackground();
        infoBarFullscreen->hide();

        if(showInfoBarWindowed)
            infoBarWindowed->show();
        else
            infoBarWindowed->hide();

        controlsOverlay->hide();
    }
    folderView->onFullscreenModeChanged(isFullScreen());
    docWidget->onFullscreenModeChanged(isFullScreen());
    viewerWidget->onFullscreenModeChanged(isFullScreen());
}

void MW::paintEvent(QPaintEvent *event) {
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    FloatingWidgetContainer::paintEvent(event);
}

void MW::leaveEvent(QEvent *event) {
    QWidget::leaveEvent(event);
    docWidget->hideFloatingPanel(true);
}

// block native tab-switching so we can use it in shortcuts
//bool MW::focusNextPrevChild(bool) {
//    return false;
//}



bool MW::showPdf(std::shared_ptr<PdfPageModel> model, const QString &path) {
    Q_UNUSED(path)
    if(!mPdfPageConnected && viewerWidget) {
        // the paged viewer is a child that outlives individual documents
        connect(viewerWidget.get(), &ViewerWidget::currentPageChanged,
                this, &MW::pdfPageChanged);
        // the fit button follows the zoom the paged viewer ends up at
        connect(viewerWidget.get(), &ViewerWidget::pdfZoomChanged, this, [this] {
            if(navBar)
                navBar->refreshFitCycleIcon();
        });
        mPdfPageConnected = true;
    }
    const bool shown = viewerWidget->showPdf(model);
    if(shown) {
        navBar->refreshFitCycleIcon();
        updateCropPanelData();
    }
    return shown;
}

bool MW::isPdfView() {
    return viewerWidget && viewerWidget->isPdf();
}

void MW::setModelView(bool enabled) {
    if(viewerWidget)
        viewerWidget->setModelMode(enabled);
}

void MW::pdfNextPage() {
    viewerWidget->pdfNextPage();
}

void MW::pdfPrevPage() {
    viewerWidget->pdfPrevPage();
}

int MW::pdfCurrentPage() {
    return viewerWidget ? viewerWidget->pdfCurrentPage() : 0;
}

QImage MW::pdfCurrentPageImage() {
    return viewerWidget ? viewerWidget->pdfCurrentPageImage() : QImage();
}

int MW::pdfPageCount() {
    return viewerWidget->pdfPageCount();
}
