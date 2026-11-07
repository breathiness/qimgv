#include "folderview.h"
#include "ui_folderview.h"
#include "uistyle.h"

#include <QDir>
#include <QFileInfo>
#include <QFontMetrics>
#include <QContextMenuEvent>
#include <QMenu>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>
#include "utils/imagelib.h" 

FolderView::FolderView(QWidget *parent) :
    FloatingWidgetContainer(parent),
    ui(new Ui::FolderView)
{
    ui->setupUi(this);

    // Without the stylesheet nothing fixes these sizes any more; restore what the
    // rules in style-template.qss used to set (FolderViewTopBar, PathBar QLabel).
    if(UiStyle::useQlementine()) {
        const QFontMetrics fm(font());
        const int textHeight = fm.height();
        const int textPadding = qMax(static_cast<int>(textHeight * 0.10f), 4);
        ui->topBar->setFixedHeight(qMax(textHeight + textPadding * 2 + 4 * 2, 38));
        // keeps the path off the rounded border of its field
        ui->pathLabel->setContentsMargins(8, 0, 9, 0);
    }

    // ------- filesystem view --------
    // The font has to be restated for the tree: the stylesheet used to do it. It
    // is defined in points by default and in pixels by Qlementine, where
    // pointSize() is -1 - a "-1pt" rule makes Qt warn for every row and leaves
    // the view with no usable font.
    //
    // Under Qlementine the font is set on the widget instead: a per-widget
    // stylesheet wraps the view in QStyleSheetStyle, which keeps the palette it
    // was polished with, so switching themes left the tree in the old (black on
    // black) colours until a restart.
    const QFont appFont = QApplication::font();
    if(UiStyle::useQlementine()) {
        ui->dirTreeView->setFont(appFont);
    } else if(appFont.pointSize() > 0) {
        ui->dirTreeView->setStyleSheet(QString("font: %1pt;").arg(appFont.pointSize()));
    } else if(appFont.pixelSize() > 0) {
        ui->dirTreeView->setStyleSheet(QString("font: %1px;").arg(appFont.pixelSize()));
    }

    optionsPopup = new FVOptionsPopup();
    popupTimerClutch.start();

    dirModel = new FileSystemModelCustom(this);
    dirModel->setFilter(QDir::NoDotAndDotDot | QDir::AllDirs);
    ui->dirTreeView->setModel(dirModel);

    QHeaderView* header = ui->dirTreeView->header();
    header->hideSection(1); // size
    header->hideSection(2); // type
    header->hideSection(3); // mod date

#ifdef _WIN32
    dirModel->setRootPath("");
#else
    dirModel->setRootPath(QDir::homePath());
    QModelIndex idx = dirModel->index(dirModel->rootPath());
    ui->dirTreeView->setRootIndex(idx);
#endif
    // -------------------------------
    ui->upButton->setAction("goUp");
    ui->upButton->setIconPath(":res/icons/common/buttons/panel/up16.png");
    ui->upButton->setTriggerMode(TriggerMode::ClickTrigger);
    ui->settingsButton->setAction("openSettings");
    ui->settingsButton->setIconPath(":res/icons/common/buttons/panel/settings16.png");
    ui->exitButton->setAction("exit");
    ui->exitButton->setIconPath(":res/icons/common/buttons/panel/close16.png");
    ui->exitButton->setIconOffset(-1, 0);
    ui->docViewButton->setAction("documentView");
    ui->docViewButton->setIconPath(":res/icons/common/buttons/panel/document-view20.png");
    ui->togglePlacesPanelButton->setCheckable(true);
    ui->togglePlacesPanelButton->setIconPath(":res/icons/common/buttons/panel/toggle-panel20.png");
    ui->togglePlacesPanelButton->setIconOffset(1, 0);

    ui->optionsPopupButton->setCheckable(true);
    ui->optionsPopupButton->setIconPath(":res/icons/common/buttons/panel/folderview20.png");

    ui->sortingComboBox->setIconPath(":res/icons/common/other/sorting-mode16.png");

    ui->newBookmarkButton->setIconPath(":res/icons/common/buttons/panel-small/add-new12.png");
    ui->homeButton->setIconPath(":res/icons/common/buttons/panel-small/home12.png");
    ui->rootButton->setIconPath(":res/icons/common/buttons/panel-small/root12.png");

    int min = ui->thumbnailGrid->THUMBNAIL_SIZE_MIN;
    int max = ui->thumbnailGrid->THUMBNAIL_SIZE_MAX;
    int step = ui->thumbnailGrid->ZOOM_STEP;

    ui->zoomSlider->setMinimum(min / step);
    ui->zoomSlider->setMaximum(max / step);
    ui->zoomSlider->setSingleStep(1);
    ui->zoomSlider->setPageStep(1);

    ui->splitter->setStretchFactor(1, 50);

    connect(ui->thumbnailGrid, &FolderGridView::thumbnailsRequested,  this, &FolderView::thumbnailsRequested);
    connect(ui->thumbnailGrid, &FolderGridView::thumbnailSizeChanged, this, &FolderView::onThumbnailSizeChanged);
    connect(ui->thumbnailGrid, &FolderGridView::itemActivated,   this, &FolderView::itemActivated);
    connect(ui->thumbnailGrid, &FolderGridView::draggedOut,      this, &FolderView::draggedOut);
    connect(ui->thumbnailGrid, &FolderGridView::draggedOver,     this, &FolderView::draggedOver);
    connect(ui->thumbnailGrid, &FolderGridView::droppedInto,     this, &FolderView::droppedInto);

    connect(ui->bookmarksWidget, &BookmarksWidget::bookmarkClicked, this, &FolderView::onBookmarkClicked);

    connect(ui->newBookmarkButton, &IconButton::clicked, this, &FolderView::newBookmark);
    connect(ui->homeButton, &IconButton::clicked, this, &FolderView::onHomeBtn);
    connect(ui->rootButton, &IconButton::clicked, this, &FolderView::onRootBtn);

    connect(ui->zoomSlider, &QSlider::valueChanged, this, &FolderView::onZoomSliderValueChanged);
    connect(ui->sortingComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &FolderView::onSortingSelected);
    connect(ui->togglePlacesPanelButton, &ActionButton::toggled, this, &FolderView::onPlacesPanelButtonChecked);

    connect(ui->optionsPopupButton, &IconButton::toggled, this, &FolderView::onOptionsPopupButtonToggled);
    connect(optionsPopup, &FVOptionsPopup::dismissed, this, &FolderView::onOptionsPopupDismissed);
    connect(optionsPopup, &FVOptionsPopup::viewModeSelected, this, &FolderView::onViewModeSelected);

    connect(ui->dirTreeView, &TreeViewCustom::droppedIn, this, &FolderView::onDroppedInByIndex);
    connect(ui->dirTreeView, &TreeViewCustom::tabbedOut, this, &FolderView::onTreeViewTabOut);
    connect(ui->bookmarksWidget, &BookmarksWidget::droppedIn, this, &FolderView::moveUrlsRequested); // ask what to do via popup? copy or move

    ui->sortingComboBox->setItemDelegate(new QStyledItemDelegate(ui->sortingComboBox));
    ui->sortingComboBox->view()->setTextElideMode(Qt::ElideNone);

    connect(ui->splitter, &QSplitter::splitterMoved, this, &FolderView::onSplitterMoved);

    readSettings();

    QSizePolicy sp_retain = sizePolicy();
    sp_retain.setRetainSizeWhenHidden(true);
    setSizePolicy(sp_retain);
    connect(settings, &Settings::settingsChanged, this, &FolderView::readSettings);

    hide();
}

void FolderView::readSettings() {
    ui->thumbnailGrid->setThumbnailSize(settings->folderViewIconSize());
    ui->thumbnailGrid->setShowLabels((settings->folderViewMode() != FV_SIMPLE));
    ui->togglePlacesPanelButton->setChecked(settings->placesPanel());

    setPlacesPanel(settings->placesPanel());
    ui->bookmarksWidget->setVisible(settings->placesPanelBookmarksExpanded());
    ui->dirTreeView->setVisible(settings->placesPanelTreeExpanded());

    // A theme switch swaps the application palette, but the tree keeps painting
    // with the one it was polished with: after switching to a dark theme the
    // places panel stayed white with dark text. Re-seed it from the palette the
    // style just installed.
    if(UiStyle::useQlementine()) {
        const QPalette appPalette = QApplication::palette();
        ui->dirTreeView->setPalette(appPalette);
        ui->dirTreeView->viewport()->setPalette(appPalette);
        ui->dirTreeView->viewport()->update();
    }

    QList<int> sizes;
    sizes << settings->placesPanelWidth() << 1;
    ui->splitter->setSizes(sizes);
}

void FolderView::onSplitterMoved() {
    settings->setPlacesPanelWidth(ui->placesPanel->width());
}

void FolderView::onPlacesPanelButtonChecked(bool mode) {
    setPlacesPanel(mode);
    settings->setPlacesPanel(mode);
}

void FolderView::setPlacesPanel(bool mode) {
    if(width() >= 600)
        ui->placesPanel->setVisible(mode);
}

void FolderView::toggleBookmarks() {
    if(ui->bookmarksWidget->isVisible())
        ui->bookmarksWidget->hide();
    else
        ui->bookmarksWidget->show();
    settings->setPlacesPanelBookmarksExpanded(ui->bookmarksWidget->isVisible());
}

void FolderView::toggleFilesystemView() {
    if(ui->dirTreeView->isVisible())
        ui->dirTreeView->hide();
    else
        ui->dirTreeView->show();
    settings->setPlacesPanelTreeExpanded(ui->dirTreeView->isVisible());
}

void FolderView::onTreeViewTabOut() {
    ui->thumbnailGrid->setFocus();
    // TODO: maybe add a focus change indication? a border blink or something
}

// TODO: ask what to do
void FolderView::onDroppedInByIndex(QList<QString> paths, QModelIndex index) {
    emit moveUrlsRequested(paths, dirModel->filePath(index));
}

void FolderView::onOptionsPopupButtonToggled(bool mode) {
    if(mode) {
        // Fixes popup being shown again right after dismissing it
        // by clicking on this toggle button.
        // This issue is only present on windows (different Qt::Popup behavior)
        if(popupTimerClutch.elapsed() < 10) {
            ui->optionsPopupButton->setChecked(false);
            return;
        }
        QPoint pos = ui->optionsPopupButton->geometry().bottomRight() -
                     QPoint(optionsPopup->width(), 0);
        optionsPopup->showAt(mapToGlobal(pos));
    }
}

void FolderView::onOptionsPopupDismissed() {
    popupTimerClutch.start();
    ui->optionsPopupButton->setChecked(false);
}

void FolderView::onViewModeSelected(FolderViewMode mode) {
    settings->setFolderViewMode(mode);
    ui->thumbnailGrid->setShowLabels((settings->folderViewMode() != FV_SIMPLE));
    emit showFoldersChanged((mode == FV_EXT_FOLDERS));
}

void FolderView::onThumbnailSizeChanged(int newSize) {
    ui->zoomSlider->setValue(newSize / ui->thumbnailGrid->ZOOM_STEP);
    settings->setFolderViewIconSize(newSize);
}

void FolderView::onZoomSliderValueChanged(int value) {
    ui->thumbnailGrid->setThumbnailSize(value * ui->thumbnailGrid->ZOOM_STEP);
}

// changed by user via combobox
void FolderView::onSortingSelected(int mode) {
    emit sortingSelected(static_cast<SortingMode>(mode));
}

void FolderView::onSortingChanged(SortingMode mode) {
    ui->sortingComboBox->blockSignals(true);
    ui->sortingComboBox->setCurrentIndex(static_cast<int>(mode));
    ui->sortingComboBox->blockSignals(false);
}

FolderView::~FolderView() {
    delete ui;
}

// probably unneeded
void FolderView::show() {
    QWidget::show();
    ui->thumbnailGrid->setFocus();
}

// probably unneeded
void FolderView::hide() {
    QWidget::hide();
    ui->thumbnailGrid->clearFocus();
}

void FolderView::onFullscreenModeChanged(bool mode) {
    ui->exitButton->setHidden(!mode);
    if(mode) // hide 2px spacer
        ui->panelRightEdgeSpacer->changeSize(0, 20, QSizePolicy::Fixed, QSizePolicy::Fixed);
    else // show spacer
        ui->panelRightEdgeSpacer->changeSize(2, 20, QSizePolicy::Fixed, QSizePolicy::Fixed);
    ui->topBar->layout()->invalidate();
}

void FolderView::focusInEvent(QFocusEvent *event) {
    Q_UNUSED(event)
    ui->thumbnailGrid->setFocus();
}

void FolderView::populate(int count) {
    ui->thumbnailGrid->populate(count);
}

void FolderView::setThumbnail(int pos, std::shared_ptr<Thumbnail> thumb) {
    ui->thumbnailGrid->setThumbnail(pos, thumb);
}

void FolderView::select(QList<int> indices) {
    ui->thumbnailGrid->select(indices);
}

void FolderView::select(int index) {
    ui->thumbnailGrid->select(index);
}

QList<int> FolderView::selection() {
    return ui->thumbnailGrid->selection();
}

void FolderView::focusOn(int index) {
    ui->thumbnailGrid->focusOn(index);
}

void FolderView::focusOnSelection() {
    ui->thumbnailGrid->focusOnSelection();
}

void FolderView::onHomeBtn() {
    emit directorySelected(QDir::homePath());
}

void FolderView::onRootBtn() {
    emit directorySelected("/");
}

void FolderView::setDirectoryPath(QString path) {
#ifdef __linux
    if(path.startsWith(QDir::homePath())) {
        if(dirModel->rootPath() != QDir::homePath()) {
            dirModel->setRootPath(QDir::homePath());
            QModelIndex idx = dirModel->index(dirModel->rootPath());
            ui->dirTreeView->setRootIndex(idx);
        }
    } else {
        dirModel->setRootPath("/");
        QModelIndex idx = dirModel->index(dirModel->rootPath());
        ui->dirTreeView->setRootIndex(idx);
    }
#endif
    ui->pathLabel->setText(path);
    // the label elides a long path, so keep the whole one reachable
    ui->pathLabel->setToolTip(path);

    if(ui->dirTreeView->currentIndex().data() == path)
        return;

    ui->bookmarksWidget->onPathChanged(path);

    QModelIndex targetIndex = dirModel->index(path);
    bool keepExpand = ui->dirTreeView->isExpanded(targetIndex);
    bool collapse = !ui->dirTreeView->isExpanded(ui->dirTreeView->currentIndex().parent());

    if(collapse)
        ui->dirTreeView->collapseAll();
    ui->dirTreeView->setCurrentIndex(targetIndex);

    if(keepExpand)
        ui->dirTreeView->expand(targetIndex);

    // ok, i'm done with this shit. none of the "solutions" work
    // just do scrollTo after a delay and hope that model is loaded by then
    // larger than ~150ms becomes too noticeable
    QTimer::singleShot(150, this, &FolderView::fsTreeScrollToCurrent);
}

void FolderView::fsTreeScrollToCurrent() {
    ui->dirTreeView->scrollTo(ui->dirTreeView->currentIndex());
}

void FolderView::onTreeViewClicked(QModelIndex index) {
    emit directorySelected(dirModel->fileInfo(index).absoluteFilePath());
}

void FolderView::onBookmarkClicked(QString dirPath) {
    emit directorySelected(dirPath);
}

void FolderView::newBookmark() {
    QFileDialog dialog;
    dialog.setDirectory(QDir::homePath());
    dialog.setWindowTitle("Select directory");
    dialog.setWindowModality(Qt::ApplicationModal);
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly);
    dialog.setOption(QFileDialog::DontResolveSymlinks);
    connect(&dialog, &QFileDialog::fileSelected, ui->bookmarksWidget, &BookmarksWidget::addBookmark);
    dialog.exec();
}

void FolderView::addItem() {
    ui->thumbnailGrid->addItem();
}

void FolderView::insertItem(int index) {
    ui->thumbnailGrid->insertItem(index);
}

void FolderView::removeItem(int index) {
    ui->thumbnailGrid->removeItem(index);
}

void FolderView::reloadItem(int index) {
    ui->thumbnailGrid->reloadItem(index);
}

void FolderView::setDragHover(int index) {
    ui->thumbnailGrid->setDragHover(index);
}

// prevent passthrough to parent
void FolderView::wheelEvent(QWheelEvent *event) {
    event->accept();
}

void FolderView::paintEvent(QPaintEvent *) {
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    if(UiStyle::paintBackground(this, p, false)) {
        paintChrome(p);
        return;
    }
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

// Parts of the folder view that only a stylesheet used to paint, drawn here in
// the colours the Qlementine theme uses, before the children (they draw on top).
void FolderView::paintChrome(QPainter &p) {
    UiStyle::ChromePalette pal;
    if(!UiStyle::chromePalette(pal))
        return;
    const QRect topBarRect = ui->topBar->geometry();
    p.fillRect(topBarRect, pal.bar);

    // The path field: only the text sits in it, so the up button next to it reads
    // as a button of its own instead of part of the field.
    const QRect labelRect(ui->pathLabel->mapTo(this, QPoint(0, 0)), ui->pathLabel->size());
    const QRectF field = QRectF(labelRect).adjusted(0.5, 4.5, -0.5, -4.5);
    if(field.width() > 2 && field.height() > 2) {
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setBrush(pal.surface);
        p.setPen(QPen(pal.surfaceLine, 1.0));
        p.drawRoundedRect(field, pal.radius, pal.radius);
    }

    // 1px seam under the top bar
    const QRect contentsRect = ui->contentsWidget->geometry();
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QPen(pal.seam, 1));
    p.drawLine(contentsRect.topLeft(), contentsRect.topRight());
    p.setBrush(Qt::NoBrush);
}

void FolderView::resizeEvent(QResizeEvent *event) {
    Q_UNUSED(event)
    if(width() < 600)
        ui->placesPanel->setVisible(false);
    else if (ui->togglePlacesPanelButton->isChecked())
        ui->placesPanel->setVisible(true);

    if(width() < 510) {
        ui->zoomSlider->setVisible(false);
        ui->zoomSliderSpacer->changeSize(0, 20, QSizePolicy::Fixed, QSizePolicy::Fixed);
        ui->pathbarSpacer->changeSize(0, 20, QSizePolicy::Fixed, QSizePolicy::Fixed);
        ui->topBar->layout()->invalidate();
    } else {
        ui->zoomSlider->setVisible(true);
        ui->zoomSliderSpacer->changeSize(3, 20, QSizePolicy::Fixed, QSizePolicy::Fixed);
        ui->pathbarSpacer->changeSize(12, 20, QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        ui->topBar->layout()->invalidate();
    }
}

// ----------------------------------------------------------------------------
// menu icons come from the same vector set as the rest of the interface; they are
// masks, so they are tinted with the icon colour of the theme here
static QIcon folderMenuIcon(const QString &themeName) {
    const qreal dpr = qApp->devicePixelRatio();
    QPixmap pixmap = UiStyle::vectorIcon(themeName).pixmap(QSize(16, 16) * qMax(1.0, dpr));
    if(pixmap.isNull())
        return QIcon();
    pixmap.setDevicePixelRatio(qMax(1.0, dpr));
    ImageLib::recolor(pixmap, settings->colorScheme().icons);
    return QIcon(pixmap);
}

QString FolderView::directoryPath() const {
    return ui->pathLabel->text();
}

void FolderView::contextMenuEvent(QContextMenuEvent *event) {
    showContextMenu(event->globalPos());
    event->accept();
}

// Only what makes sense on this page: the viewer's menu (zoom, rotate, crop, ...)
// has nothing to act on here.
void FolderView::showContextMenu(const QPoint &globalPos) {
    const QString path = directoryPath();
    QMenu menu(this);

    const int row = ui->thumbnailGrid->selection().isEmpty() ? -1 : ui->thumbnailGrid->lastSelected();
    if(row >= 0) {
        QAction *open = menu.addAction(folderMenuIcon("document/open"), tr("Open"));
        connect(open, &QAction::triggered, this, [this, row]() { emit itemActivated(row); });
        menu.addSeparator();
    }

    // Folder-only command: on a file it either did nothing or answered with
    // "Select a folder to dissolve." after the menu had already closed.
    const QString selectedName = row >= 0 ? ui->thumbnailGrid->itemName(row) : QString();
    const bool selectedIsFolder = row >= 0 && !selectedName.isEmpty()
                                  && QFileInfo(QDir(directoryPath()).filePath(selectedName)).isDir();
    if(selectedIsFolder) {
        QAction *dissolve = menu.addAction(folderMenuIcon("action/ungroup"), tr("Dissolve folder"));
        connect(dissolve, &QAction::triggered, this, [this]() {
            // let the menu close before anything asks a question
            QTimer::singleShot(0, this, [this]() { emit dissolveFolderRequested(); });
        });
    }

    menu.addSeparator();

    // file operations on the selection; the clipboard is kept by Core, so these
    // are the same actions the shortcuts run
    if(row >= 0) {
        QAction *copy = menu.addAction(folderMenuIcon("action/copy"), tr("Copy"));
        connect(copy, &QAction::triggered, this, []() { actionManager->invokeAction("copySelection"); });

        QAction *cut = menu.addAction(folderMenuIcon("action/cut"), tr("Cut"));
        connect(cut, &QAction::triggered, this, []() { actionManager->invokeAction("cutSelection"); });
    }

    QAction *paste = menu.addAction(folderMenuIcon("action/paste"), tr("Paste"));
    connect(paste, &QAction::triggered, this, []() { actionManager->invokeAction("pasteSelection"); });

    if(row >= 0) {
        QAction *remove = menu.addAction(folderMenuIcon("action/trash"), tr("Delete"));
        connect(remove, &QAction::triggered, this, []() { actionManager->invokeAction("moveToTrash"); });
    }

    menu.addSeparator();

    QAction *showInFileManager = menu.addAction(folderMenuIcon("file/folder-open"), tr("Show in file manager"));
    connect(showInFileManager, &QAction::triggered, this, [path]() {
        if(!path.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });

    QAction *copyPath = menu.addAction(folderMenuIcon("action/copy"), tr("Copy path"));
    connect(copyPath, &QAction::triggered, this, [path]() {
        if(!path.isEmpty())
            QApplication::clipboard()->setText(path);
    });

    menu.addSeparator();

    // ---- sorting: same entries as the combo box in the top bar ----
    QMenu *sortMenu = menu.addMenu(folderMenuIcon("misc/grid"), tr("Sort by"));
    auto *sortGroup = new QActionGroup(sortMenu);
    sortGroup->setExclusive(true);
    const QList<QPair<SortingMode, QString>> sortingModes = {
        {SORT_NAME,        tr("A - Z")},
        {SORT_NAME_DESC,   tr("Z - A")},
        {SORT_SIZE,        tr("Size")},
        {SORT_SIZE_DESC,   tr("Size (desc)")},
        {SORT_TIME,        tr("Oldest")},
        {SORT_TIME_DESC,   tr("Newest")},
    };
    const SortingMode currentSorting = settings->sortingMode();
    for(const auto &entry : sortingModes) {
        QAction *action = sortMenu->addAction(entry.second);
        action->setCheckable(true);
        action->setChecked(entry.first == currentSorting);
        sortGroup->addAction(action);
        const SortingMode mode = entry.first;
        connect(action, &QAction::triggered, this, [this, mode]() {
            ui->sortingComboBox->setCurrentIndex(mode);
            emit sortingSelected(mode);
        });
    }

    // ---- view mode ----
    QMenu *viewMenu = menu.addMenu(folderMenuIcon("misc/grid"), tr("View"));
    auto *viewGroup = new QActionGroup(viewMenu);
    viewGroup->setExclusive(true);
    const QList<QPair<FolderViewMode, QString>> viewModes = {
        {FV_SIMPLE,      tr("Simple")},
        {FV_EXTENDED,    tr("Extended")},
        {FV_EXT_FOLDERS, tr("Extended + folders")},
    };
    const FolderViewMode currentView = settings->folderViewMode();
    for(const auto &entry : viewModes) {
        QAction *action = viewMenu->addAction(entry.second);
        action->setCheckable(true);
        action->setChecked(entry.first == currentView);
        viewGroup->addAction(action);
        const FolderViewMode mode = entry.first;
        connect(action, &QAction::triggered, this, [this, mode]() { onViewModeSelected(mode); });
    }

    QAction *showHidden = menu.addAction(tr("Show hidden files"));
    showHidden->setCheckable(true);
    showHidden->setChecked(settings->showHiddenFiles());
    connect(showHidden, &QAction::triggered, this, [this, path](bool checked) {
        settings->setShowHiddenFiles(checked);
        // the file list is built by the directory manager: ask for the same folder
        // again so it is rescanned with the new setting
        if(!path.isEmpty())
            emit directorySelected(path);
    });

    menu.addSeparator();

    QAction *addBookmark = menu.addAction(folderMenuIcon("action/bookmark-add"), tr("Add bookmark"));
    connect(addBookmark, &QAction::triggered, this, [this, path]() {
        if(!path.isEmpty())
            ui->bookmarksWidget->addBookmark(path);
    });

    QAction *refresh = menu.addAction(folderMenuIcon("action/refresh"), tr("Refresh"));
    connect(refresh, &QAction::triggered, this, [this, path]() {
        if(!path.isEmpty())
            emit directorySelected(path);
    });

    menu.addSeparator();

    QAction *settingsAction = menu.addAction(folderMenuIcon("navigation/settings"), tr("Settings"));
    connect(settingsAction, &QAction::triggered, this, []() {
        actionManager->invokeAction("openSettings");
    });

    QAction *closeFolderView = menu.addAction(folderMenuIcon("action/close"), tr("Exit folder view"));
    connect(closeFolderView, &QAction::triggered, this, []() {
        actionManager->invokeAction("documentView");
    });

    menu.exec(globalPos);
}
