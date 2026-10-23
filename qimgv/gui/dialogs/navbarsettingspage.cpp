#include "navbarsettingspage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QScrollArea>
#include <QFileDialog>
#include <QApplication>
#include <QPalette>
#include <QPixmap>
#include <QIcon>

#include "gui/customwidgets/keysequenceedit.h"
#include "gui/customwidgets/floatingnavbar.h"
#include "utils/imagelib.h"
#include "settings.h"

namespace {

// Renders an icon preview. Built-in QimgV icons are plain masks, so they have
// to be tinted with the text colour to stay visible on the dialog background.
QIcon previewIcon(const QString &path, bool tint) {
    QPixmap pixmap(path);
    if(pixmap.isNull())
        return QIcon();
    if(tint)
        ImageLib::recolor(pixmap, QApplication::palette().color(QPalette::WindowText));
    return QIcon(pixmap);
}

}

NavBarSettingsPage::NavBarSettingsPage(QWidget *parent)
    : QWidget(parent), autoHideCheckBox(nullptr), countSpinBox(nullptr), rowsLayout(nullptr)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(8);

    autoHideCheckBox = new QCheckBox(tr("Auto-hide (expand when the mouse gets close)"), this);
    autoHideCheckBox->setToolTip(tr("Only a thin strip stays visible; the bar slides up when the mouse reaches the bottom edge."));
    mainLayout->addWidget(autoHideCheckBox);

    QHBoxLayout *countLayout = new QHBoxLayout();
    countLayout->addWidget(new QLabel(tr("Number of buttons:"), this));
    countSpinBox = new QSpinBox(this);
    countSpinBox->setRange(1, 16);
    countSpinBox->setToolTip(tr("How many buttons the navigation bar has."));
    countLayout->addWidget(countSpinBox);
    countLayout->addStretch();
    mainLayout->addLayout(countLayout);

    mainLayout->addWidget(new QLabel(tr("Click a field and press the shortcut you want the button to run. Click the icon on the left to choose a custom image."), this));

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setMinimumHeight(200);
    QWidget *rowsWidget = new QWidget(scrollArea);
    rowsLayout = new QVBoxLayout(rowsWidget);
    rowsLayout->setContentsMargins(0, 0, 0, 0);
    scrollArea->setWidget(rowsWidget);
    mainLayout->addWidget(scrollArea, 1);

    QPushButton *resetButton = new QPushButton(tr("Reset to defaults"), this);
    mainLayout->addWidget(resetButton);

    connect(resetButton, &QPushButton::clicked, this, &NavBarSettingsPage::resetToDefaults);
    connect(countSpinBox, qOverload<int>(&QSpinBox::valueChanged), this, &NavBarSettingsPage::rebuildRows);
}

void NavBarSettingsPage::readSettings() {
    mShortcuts = settings->navBarShortcuts();
    mIcons = settings->navBarIcons();
    while(mIcons.size() < mShortcuts.size())
        mIcons.append(QString());
    while(mIcons.size() > mShortcuts.size())
        mIcons.removeLast();

    autoHideCheckBox->setChecked(settings->navBarAutoHide());

    countSpinBox->blockSignals(true);
    countSpinBox->setValue(qMax(1, mShortcuts.size()));
    countSpinBox->blockSignals(false);

    fillRows();
}

void NavBarSettingsPage::saveSettings() {
    settings->setNavBarAutoHide(autoHideCheckBox->isChecked());
    settings->saveNavBar(shortcuts(), icons());
}

QStringList NavBarSettingsPage::shortcuts() const {
    QStringList result;
    for(int i = 0; i < sequenceEdits.size(); i++)
        result << sequenceEdits.at(i)->sequence();
    return result;
}

QStringList NavBarSettingsPage::icons() const {
    QStringList result;
    for(int i = 0; i < sequenceEdits.size(); i++)
        result << mIcons.value(i);
    return result;
}

void NavBarSettingsPage::rebuildRows() {
    // keep whatever was already typed before resizing the list
    for(int i = 0; i < sequenceEdits.size() && i < mShortcuts.size(); i++)
        mShortcuts[i] = sequenceEdits.at(i)->sequence();
    while(mIcons.size() < mShortcuts.size())
        mIcons.append(QString());

    const int count = countSpinBox->value();
    while(mShortcuts.size() < count)
        mShortcuts.append(QString());
    while(mShortcuts.size() > count)
        mShortcuts.removeLast();
    while(mIcons.size() < count)
        mIcons.append(QString());
    while(mIcons.size() > count)
        mIcons.removeLast();

    fillRows();
}

void NavBarSettingsPage::fillRows() {
    while(QLayoutItem *item = rowsLayout->takeAt(0)) {
        if(QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }
    sequenceEdits.clear();
    iconButtons.clear();
    clearButtons.clear();

    for(int i = 0; i < mShortcuts.size(); i++) {
        QWidget *row = new QWidget(this);
        QHBoxLayout *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);

        QPushButton *iconButton = new QPushButton(row);
        iconButton->setFixedSize(46, 32);
        iconButton->setIconSize(QSize(22, 22));
        iconButton->setToolTip(tr("Choose a custom icon for this button"));
        connect(iconButton, &QPushButton::clicked, this, [this, i]() { pickIcon(i); });
        rowLayout->addWidget(iconButton);
        iconButtons.append(iconButton);

        rowLayout->addWidget(new QLabel(tr("Button %1:").arg(i + 1), row));

        KeySequenceEdit *edit = new KeySequenceEdit(row);
        edit->setText(mShortcuts.at(i));
        rowLayout->addWidget(edit, 1);
        connect(edit, &KeySequenceEdit::edited, this, [this, i]() { updateIconButton(i); });
        sequenceEdits.append(edit);

        QPushButton *clearButton = new QPushButton(tr("x"), row);
        clearButton->setFixedWidth(26);
        clearButton->setToolTip(tr("Use the automatic icon again"));
        connect(clearButton, &QPushButton::clicked, this, [this, i]() { clearIcon(i); });
        rowLayout->addWidget(clearButton);
        clearButtons.append(clearButton);

        rowsLayout->addWidget(row);
        updateIconButton(i);
    }
    rowsLayout->addStretch();
}

void NavBarSettingsPage::updateIconButton(int index) {
    if(index < 0 || index >= iconButtons.size() || index >= sequenceEdits.size())
        return;
    QPushButton *button = qobject_cast<QPushButton*>(iconButtons.at(index));
    if(!button)
        return;

    const QString custom = mIcons.value(index);
    const QString shortcut = sequenceEdits.at(index)->sequence();

    if(!custom.isEmpty()) {
        button->setIcon(previewIcon(custom, false));
        button->setText(QString());
        button->setToolTip(tr("Custom icon: %1").arg(custom));
    } else {
        const QString automatic = FloatingNavBar::defaultIconPath(shortcut);
        if(!automatic.isEmpty()) {
            button->setIcon(previewIcon(automatic, true));
            button->setText(QString());
            button->setToolTip(tr("Automatic icon - click to choose your own"));
        } else {
            button->setIcon(QIcon());
            button->setText(tr("none"));
            button->setToolTip(tr("No automatic icon - click to choose one"));
        }
    }
}

void NavBarSettingsPage::pickIcon(int index) {
    const QString file = QFileDialog::getOpenFileName(
        this,
        tr("Choose icon for button %1").arg(index + 1),
        QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.ico *.svg);;All files (*)"));
    if(file.isEmpty())
        return;
    while(mIcons.size() < sequenceEdits.size())
        mIcons.append(QString());
    mIcons[index] = file;
    updateIconButton(index);
}

void NavBarSettingsPage::clearIcon(int index) {
    while(mIcons.size() < sequenceEdits.size())
        mIcons.append(QString());
    if(index < mIcons.size())
        mIcons[index] = QString();
    updateIconButton(index);
}

void NavBarSettingsPage::resetToDefaults() {
    mShortcuts = Settings::defaultNavBarShortcuts();
    mIcons.clear();
    for(int i = 0; i < mShortcuts.size(); i++)
        mIcons.append(QString());
    countSpinBox->blockSignals(true);
    countSpinBox->setValue(mShortcuts.size());
    countSpinBox->blockSignals(false);
    fillRows();
}
