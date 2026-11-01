#pragma once

#include <QWidget>
#include <QStringList>
#include <QList>

class QCheckBox;
class QSpinBox;
class QVBoxLayout;
class QComboBox;
class KeySequenceEdit;
class RowsArea;

// Page shown in Preferences that owns all navigation bar options: auto-hide
// behaviour, the buttons in the order they appear on the bar, the shortcut of
// every button and the icon it shows. Rows are reordered by dragging their grip.
class NavBarSettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit NavBarSettingsPage(QWidget *parent = nullptr);

    // load from / write to Settings (called by SettingsDialog)
    void readSettings();
    void saveSettings();

    QStringList shortcuts() const;
    // icon of every button in display order: empty = automatic, "theme:<name>" =
    // vector icon from the set, anything else = path to an image file
    QStringList icons() const;

private slots:
    void rebuildRows();
    void resetToDefaults();
    // appends a group separator row (a divider on the bar)
    void addSeparator();
    // appends the button that cycles the image fit mode
    void addFitCycle();

private:
    void fillRows();
    void syncFromRows();
    void moveRow(int from, int to);
    void removeRow(int index);
    void selectIcon(int index, const QString &value);
    void updateAutoPreview(int index);
    void chooseIconFile(int index);
    // per-mode icons of the fit-mode button, in FloatingNavBar::fitCycleActions()
    // order; empty when the bar has no such button
    QStringList fitIcons() const;

    QComboBox *modeComboBox;
    QSpinBox *countSpinBox;
    QVBoxLayout *rowsLayout;
    RowsArea *rowsArea;
    QList<QWidget*> rows;
    // parallel to rows; a separator row has no key field and no icon combo
    QList<KeySequenceEdit*> sequenceEdits;
    QList<QComboBox*> iconCombos;
    // parallel to rows; one icon picker per fit mode for the cycle button row,
    // empty for every other row
    QList<QList<QComboBox*>> fitIconCombos;
    QStringList mShortcuts;
    QStringList mIcons;
    QStringList mFitIcons;
};
