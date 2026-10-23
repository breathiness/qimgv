#pragma once

#include <QWidget>
#include <QStringList>
#include <QList>

class QCheckBox;
class QSpinBox;
class QVBoxLayout;
class KeySequenceEdit;

// Page shown in Preferences that owns all navigation bar options:
// auto-hide behaviour, number of buttons, the shortcut of every button and an
// optional custom icon per button.
class NavBarSettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit NavBarSettingsPage(QWidget *parent = nullptr);

    // load from / write to Settings (called by SettingsDialog)
    void readSettings();
    void saveSettings();

    QStringList shortcuts() const;
    // per-button custom icon paths, empty = automatic icon
    QStringList icons() const;

private slots:
    void rebuildRows();
    void resetToDefaults();

private:
    void fillRows();
    void pickIcon(int index);
    void clearIcon(int index);
    void updateIconButton(int index);

    QCheckBox *autoHideCheckBox;
    QSpinBox *countSpinBox;
    QVBoxLayout *rowsLayout;
    QList<KeySequenceEdit*> sequenceEdits;
    QList<QWidget*> iconButtons;
    QList<QWidget*> clearButtons;
    QStringList mShortcuts;
    QStringList mIcons;
};
