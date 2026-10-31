#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include "gui/dialogs/navbarsettingspage.h"
#include "uistyle.h"

#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    this->setWindowTitle(tr("Preferences — ") + qApp->applicationName());

    ui->shortcutsTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);   
    ui->aboutAppTextBrowser->viewport()->setAutoFillBackground(false);
    ui->versionLabel->setText("" + QApplication::applicationVersion());
    ui->qtVersionLabel->setText(qVersion());
    ui->appIconLabel->setPixmap(QIcon(":/res/icons/common/logo/app/22.png").pixmap(22,22));
    ui->qtIconLabel->setPixmap(QIcon(":/res/icons/common/logo/3rdparty/qt22.png").pixmap(22,16));

    // fake combobox that acts as a menu button
    // less code than using pushbutton with menu
    // will be replaced with something custom later
    connect(ui->themeSelectorComboBox, qOverload<int>(&QComboBox::currentIndexChanged), [this](int index) {
        ui->themeSelectorComboBox->blockSignals(true);
        ui->themeSelectorComboBox->setCurrentIndex(index);
        ui->themeSelectorComboBox->blockSignals(false);
        if(UiStyle::useQlementine()) {
            // two entries only: Dark, Light (see setupQlementineThemePage())
            if(index == 0) {
                mQlementinePresetTid = COLORS_DARK;
                setColorScheme(ThemeStore::colorScheme(COLORS_DARK));
                settings->setColorTid(COLORS_DARK);
            } else if(index == 1) {
                mQlementinePresetTid = COLORS_LIGHT;
                setColorScheme(ThemeStore::colorScheme(COLORS_LIGHT));
                settings->setColorTid(COLORS_LIGHT);
            }
            return;
        }
        switch(index) {
            case 0: setColorScheme(ThemeStore::colorScheme(COLORS_BLACK));    settings->setColorTid(COLORS_BLACK);    break;
            case 1: setColorScheme(ThemeStore::colorScheme(COLORS_DARK));     settings->setColorTid(COLORS_DARK);     break;
            case 2: setColorScheme(ThemeStore::colorScheme(COLORS_DARKBLUE)); settings->setColorTid(COLORS_DARKBLUE); break;
            case 3: setColorScheme(ThemeStore::colorScheme(COLORS_LIGHT));    settings->setColorTid(COLORS_LIGHT);    break;
        }
    });

    connect(ui->useSystemColorsCheckBox, &QCheckBox::toggled, [this](bool useSystemTheme) {
        if(useSystemTheme) {
            ui->themeSelectorComboBox->setCurrentIndex(-1);
            setColorScheme(ThemeStore::colorScheme(COLORS_SYSTEM));
            settings->setColorTid(COLORS_SYSTEM);
        }
        else if(UiStyle::useQlementine()) {
            // Back to the last window theme the user picked. COLORS_CUSTOMIZED
            // would take the colours from the pickers, but in Qlementine mode
            // those describe the theme and cannot be applied back.
            const ColorSchemes tid = mQlementinePresetTid == COLORS_LIGHT ? COLORS_LIGHT : COLORS_DARK;
            setColorScheme(ThemeStore::colorScheme(tid));
            settings->setColorTid(tid);
        }
        else {
            readColorScheme();
            settings->setColorTid(COLORS_CUSTOMIZED);
        }
        ui->themeSelectorComboBox->setEnabled(!useSystemTheme);
        ui->colorConfigSubgroup->setEnabled(!useSystemTheme);
        if(!UiStyle::useQlementine())
            ui->modifySystemSchemeLabel->setVisible(useSystemTheme);
    });

    connect(ui->modifySystemSchemeLabel, &ClickableLabel::clicked, [this]() {
        ui->useSystemColorsCheckBox->setChecked(false);
        setColorScheme(ThemeStore::colorScheme(COLORS_CUSTOMIZED));
        settings->setColorTid(COLORS_CUSTOMIZED);
    });

    ui->colorSelectorAccent->setDescription(tr("Accent color"));
    ui->colorSelectorBackground->setDescription(tr("Windowed mode background"));
    ui->colorSelectorFullscreen->setDescription(tr("Fullscreen mode background"));
    ui->colorSelectorFolderview->setDescription(tr("FolderView background"));
    ui->colorSelectorFolderviewPanel->setDescription(tr("FolderView top panel"));
    ui->colorSelectorText->setDescription(tr("Text color"));
    ui->colorSelectorWidget->setDescription(tr("Widget background"));
    ui->colorSelectorWidgetBorder->setDescription(tr("Widget border"));
    ui->colorSelectorOverlay->setDescription(tr("Overlay background"));
    ui->colorSelectorOverlayText->setDescription(tr("Overlay text"));
    ui->colorSelectorScrollbar->setDescription(tr("Scrollbars"));

    setupUiStyleRow();
    setupQlementineThemePage();

#ifndef USE_KDE_BLUR
    ui->blurBackgroundCheckBox->setEnabled(false);
#endif

#ifndef USE_MPV
    ui->videoPlaybackGroup->setEnabled(false);
    //ui->novideoInfoLabel->setHidden(false);
#else
    //ui->novideoInfoLabel->setHidden(true);
#endif

#ifdef USE_OPENCV
    ui->scalingQualityComboBox->addItem("Bilinear+sharpen (OpenCV)");
    ui->scalingQualityComboBox->addItem("Bicubic (OpenCV)");
    ui->scalingQualityComboBox->addItem("Bicubic+sharpen (OpenCV)");
#endif

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    ui->memoryLimitSpinBox->setEnabled(false);
    ui->memoryLimitLabel->setEnabled(false);
#endif

    if(!settings->supportedFormats().contains("jxl"))
        ui->animatedJxlCheckBox->hide();

    setupSidebar();

    // setup radioBtn groups
    fitModeGrp.addButton(ui->fitModeWindow);
    fitModeGrp.addButton(ui->fitModeWidth);
    fitModeGrp.addButton(ui->fitMode1to1);
    fitModeGrp.addButton(ui->fitModeWindowStretch);
    folderEndGrp.addButton(ui->folderEndSwitchFolder);
    folderEndGrp.addButton(ui->folderEndNoAction);
    folderEndGrp.addButton(ui->folderEndLoop);
    zoomIndGrp.addButton(ui->zoomIndicatorAuto);
    zoomIndGrp.addButton(ui->zoomIndicatorOff);
    zoomIndGrp.addButton(ui->zoomIndicatorOn);

    // readable language names
    langs.insert("de_DE", "Deutsch");
    langs.insert("en_US", "English");
    langs.insert("es_ES", "Español");
    langs.insert("fr_FR", "Français");
    langs.insert("ja_JP", "日本語");
    langs.insert("tr_TR", "Türkçe");
    langs.insert("uk_UA", "Українська");
    langs.insert("zh_CN", "简体中文");
    // fill langs combobox, sorted by locale
    ui->langComboBox->addItems(langs.values());
    // insert system language entry manually at the beginning
    langs.insert("system", "System language");
    ui->langComboBox->insertItem(0, "System language");

    connect(this, &SettingsDialog::settingsChanged, settings, &Settings::sendChangeNotification);
    readSettings();

    adjustSizeToContents();
}
//------------------------------------------------------------------------------
SettingsDialog::~SettingsDialog() {
    delete ui;
}
//------------------------------------------------------------------------------
// an attempt to force minimum width to fit contents
void SettingsDialog::adjustSizeToContents() {
    // general tab
    ui->gridLayout->activate();
    ui->horizontalLayout_28->activate();
    ui->horizontalLayout_19->activate();
    ui->gridLayout_3->activate();
    ui->horizontalLayout_18->activate();
    ui->gridLayout_4->activate();
    ui->horizontalLayout_24->activate();
    ui->gridLayout_5->activate();
    ui->slideshowGroupContents->activate();
    ui->scrollAreaWidgetContents->layout()->activate();
    ui->scrollArea->setMinimumWidth(ui->scrollAreaWidgetContents->minimumSizeHint().width());
    // view tab
    ui->horizontalLayout_29->activate();
    ui->horizontalLayout_31->activate();
    ui->widget->layout()->activate();
    ui->scrollAreaWidgetContents_3->layout()->activate();
    ui->scrollArea_3->setMinimumWidth(ui->scrollAreaWidgetContents_3->minimumSizeHint().width());
    // container
    //ui->stackedWidget->layout()->activate();
    this->setMinimumWidth(sizeHint().width() + 22);

    //qDebug() << "window:" << this->sizeHint() << this->minimumSizeHint() << this->size();
    //qDebug() << "stackedwidget:" << ui->stackedWidget->sizeHint() << ui->stackedWidget->minimumSizeHint() << ui->stackedWidget->size();
    //qDebug() << "scrollarea:" << ui->scrollArea->sizeHint() << ui->scrollArea->minimumSizeHint() << ui->scrollArea->size();
    //qDebug() << "scrollareawidget:" << ui->scrollAreaWidgetContents->sizeHint() << ui->scrollAreaWidgetContents->minimumSizeHint() << ui->scrollAreaWidgetContents->size();
    //qDebug() << "grid" << ui->gridLayout_15->sizeHint();
    //qDebug() << "wtf" << ui->startInFolderViewCheckBox->sizeHint() << ui->startInFolderViewCheckBox->minimumSizeHint();
}
//------------------------------------------------------------------------------
// Qlementine / Classic, on the theme page next to the theme itself. The widget
// style is created before the rest of the application exists, so this can only
// be stored here; the note says it needs a restart.
void SettingsDialog::setupUiStyleRow() {
    auto *label = new QLabel(tr("Interface style:"), this);
    uiStyleComboBox = new QComboBox(this);
    uiStyleComboBox->addItem(tr("Qlementine"));
    uiStyleComboBox->addItem(tr("Classic"));
    uiStyleComboBox->setToolTip(tr("Qlementine draws the whole window from a theme. "
                                   "Classic is the old stylesheet look with separate colours."));
    auto *note = new QLabel(tr("Requires application restart"), this);
    note->setAccessibleName("SNoteText");

    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(7);
    row->addWidget(label);
    row->addWidget(uiStyleComboBox);
    row->addWidget(note);
    row->addStretch();
    ui->verticalLayout_38->insertLayout(0, row);
}
//------------------------------------------------------------------------------
// Qlementine mode: the standard widgets are drawn from a theme file, so what the
// user picks here is the window theme itself rather than a dozen independent
// colours. The preset list shrinks to Dark/Light (with "Use system colors" next
// to it, which follows the desktop), the accent stays editable because the theme
// is recoloured with it, and the rest of the grid is hidden: those values are
// overwritten by the theme on every apply, so leaving them editable only made
// them look broken.
void SettingsDialog::setupQlementineThemePage() {
    if(!UiStyle::useQlementine())
        return;

    ui->loadPresetLabel->setText(tr("Theme:"));
    {
        // filling the list would otherwise emit currentIndexChanged(0) and
        // rewrite the saved theme to "Dark" before the dialog is even read
        const QSignalBlocker blocker(ui->themeSelectorComboBox);
        ui->themeSelectorComboBox->clear();
        ui->themeSelectorComboBox->addItem(tr("Dark"));
        ui->themeSelectorComboBox->addItem(tr("Light"));
    }
    ui->modifySystemSchemeLabel->hide();
    // what "Use system colors" falls back to when switched off again
    mQlementinePresetTid = settings->colorScheme().tid == COLORS_LIGHT ? COLORS_LIGHT : COLORS_DARK;

    // Keep the accent and its label, hide the rest of the grid.
    for(int i = 0; i < ui->gridLayout_2->count(); i++) {
        QWidget *w = ui->gridLayout_2->itemAt(i)->widget();
        if(!w || w == ui->colorSelectorAccent || w == ui->label_33)
            continue;
        w->hide();
    }

    auto *note = new QLabel(tr("Panels and controls follow the window theme. The accent color is the "
                               "only color that can be changed."), this);
    note->setAccessibleName("SNoteText");
    note->setWordWrap(true);
    ui->verticalLayout_23->insertWidget(0, note);
}
//------------------------------------------------------------------------------
void SettingsDialog::resetToDesktopTheme() {
    settings->setColorScheme(ThemeStore::colorScheme(ColorSchemes::COLORS_SYSTEM));
    this->readColorScheme();
}
//------------------------------------------------------------------------------
void SettingsDialog::setupSidebar() {
    // The edge-click options live at the top of the "View" page. They used to
    // sit at the bottom of the "Controls" page, which has no scroll area, so
    // they were easy to miss there.
    edgeClickCheckBox = new QCheckBox(tr("Switch image by clicking window edges"), this);
    ui->verticalLayout_27->insertWidget(2, edgeClickCheckBox);

    edgeVisibleCheckBox = new QCheckBox(tr("Visible edges"), this);
    edgeVisibleCheckBox->setEnabled(false);
    connect(edgeClickCheckBox, &QCheckBox::toggled, edgeVisibleCheckBox, &QCheckBox::setEnabled);
    ui->verticalLayout_27->insertWidget(3, edgeVisibleCheckBox);

    // superseded by the checkboxes above, hide the old ones on the Controls page
    ui->clickableEdgesCheckBox->hide();
    ui->clickableEdgesVisibleCheckBox->hide();

    // large image decoding: preview first, remaining pixels on demand
    twoPassCheckBox = new QCheckBox(tr("Decode large images at display size"), this);
    twoPassCheckBox->setToolTip(tr("Large images are decoded at the size they are shown at first. The rest of the pixels are decoded when you zoom in, crop or save. Makes opening big files much cheaper."));
    // created after readSettings() runs, so pick the current value up here
    twoPassCheckBox->setChecked(settings->twoPassLoading());
    ui->verticalLayout_34->insertWidget(2, twoPassCheckBox);

    twoPassNote = new QLabel(tr("Only visible pixels are decoded up front"), this);
    twoPassNote->setAccessibleName("SNoteText");
    twoPassNote->setWordWrap(true);
    ui->verticalLayout_34->insertWidget(3, twoPassNote);

    // The navigation bar page is built in code instead of the .ui file; it is
    // inserted right after "Controls" so the stacked widget and the sidebar
    // stay index-aligned (SSideBar adds its "Navigation bar" entry in the same
    // position).
    navBarPage = new NavBarSettingsPage(ui->stackedWidget);
    ui->stackedWidget->insertWidget(4, navBarPage);
}
//------------------------------------------------------------------------------
void SettingsDialog::readSettings() {
    ui->loopSlideshowCheckBox->setChecked(settings->loopSlideshow());
    ui->videoPlaybackCheckBox->setChecked(settings->videoPlayback());
    ui->videoPlaybackGroupContents->setEnabled(settings->videoPlayback());
    ui->playSoundsCheckBox->setChecked(settings->playVideoSounds());
    ui->enablePanelCheckBox->setChecked(settings->panelEnabled());
    ui->thumbnailPanelGroupContents->setEnabled(settings->panelEnabled());
    ui->panelFullscreenOnlyCheckBox->setChecked(settings->panelFullscreenOnly());
    ui->squareThumbnailsCheckBox->setChecked(settings->squareThumbnails());
    ui->transparencyGridCheckBox->setChecked(settings->transparencyGrid());
    ui->enableSmoothScrollCheckBox->setChecked(settings->enableSmoothScroll());
    ui->usePreloaderCheckBox->setChecked(settings->usePreloader());
    ui->useThumbnailCacheCheckBox->setChecked(settings->useThumbnailCache());
    ui->smoothUpscalingCheckBox->setChecked(settings->smoothUpscaling());
    ui->expandImageCheckBox->setChecked(settings->expandImage());
    ui->expandImagesGroupContents->setEnabled(settings->expandImage());
    ui->smoothAnimatedImagesCheckBox->setChecked(settings->smoothAnimatedImages());
    ui->bgOpacitySlider->setValue(static_cast<int>(settings->backgroundOpacity() * 100));
    ui->blurBackgroundCheckBox->setChecked(settings->blurBackground());
    ui->sortingComboBox->setCurrentIndex(settings->sortingMode());
    ui->confirmDeleteCheckBox->setChecked(settings->confirmDelete());
    ui->confirmTrashCheckBox->setChecked(settings->confirmTrash());
    ui->unlockMinZoomCheckBox->setChecked(settings->unlockMinZoom());
    ui->sortFoldersCheckBox->setChecked(settings->sortFolders());
    ui->trackpadDetectionCheckBox->setChecked(settings->trackpadDetection());
    if(edgeClickCheckBox) {
        edgeClickCheckBox->setChecked(settings->clickableEdges());
        edgeVisibleCheckBox->setChecked(settings->clickableEdgesVisible());
        edgeVisibleCheckBox->setEnabled(settings->clickableEdges());
    }
    ui->showHiddenFilesCheckBox->setChecked(settings->showHiddenFiles());

    if(settings->zoomIndicatorMode() == INDICATOR_ENABLED)
        ui->zoomIndicatorOn->setChecked(true);
    else if(settings->zoomIndicatorMode() == INDICATOR_AUTO)
        ui->zoomIndicatorAuto->setChecked(true);
    else
        ui->zoomIndicatorOff->setChecked(true);
    ui->showInfoBarFullscreen->setChecked(settings->infoBarFullscreen());
    ui->showInfoBarWindowed->setChecked(settings->infoBarWindowed());
    ui->showExtendedInfoTitle->setChecked(settings->windowTitleExtendedInfo());
    ui->cursorAutohideCheckBox->setChecked(settings->cursorAutohide());
    ui->keepFitModeCheckBox->setChecked(settings->keepFitMode());
    if(settings->focusPointIn1to1Mode() == FOCUS_TOP)
        ui->focus1to1Top->setChecked(true);
    else if(settings->focusPointIn1to1Mode() == FOCUS_CENTER)
        ui->focus1to1Center->setChecked(true);
    else
        ui->focus1to1Cursor->setChecked(true);
    ui->slideshowIntervalSpinBox->setValue(settings->slideshowInterval());
    ui->imageScrollingComboBox->setCurrentIndex(settings->imageScrolling());
    ui->saveOverlayCheckBox->setChecked(settings->showSaveOverlay());
    ui->unloadThumbsCheckBox->setChecked(settings->unloadThumbs());
    if(settings->thumbPanelStyle() == TH_PANEL_SIMPLE)
        ui->thumbStyleSimple->setChecked(true);
    else
        ui->thumbStyleExtended->setChecked(true);
    ui->animatedJxlCheckBox->setChecked(settings->jxlAnimation());
    ui->autoResizeWindowCheckBox->setChecked(settings->autoResizeWindow());
    ui->panelCenterSelectionCheckBox->setChecked(settings->panelCenterSelection());
    ui->useFixedZoomLevelsCheckBox->setChecked(settings->useFixedZoomLevels());
    ui->zoomLevels->setText(settings->zoomLevels());

    if(settings->defaultViewMode() == MODE_FOLDERVIEW)
        ui->startInFolderViewCheckBox->setChecked(true);
    else
        ui->startInFolderViewCheckBox->setChecked(false);

    if(settings->folderEndAction() == FOLDER_END_NO_ACTION)
        ui->folderEndNoAction->setChecked(true);
    else if(settings->folderEndAction() == FOLDER_END_LOOP)
        ui->folderEndLoop->setChecked(true);
    else
        ui->folderEndSwitchFolder->setChecked(true);

    ui->zoomStepSlider->setValue(static_cast<int>(settings->zoomStep() * 100.f));
    onZoomStepSliderChanged(ui->zoomStepSlider->value());

    ui->mouseScrollingSpeedSlider->setValue(static_cast<int>((settings->mouseScrollingSpeed() - 0.5f) / 0.25f));
    onMouseScrollingSpeedSliderChanged(ui->mouseScrollingSpeedSlider->value());

    ui->autoResizeLimitSlider->setValue(static_cast<int>(settings->autoResizeLimit() / 5.f));
    onAutoResizeLimitSliderChanged(ui->autoResizeLimitSlider->value());

    ui->JPEGQualitySlider->setValue(settings->JPEGSaveQuality());
    onJPEGQualitySliderChanged(ui->JPEGQualitySlider->value());

    ui->expandLimitSlider->setValue(settings->expandLimit());
    onExpandLimitSliderChanged(ui->expandLimitSlider->value());

    // thumbnailer threads
    ui->thumbnailerThreadsSlider->setValue(settings->thumbnailerThreadCount());
    onThumbnailerThreadsSliderChanged(ui->thumbnailerThreadsSlider->value());

    ui->memoryLimitSpinBox->setValue(settings->memoryAllocationLimit());

    // language
    QString langName = langs.value(settings->language());
    if(langName.isEmpty() || ui->langComboBox->findText(langName) == -1)
        ui->langComboBox->setCurrentText("en_US");
    else
        ui->langComboBox->setCurrentText(langName);

    // ##### fit mode #####
    if(settings->imageFitMode() == FIT_WINDOW)
        ui->fitModeWindow->setChecked(true);
    else if(settings->imageFitMode() == FIT_WIDTH)
        ui->fitModeWidth->setChecked(true);
    else if(settings->imageFitMode() == FIT_WINDOW_STRETCH)
        ui->fitModeWindowStretch->setChecked(true);
    else
        ui->fitMode1to1->setChecked(true);

    // ##### UI #####
    ui->scalingQualityComboBox->setCurrentIndex(settings->scalingFilter());
    ui->fullscreenCheckBox->setChecked(settings->fullscreenMode());
    ui->pinPanelCheckBox->setChecked(settings->panelPinned());
    ui->panelPositionComboBox->setCurrentIndex(settings->panelPosition());

    // reduce by 10x to have nice granular control in qslider
    ui->panelSizeSlider->setValue(settings->panelPreviewsSize() / 10);

    ui->useSystemColorsCheckBox->setChecked(settings->useSystemColorScheme());
    uiStyleComboBox->setCurrentIndex(settings->uiStyle() == UI_STYLE_CLASSIC ? 1 : 0);
    if(!UiStyle::useQlementine()) // the "modify" shortcut is hidden in Qlementine mode
        ui->modifySystemSchemeLabel->setVisible(settings->useSystemColorScheme());
    ui->themeSelectorComboBox->setEnabled(!settings->useSystemColorScheme());
    ui->colorConfigSubgroup->setEnabled(!settings->useSystemColorScheme());
    
    readColorScheme();
    readShortcuts();
    readScripts();
    if(navBarPage)
        navBarPage->readSettings();
}
//------------------------------------------------------------------------------
void SettingsDialog::saveSettings() {
    // wait for all background stuff to finish
    if(QThreadPool::globalInstance()->activeThreadCount()) {
        QThreadPool::globalInstance()->waitForDone();
    }

    settings->setLoopSlideshow(ui->loopSlideshowCheckBox->isChecked());
    settings->setFullscreenMode(ui->fullscreenCheckBox->isChecked());
    if(ui->fitModeWindow->isChecked())
        settings->setImageFitMode(FIT_WINDOW);
    else if(ui->fitModeWidth->isChecked())
        settings->setImageFitMode(FIT_WIDTH);
    else if(ui->fitModeWindowStretch->isChecked())
        settings->setImageFitMode(FIT_WINDOW_STRETCH);
    else
        settings->setImageFitMode(FIT_ORIGINAL);

    settings->setLanguage(langs.key(ui->langComboBox->currentText()));

    settings->setVideoPlayback(ui->videoPlaybackCheckBox->isChecked());
    settings->setPlayVideoSounds(ui->playSoundsCheckBox->isChecked());
    settings->setPanelEnabled(ui->enablePanelCheckBox->isChecked());
    settings->setPanelFullscreenOnly(ui->panelFullscreenOnlyCheckBox->isChecked());
    settings->setSquareThumbnails(ui->squareThumbnailsCheckBox->isChecked());
    settings->setTransparencyGrid(ui->transparencyGridCheckBox->isChecked());
    settings->setShowHiddenFiles(ui->showHiddenFilesCheckBox->isChecked());
    settings->setEnableSmoothScroll(ui->enableSmoothScrollCheckBox->isChecked());
    settings->setUsePreloader(ui->usePreloaderCheckBox->isChecked());
    if(twoPassCheckBox)
        settings->setTwoPassLoading(twoPassCheckBox->isChecked());
    settings->setUseThumbnailCache(ui->useThumbnailCacheCheckBox->isChecked());
    settings->setSmoothUpscaling(ui->smoothUpscalingCheckBox->isChecked());
    settings->setExpandImage(ui->expandImageCheckBox->isChecked());
    settings->setSmoothAnimatedImages(ui->smoothAnimatedImagesCheckBox->isChecked());

    settings->setBackgroundOpacity(static_cast<qreal>(ui->bgOpacitySlider->value()) / 100.f);
    settings->setBlurBackground(ui->blurBackgroundCheckBox->isChecked());
    settings->setSortingMode(static_cast<SortingMode>(ui->sortingComboBox->currentIndex()));
    settings->setConfirmDelete(ui->confirmDeleteCheckBox->isChecked());
    settings->setConfirmTrash(ui->confirmTrashCheckBox->isChecked());
    settings->setUnlockMinZoom(ui->unlockMinZoomCheckBox->isChecked());
    settings->setSortFolders(ui->sortFoldersCheckBox->isChecked());
    settings->setTrackpadDetection(ui->trackpadDetectionCheckBox->isChecked());
    if(edgeClickCheckBox) {
        settings->setClickableEdges(edgeClickCheckBox->isChecked());
        settings->setClickableEdgesVisible(edgeVisibleCheckBox->isChecked());
    }

    if(ui->zoomIndicatorOn->isChecked())
        settings->setZoomIndicatorMode(INDICATOR_ENABLED);
    else if(ui->zoomIndicatorAuto->isChecked())
        settings->setZoomIndicatorMode(INDICATOR_AUTO);
    else
        settings->setZoomIndicatorMode(INDICATOR_DISABLED);
    settings->setInfoBarFullscreen(ui->showInfoBarFullscreen->isChecked());
    settings->setInfoBarWindowed(ui->showInfoBarWindowed->isChecked());
    settings->setWindowTitleExtendedInfo(ui->showExtendedInfoTitle->isChecked());
    settings->setCursorAutohide(ui->cursorAutohideCheckBox->isChecked());
    settings->setKeepFitMode(ui->keepFitModeCheckBox->isChecked());
    if(ui->focus1to1Top->isChecked())
        settings->setFocusPointIn1to1Mode(FOCUS_TOP);
    else if(ui->focus1to1Center->isChecked())
        settings->setFocusPointIn1to1Mode(FOCUS_CENTER);
    else
        settings->setFocusPointIn1to1Mode(FOCUS_CURSOR);

    settings->setSlideshowInterval(ui->slideshowIntervalSpinBox->value());

    if(ui->startInFolderViewCheckBox->isChecked())
        settings->setDefaultViewMode(MODE_FOLDERVIEW);
    else
        settings->setDefaultViewMode(MODE_DOCUMENT);

    if(ui->folderEndNoAction->isChecked())
        settings->setFolderEndAction(FOLDER_END_NO_ACTION);
    else if(ui->folderEndLoop->isChecked())
        settings->setFolderEndAction(FOLDER_END_LOOP);
    else
        settings->setFolderEndAction(FOLDER_END_GOTO_ADJACENT);

    settings->setScalingFilter(static_cast<ScalingFilter>(ui->scalingQualityComboBox->currentIndex()));
    settings->setImageScrolling(static_cast<ImageScrolling>(ui->imageScrollingComboBox->currentIndex()));
    settings->setShowSaveOverlay(ui->saveOverlayCheckBox->isChecked());
    settings->setUnloadThumbs(ui->unloadThumbsCheckBox->isChecked());
    if(ui->thumbStyleSimple->isChecked())
        settings->setThumbPanelStyle(TH_PANEL_SIMPLE);
    else
        settings->setThumbPanelStyle(TH_PANEL_EXTENDED);
    settings->setJxlAnimation(ui->animatedJxlCheckBox->isChecked());
    settings->setAutoResizeWindow(ui->autoResizeWindowCheckBox->isChecked());
    settings->setPanelCenterSelection(ui->panelCenterSelectionCheckBox->isChecked());
    settings->setUseFixedZoomLevels(ui->useFixedZoomLevelsCheckBox->isChecked());
    settings->setZoomLevels(ui->zoomLevels->text());

    settings->setPanelPinned(ui->pinPanelCheckBox->isChecked());
    int panelPos = ui->panelPositionComboBox->currentIndex();
    settings->setPanelPosition(static_cast<PanelPosition>(panelPos));

    settings->setPanelPreviewsSize(ui->panelSizeSlider->value() * 10);

    settings->setJPEGSaveQuality(ui->JPEGQualitySlider->value());
    settings->setZoomStep(static_cast<qreal>(ui->zoomStepSlider->value() / 100.f));
    settings->setMouseScrollingSpeed(static_cast<qreal>(0.5f + (ui->mouseScrollingSpeedSlider->value() * 0.25f)));
    settings->setAutoResizeLimit(ui->autoResizeLimitSlider->value() * 5);
    settings->setExpandLimit(ui->expandLimitSlider->value());
    settings->setThumbnailerThreadCount(ui->thumbnailerThreadsSlider->value());
    settings->setMemoryAllocationLimit(ui->memoryLimitSpinBox->value());

    settings->setUseSystemColorScheme(ui->useSystemColorsCheckBox->isChecked());
    settings->setUiStyle(uiStyleComboBox->currentIndex() == 1 ? UI_STYLE_CLASSIC : UI_STYLE_QLEMENTINE);

    saveColorScheme();
    saveShortcuts();
    if(navBarPage)
        navBarPage->saveSettings();

    scriptManager->saveScripts();
    actionManager->saveShortcuts();
    emit settingsChanged();
}
//------------------------------------------------------------------------------
void SettingsDialog::saveSettingsAndClose() {
    saveSettings();
    this->close();
}
//------------------------------------------------------------------------------
void SettingsDialog::readColorScheme() {
    auto colors = settings->colorScheme();
    setColorScheme(colors);
}

void SettingsDialog::setColorScheme(ColorScheme colors) {
    if(UiStyle::useQlementine()) {
        // two entries: Dark, Light (see setupQlementineThemePage())
        if(settings->useSystemColorScheme()) {
            ui->themeSelectorComboBox->setCurrentIndex(-1); // follows the desktop
        } else {
            mQlementinePresetTid = colors.tid == COLORS_LIGHT ? COLORS_LIGHT : COLORS_DARK;
            ui->themeSelectorComboBox->setCurrentIndex(mQlementinePresetTid == COLORS_LIGHT ? 1 : 0);
        }
    } else {
        switch (colors.tid) {
            case COLORS_LIGHT: ui->themeSelectorComboBox->setCurrentIndex(3);   break;
            case COLORS_BLACK: ui->themeSelectorComboBox->setCurrentIndex(0);   break;
            case COLORS_DARK: ui->themeSelectorComboBox->setCurrentIndex(1);    break;
            case COLORS_DARKBLUE: ui->themeSelectorComboBox->setCurrentIndex(2);break;
            default: ui->themeSelectorComboBox->setCurrentIndex(-1);            break;
        }
    }
    ui->colorSelectorAccent->setColor(colors.accent);
    ui->colorSelectorBackground->setColor(colors.background);
    ui->colorSelectorFullscreen->setColor(colors.background_fullscreen);
    ui->colorSelectorFolderview->setColor(colors.folderview);
    ui->colorSelectorFolderviewPanel->setColor(colors.folderview_topbar);
    ui->colorSelectorText->setColor(colors.text);
    ui->colorSelectorIcons->setColor(colors.icons);
    ui->colorSelectorWidget->setColor(colors.widget);
    ui->colorSelectorWidgetBorder->setColor(colors.widget_border);
    ui->colorSelectorOverlay->setColor(colors.overlay);
    ui->colorSelectorOverlayText->setColor(colors.overlay_text);
    ui->colorSelectorScrollbar->setColor(colors.scrollbar);
}

//------------------------------------------------------------------------------
void SettingsDialog::saveColorScheme() {
    if(UiStyle::useQlementine()) {
        // Only the accent is editable in this mode; the rest of the pickers is
        // hidden and holds the colours the theme produced, so rebuilding the
        // scheme from them would just overwrite the theme with itself.
        ColorScheme colors = settings->colorScheme();
        colors.accent = ui->colorSelectorAccent->color();
        settings->setColorScheme(colors);
        return;
    }
    BaseColorScheme base;
    base.accent = ui->colorSelectorAccent->color();
    base.background = ui->colorSelectorBackground->color();
    base.background_fullscreen = ui->colorSelectorFullscreen->color();
    base.folderview = ui->colorSelectorFolderview->color();
    base.folderview_topbar = ui->colorSelectorFolderviewPanel->color();
    base.text = ui->colorSelectorText->color();
    base.icons = ui->colorSelectorIcons->color();
    base.widget = ui->colorSelectorWidget->color();
    base.widget_border = ui->colorSelectorWidgetBorder->color();
    base.overlay = ui->colorSelectorOverlay->color();
    base.overlay_text = ui->colorSelectorOverlayText->color();
    base.scrollbar = ui->colorSelectorScrollbar->color();
    base.tid = settings->colorScheme().tid;
    settings->setColorScheme(ColorScheme(base));
}
//------------------------------------------------------------------------------
void SettingsDialog::readShortcuts() {
    ui->shortcutsTableWidget->clearContents();
    ui->shortcutsTableWidget->setRowCount(0);
    const QMap<QString, QString> shortcuts = actionManager->allShortcuts();
    QMapIterator<QString, QString> i(shortcuts);
    while(i.hasNext()) {
        i.next();
        addShortcutToTable(i.value(), i.key());
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::readScripts() {
    ui->scriptsListWidget->clear();
    const QMap<QString, Script> scripts = scriptManager->allScripts();
    QMapIterator<QString, Script> i(scripts);
    while(i.hasNext()) {
        i.next();
        addScriptToList(i.key());
    }
}
//------------------------------------------------------------------------------
// does not check if the shortcut already there
void SettingsDialog::addScriptToList(const QString &name) {
    if(name.isEmpty())
        return;

    QListWidget *list = ui->scriptsListWidget;
    QListWidgetItem *nameItem = new QListWidgetItem(name);
    nameItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    list->insertItem(ui->scriptsListWidget->count(), nameItem);
    list->sortItems(Qt::AscendingOrder);
}
//------------------------------------------------------------------------------
void SettingsDialog::addScript() {
    ScriptEditorDialog w;
    if(w.exec()) {
        if(w.scriptName().isEmpty())
            return;
        scriptManager->addScript(w.scriptName(), w.script());
        readScripts();
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::editScript() {
    int row = ui->scriptsListWidget->currentRow();
    if(row >= 0) {
        QString name = ui->scriptsListWidget->currentItem()->text();
        editScript(name);
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::editScript(QListWidgetItem* item) {
    if(item) {
        editScript(item->text());
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::editScript(QString name) {
    ScriptEditorDialog w(name, scriptManager->getScript(name));
    if(w.exec()) {
        if(w.scriptName().isEmpty())
            return;
        scriptManager->addScript(w.scriptName(), w.script());
        readScripts();
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::removeScript() {
    int row = ui->scriptsListWidget->currentRow();
    if(row >= 0) {
        QString scriptName = ui->scriptsListWidget->currentItem()->text();
        delete ui->scriptsListWidget->takeItem(row);
        saveShortcuts();
        actionManager->removeAllShortcuts("s:"+scriptName);
        readShortcuts();
        scriptManager->removeScript(scriptName);
    }
}
//------------------------------------------------------------------------------
// does not check if the shortcut already there
void SettingsDialog::addShortcutToTable(const QString &action, const QString &shortcut) {
    if(action.isEmpty() || shortcut.isEmpty())
        return;

    ui->shortcutsTableWidget->setRowCount(ui->shortcutsTableWidget->rowCount() + 1);
    QTableWidgetItem *actionItem = new QTableWidgetItem(action);
    actionItem->setTextAlignment(Qt::AlignCenter);
    ui->shortcutsTableWidget->setItem(ui->shortcutsTableWidget->rowCount() - 1, 0, actionItem);
    QTableWidgetItem *shortcutItem = new QTableWidgetItem(shortcut);
    shortcutItem->setTextAlignment(Qt::AlignCenter);
    ui->shortcutsTableWidget->setItem(ui->shortcutsTableWidget->rowCount() - 1, 1, shortcutItem);
    // EFFICIENCY
    ui->shortcutsTableWidget->sortByColumn(0, Qt::AscendingOrder);
}
//------------------------------------------------------------------------------
void SettingsDialog::addShortcut() {
    ShortcutCreatorDialog w;
    if(!w.exec())
        return;
    for(int i = 0; i < ui->shortcutsTableWidget->rowCount(); i++) {
        if(ui->shortcutsTableWidget->item(i, 1)->text() == w.selectedShortcut())
            removeShortcutAt(i);
    }
    addShortcutToTable(w.selectedAction(), w.selectedShortcut());
    // select
    auto items = ui->shortcutsTableWidget->findItems(w.selectedShortcut(), Qt::MatchExactly);
    if(items.count()) {
        int newRow = ui->shortcutsTableWidget->row(items.at(0));
        ui->shortcutsTableWidget->selectRow(newRow);
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::removeShortcutAt(int row) {
    if(row > 0 && row >= ui->shortcutsTableWidget->rowCount())
        return;
    ui->shortcutsTableWidget->removeRow(row);
}
//------------------------------------------------------------------------------
void SettingsDialog::editShortcut(int row) {
    if(row >= 0) {
        ShortcutCreatorDialog w;
        w.setWindowTitle(tr("Edit shortcut"));
        w.setAction(ui->shortcutsTableWidget->item(row, 0)->text());
        w.setShortcut(ui->shortcutsTableWidget->item(row, 1)->text());
        if(!w.exec())
            return;
        // remove itself
        removeShortcutAt(row);
        // remove anything we are replacing
        for(int i = 0; i < ui->shortcutsTableWidget->rowCount(); i++) {
            if(ui->shortcutsTableWidget->item(i, 1)->text() == w.selectedShortcut())
                removeShortcutAt(i);
        }
        // re-add
        addShortcutToTable(w.selectedAction(), w.selectedShortcut());
        // re-select
        auto items = ui->shortcutsTableWidget->findItems(w.selectedShortcut(), Qt::MatchExactly);
        if(items.count()) {
            int newRow = ui->shortcutsTableWidget->row(items.at(0));
            ui->shortcutsTableWidget->selectRow(newRow);
        }
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::editShortcut() {
    editShortcut(ui->shortcutsTableWidget->currentRow());
}
//------------------------------------------------------------------------------
void SettingsDialog::removeShortcut() {
    removeShortcutAt(ui->shortcutsTableWidget->currentRow());
}
//------------------------------------------------------------------------------
void SettingsDialog::saveShortcuts() {
    actionManager->removeAllShortcuts();
    for(int i = 0; i < ui->shortcutsTableWidget->rowCount(); i++) {
        actionManager->addShortcut(ui->shortcutsTableWidget->item(i, 1)->text(),
                                   ui->shortcutsTableWidget->item(i, 0)->text());
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::resetShortcuts() {
    actionManager->resetDefaults();
    readShortcuts();
}
//------------------------------------------------------------------------------
void SettingsDialog::resetZoomLevels() {
    ui->zoomLevels->setText(settings->defaultZoomLevels());
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void SettingsDialog::onExpandLimitSliderChanged(int value) {
    if(value == 0)
        ui->expandLimitLabel->setText("-");
    else
        ui->expandLimitLabel->setText(QString::number(value) + "x");
}
//------------------------------------------------------------------------------
void SettingsDialog::onJPEGQualitySliderChanged(int value) {
    ui->JPEGQualityLabel->setText(QString::number(value) + "%");
}
//------------------------------------------------------------------------------
void SettingsDialog::onZoomStepSliderChanged(int value) {
    ui->zoomStepLabel->setText(QString::number(value / 100.f, 'f', 2) + "x");
}
//------------------------------------------------------------------------------
void SettingsDialog::onMouseScrollingSpeedSliderChanged(int value) {
    ui->mouseScrollingSpeedLabel->setText(QString::number(0.5f + (value*0.25f), 'f', 2) + "x");
}
//------------------------------------------------------------------------------
void SettingsDialog::onThumbnailerThreadsSliderChanged(int value) {
    ui->thumbnailerThreadsLabel->setText(QString::number(value));
}
//------------------------------------------------------------------------------
void SettingsDialog::onBgOpacitySliderChanged(int value) {
    ui->bgOpacityPercentLabel->setText(QString::number(value) + "%");
}
//------------------------------------------------------------------------------
void SettingsDialog::onAutoResizeLimitSliderChanged(int value) {
    ui->autoResizeLimit->setText(QString::number(value * 5.f, 'f', 0) + "%");
}
//------------------------------------------------------------------------------
int SettingsDialog::exec() {
    return QDialog::exec();
}

void SettingsDialog::switchToPage(int number) {
    ui->sideBar2->selectEntry(number);
}
