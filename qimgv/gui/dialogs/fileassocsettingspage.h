#pragma once

#include <QWidget>

class QCheckBox;
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

// Page in Preferences that owns the Windows file associations: a tree of the
// formats the build can open, grouped by kind, with a per-user / all-users
// switch. Only the ProgId this app owns is written; the default handler for an
// extension is never touched (Windows keeps that in UserChoice and the user
// picks it in "Default apps").
class FileAssocSettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit FileAssocSettingsPage(QWidget *parent = nullptr);

    // load from / write to the registry (called by SettingsDialog)
    void readSettings();
    void saveSettings();

private slots:
    void onItemChanged(QTreeWidgetItem *item, int column);
    void onMachineWideToggled(bool on);
    void selectAll();
    void restoreDefaults();

private:
    void rebuildTree(const QStringList &checked, bool defaultWhenEmpty);
    QStringList checkedExtensions() const;
    void updateParent(QTreeWidgetItem *parent);
    bool targetHiveMachineWide() const;

    QTreeWidget *tree = nullptr;
    QCheckBox *machineWideBox = nullptr;
    QLabel *warning = nullptr;
    QLabel *status = nullptr;
};
