#pragma once

#include "gui/customwidgets/materialdialog.h"
#include <QFileDialog>
#include <QColorDialog>
#include <QThreadPool>
#include <QTableWidget>
#include <QTextBrowser>
#include <QListWidget>
#include <QStackedWidget>
#include <QButtonGroup>
#include <QApplication>
#include <QDebug>
#include <QMenu>
#include "gui/customwidgets/colorselectorbutton.h"
#include "gui/dialogs/shortcutcreatordialog.h"
#include "gui/dialogs/scripteditordialog.h"
#include "settings.h"
#include "components/actionmanager/actionmanager.h"

namespace Ui {
class SettingsDialog;
}

class NavBarSettingsPage;
class FileAssocSettingsPage;
class QCheckBox;
class QComboBox;
class QLabel;

class SettingsDialog : public MaterialDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();
    void switchToPage(int number);

public slots:
    int exec();

private:
    void readColorScheme();
    void setColorScheme(ColorScheme colors);
    void saveColorScheme();
    void readSettings();
    void readShortcuts();
    void readScripts();
    Ui::SettingsDialog *ui;

    // page created at runtime (not part of settingsdialog.ui)
    NavBarSettingsPage *navBarPage = nullptr;
    FileAssocSettingsPage *fileAssocPage = nullptr;
    // "Viewer" page controls, also built at runtime
    QCheckBox *edgeClickCheckBox = nullptr;
    QCheckBox *edgeVisibleCheckBox = nullptr;
    // "Advanced" page: large image decoding mode
    QCheckBox *twoPassCheckBox = nullptr;
    QLabel *twoPassNote = nullptr;
    // "Appearance" page, Qlementine mode: the preset list becomes Dark/Light and
    // only the accent colour stays editable
    int mQlementinePresetTid = -1;
    // "Appearance" page: Qlementine / Classic, applied on the next start
    QComboBox *uiStyleComboBox = nullptr;

    void saveShortcuts();
    void addShortcutToTable(const QString &action, const QString &shortcut);
    void addScriptToList(const QString &name);

    void setupSidebar();
    void setupUiStyleRow();
    void setupQlementineThemePage();
    // The Material themes carry their own accent, so the picker is hidden there
    void updateAccentRow(int tid);
    void removeShortcutAt(int row);
    void adjustSizeToContents();
    QMap<QString, QString> langs; // <"en_US", "English">
    QButtonGroup fitModeGrp, folderEndGrp, zoomIndGrp;

private slots:
    void saveSettings();
    void saveSettingsAndClose();

    void addScript();
    void editScript();
    void editScript(QListWidgetItem *item);
    void editScript(QString name);
    void removeScript();

    void addShortcut();
    void editShortcut();
    void editShortcut(int row);
    void removeShortcut();
    void resetShortcuts();
    void onBgOpacitySliderChanged(int value);
    void onThumbnailerThreadsSliderChanged(int value);
    void onExpandLimitSliderChanged(int value);
    void onZoomStepSliderChanged(int value);
    void onJPEGQualitySliderChanged(int value);
    void resetToDesktopTheme();    
    void onAutoResizeLimitSliderChanged(int value);
    void onMouseScrollingSpeedSliderChanged(int value);

    void resetZoomLevels();
signals:
    void settingsChanged();
};
