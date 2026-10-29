#include "navbarsettingspage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QScrollArea>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QApplication>
#include <QPalette>
#include <QPixmap>
#include <QIcon>
#include <QPainter>
#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QTimer>
#include <functional>

#include "gui/customwidgets/keysequenceedit.h"
#include "gui/customwidgets/floatingnavbar.h"
#include "utils/imagelib.h"
#include "settings.h"
#include "uistyle.h"

namespace {

const char *kRowMime = "application/x-qimgv-navrow";

inline QPoint eventPos(const QMouseEvent *event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position().toPoint();
#else
    return event->pos();
#endif
}

inline QPoint eventPos(const QDropEvent *event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position().toPoint();
#else
    return event->pos();
#endif
}

inline QPoint eventPos(const QDragMoveEvent *event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position().toPoint();
#else
    return event->pos();
#endif
}

// Vector icon from the set that ships with the app. These are masks, so they get
// tinted like the built-in png icons.
QIcon themedIcon(const QString &name) {
    if(name.isEmpty())
        return QIcon();
    return QIcon(QStringLiteral(":/qlementine/icons/16/%1.svg").arg(name));
}

// Preview for a png resource; built-in icons are masks and have to be tinted to
// stay visible on the dialog background.
QIcon previewIcon(const QString &path, bool tint) {
    QPixmap pixmap(path);
    if(pixmap.isNull())
        return QIcon();
    if(tint)
        ImageLib::recolor(pixmap, QApplication::palette().color(QPalette::WindowText));
    return QIcon(pixmap);
}

QIcon automaticIcon(const QString &shortcut) {
    if(UiStyle::useQlementine()) {
        const QString name = FloatingNavBar::defaultThemedIconName(shortcut);
        if(!name.isEmpty())
            return themedIcon(name);
    }
    const QString path = FloatingNavBar::defaultIconPath(shortcut);
    if(path.isEmpty())
        return QIcon();
    // the @2x variant is the crisp one at this size
    QString hiRes = path;
    hiRes.replace(QLatin1String(".png"), QLatin1String("@2x.png"));
    QIcon icon = previewIcon(hiRes, true);
    return icon.isNull() ? previewIcon(path, true) : icon;
}

} // namespace

// Container of the button rows: accepts rows dragged by their grip and reports
// where they were dropped. Not in the anonymous namespace because the page holds
// a pointer to it.
class RowsArea : public QWidget {
public:
    explicit RowsArea(QWidget *parent = nullptr) : QWidget(parent) {
        setAcceptDrops(true);
    }

    // row widgets, in display order (owned by the page)
    QList<QWidget*> *rows = nullptr;

    // the drop recorded by the last drag, taken by the handle once the drag ended
    bool takePendingMove(int &from, int &to) {
        if(mPendingFrom < 0)
            return false;
        from = mPendingFrom;
        to = mPendingTo;
        mPendingFrom = -1;
        return true;
    }

protected:
    void dragEnterEvent(QDragEnterEvent *event) override {
        if(event->mimeData()->hasFormat(kRowMime))
            event->acceptProposedAction();
    }
    void dragMoveEvent(QDragMoveEvent *event) override {
        if(!event->mimeData()->hasFormat(kRowMime))
            return;
        mDropRow = rowAt(eventPos(event).y());
        update();
        event->acceptProposedAction();
    }
    void dragLeaveEvent(QDragLeaveEvent *) override {
        mDropRow = -1;
        update();
    }
    void dropEvent(QDropEvent *event) override {
        if(!event->mimeData()->hasFormat(kRowMime) || !rows || rows->isEmpty())
            return;
        const int from = event->mimeData()->data(kRowMime).toInt();
        int to = rowAt(eventPos(event).y());
        mDropRow = -1;
        update();
        if(to > from)
            --to;
        to = qBound(0, to, rows->size() - 1);
        if(to != from) {
            mPendingFrom = from;
            mPendingTo = to;
        }
        event->acceptProposedAction();
    }
    void paintEvent(QPaintEvent *) override {
        if(mDropRow < 0 || !rows || rows->isEmpty())
            return;
        QPainter p(this);
        p.setPen(QPen(palette().color(QPalette::Highlight), 2));
        int y = rows->last()->geometry().bottom() + 1;
        if(mDropRow < rows->size())
            y = rows->at(mDropRow)->geometry().top() - 2;
        p.drawLine(0, y, width(), y);
    }

private:
    // insertion index for a y coordinate inside this widget
    int rowAt(int y) const {
        if(!rows)
            return -1;
        for(int i = 0; i < rows->size(); ++i)
            if(y < rows->at(i)->geometry().center().y())
                return i;
        return rows->size();
    }

    int mDropRow = -1;
    int mPendingFrom = -1;
    int mPendingTo = -1;
};

namespace {

// Grip at the left of a row. Dragging it moves the row; nothing else in the row
// starts a drag, so the shortcut field and the icon list keep working normally.
class DragHandle : public QWidget {
public:
    using MoveHandler = std::function<void(int, int)>;

    DragHandle(QWidget *parent, RowsArea *area, MoveHandler onMove)
        : QWidget(parent), mArea(area), mOnMove(std::move(onMove))
    {
        setFixedWidth(14);
        setCursor(Qt::OpenHandCursor);
        setToolTip(NavBarSettingsPage::tr("Drag to change the order"));
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        QColor color = palette().color(QPalette::WindowText);
        color.setAlpha(110);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        const int dot = 2;
        const int gap = 2;
        int y = (height() - (3 * dot + 2 * gap)) / 2;
        const int x = width() / 2 - dot - 1;
        for(int i = 0; i < 3; ++i) {
            painter.drawRect(x, y, dot, dot);
            painter.drawRect(x + dot + 2, y, dot, dot);
            y += dot + gap;
        }
    }

    void mousePressEvent(QMouseEvent *event) override {
        if(event->button() == Qt::LeftButton)
            mPressPos = eventPos(event);
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        if(!(event->buttons() & Qt::LeftButton) || !mArea || !mArea->rows)
            return;
        if((eventPos(event) - mPressPos).manhattanLength() < QApplication::startDragDistance())
            return;
        QWidget *row = parentWidget();
        const int index = row ? mArea->rows->indexOf(row) : -1;
        if(index < 0)
            return;

        auto *drag = new QDrag(this);
        auto *mime = new QMimeData;
        mime->setData(kRowMime, QByteArray::number(index));
        drag->setMimeData(mime);
        const QPixmap pixmap = row->grab();
        drag->setPixmap(pixmap.scaledToWidth(qMin(pixmap.width(), 360), Qt::SmoothTransformation));
        drag->setHotSpot(QPoint(0, 0));

        setCursor(Qt::ClosedHandCursor);
        drag->exec(Qt::MoveAction);
        setCursor(Qt::OpenHandCursor);

        // Only now, with the drag loop finished, is it safe to rebuild the rows -
        // and even then the widget that started the drag must not be deleted from
        // inside its own event handler, so the page defers the rebuild.
        int from = -1;
        int to = -1;
        if(mArea->takePendingMove(from, to) && mOnMove)
            mOnMove(from, to);
    }

private:
    RowsArea *mArea;
    MoveHandler mOnMove;
    QPoint mPressPos;
};

} // namespace

NavBarSettingsPage::NavBarSettingsPage(QWidget *parent)
    : QWidget(parent), modeComboBox(nullptr), countSpinBox(nullptr),
      rowsLayout(nullptr), rowsArea(nullptr)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(8);

    QHBoxLayout *modeLayout = new QHBoxLayout();
    modeLayout->addWidget(new QLabel(tr("Show the bar:"), this));
    modeComboBox = new QComboBox(this);
    modeComboBox->addItem(tr("Always"), NAVBAR_ALWAYS);
    modeComboBox->addItem(tr("Auto-hide"), NAVBAR_AUTO);
    modeComboBox->addItem(tr("Never"), NAVBAR_NEVER);
    modeComboBox->setToolTip(tr("Always keeps the bar docked at the bottom, auto-hide slides it in when the mouse reaches the bottom edge, never leaves it out."));
    modeLayout->addWidget(modeComboBox);
    modeLayout->addStretch();
    mainLayout->addLayout(modeLayout);

    QHBoxLayout *countLayout = new QHBoxLayout();
    countLayout->addWidget(new QLabel(tr("Number of buttons:"), this));
    countSpinBox = new QSpinBox(this);
    countSpinBox->setRange(1, 16);
    countSpinBox->setToolTip(tr("How many buttons the navigation bar has."));
    countLayout->addWidget(countSpinBox);
    countLayout->addStretch();
    mainLayout->addLayout(countLayout);

    mainLayout->addWidget(new QLabel(tr("Drag a row by its grip to change the order. Click a field and press the shortcut this button should run. Pick the icon of a button from the list on the left."), this));

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setMinimumHeight(200);
    rowsArea = new RowsArea(scrollArea);
    rowsArea->rows = &rows;
    rowsLayout = new QVBoxLayout(rowsArea);
    rowsLayout->setContentsMargins(0, 0, 0, 0);
    rowsLayout->setSpacing(6);
    scrollArea->setWidget(rowsArea);
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

    const int index = modeComboBox->findData(int(settings->navBarMode()));
    modeComboBox->setCurrentIndex(index >= 0 ? index : 1);

    countSpinBox->blockSignals(true);
    countSpinBox->setValue(qMax(1, mShortcuts.size()));
    countSpinBox->blockSignals(false);

    fillRows();
}

void NavBarSettingsPage::saveSettings() {
    settings->setNavBarMode(static_cast<NavBarMode>(modeComboBox->currentData().toInt()));
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
    for(int i = 0; i < iconCombos.size(); i++) {
        const QString value = iconCombos.at(i)->currentData().toString();
        // "choose a file" is an action, never a value
        result << (value == QLatin1String("__file__") ? QString() : value);
    }
    return result;
}

void NavBarSettingsPage::syncFromRows() {
    for(int i = 0; i < sequenceEdits.size() && i < mShortcuts.size(); i++)
        mShortcuts[i] = sequenceEdits.at(i)->sequence();
    for(int i = 0; i < iconCombos.size(); i++) {
        const QString value = iconCombos.at(i)->currentData().toString();
        if(i < mIcons.size() && value != QLatin1String("__file__"))
            mIcons[i] = value;
    }
}

void NavBarSettingsPage::rebuildRows() {
    // keep whatever was already typed and picked before resizing the list
    syncFromRows();
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

void NavBarSettingsPage::moveRow(int from, int to) {
    if(from == to || from < 0 || to < 0 || from >= rows.size() || to >= rows.size())
        return;
    QTimer::singleShot(0, this, [this, from, to]() {
        syncFromRows();
        mShortcuts.move(from, to);
        while(mIcons.size() < mShortcuts.size())
            mIcons.append(QString());
        mIcons.move(from, to);
        fillRows();
    });
}

void NavBarSettingsPage::fillRows() {
    sequenceEdits.clear();
    iconCombos.clear();
    rows.clear();

    while(QLayoutItem *item = rowsLayout->takeAt(0)) {
        if(QWidget *widget = item->widget())
            widget->deleteLater();
        delete item;
    }

    const QStringList iconNames = FloatingNavBar::themedIconNames();

    for(int i = 0; i < mShortcuts.size(); i++) {
        QWidget *row = new QWidget(rowsArea);
        QHBoxLayout *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(6);

        rowLayout->addWidget(new DragHandle(row, rowsArea,
            [this](int from, int to) { moveRow(from, to); }));

        QComboBox *iconCombo = new QComboBox(row);
        iconCombo->setMinimumWidth(190);
        iconCombo->setIconSize(QSize(18, 18));
        iconCombo->setToolTip(tr("Icon of this button"));
        iconCombo->addItem(QIcon(), tr("Automatic"), QString());
        for(const QString &name : iconNames)
            iconCombo->addItem(themedIcon(name), name, QStringLiteral("theme:%1").arg(name));
        iconCombo->insertSeparator(iconCombo->count());
        iconCombo->addItem(tr("Choose a file..."), QStringLiteral("__file__"));
        rowLayout->addWidget(iconCombo);
        iconCombos.append(iconCombo);

        QLabel *label = new QLabel(tr("Button %1:").arg(i + 1), row);
        label->setMinimumWidth(64);
        rowLayout->addWidget(label);

        KeySequenceEdit *edit = new KeySequenceEdit(row);
        edit->setText(mShortcuts.at(i));
        rowLayout->addWidget(edit, 1);
        sequenceEdits.append(edit);

        rows.append(row);
        rowsLayout->addWidget(row);

        // the index is looked up from the row itself, so a row can be moved later
        connect(edit, &KeySequenceEdit::edited, this, [this, row]() {
            updateAutoPreview(rows.indexOf(row));
        });
        connect(iconCombo, qOverload<int>(&QComboBox::activated), this, [this, row](int) {
            const int index = rows.indexOf(row);
            if(index < 0)
                return;
            QComboBox *combo = iconCombos.at(index);
            if(combo->currentData().toString() == QLatin1String("__file__")) {
                chooseIconFile(index);
                return;
            }
            if(index < mIcons.size())
                mIcons[index] = combo->currentData().toString();
        });

        selectIcon(i, mIcons.value(i));
        updateAutoPreview(i);
    }
    rowsLayout->addStretch();
}

// Selects the entry for a stored icon value, adding one for a file that is not in
// the list yet.
void NavBarSettingsPage::selectIcon(int index, const QString &value) {
    if(index < 0 || index >= iconCombos.size())
        return;
    QComboBox *combo = iconCombos.at(index);

    if(value.isEmpty()) {
        combo->setCurrentIndex(0);
        return;
    }
    for(int i = 0; i < combo->count(); ++i) {
        if(combo->itemData(i).toString() == value) {
            combo->setCurrentIndex(i);
            return;
        }
    }
    // a file the user picked earlier: shown as an entry of its own
    const int at = combo->count() - 1;
    combo->insertItem(at, previewIcon(value, false), QFileInfo(value).fileName(), value);
    combo->setCurrentIndex(at);
}

void NavBarSettingsPage::updateAutoPreview(int index) {
    if(index < 0 || index >= iconCombos.size() || index >= sequenceEdits.size())
        return;
    iconCombos.at(index)->setItemIcon(0, automaticIcon(sequenceEdits.at(index)->sequence()));
}

void NavBarSettingsPage::chooseIconFile(int index) {
    if(index < 0 || index >= iconCombos.size())
        return;
    const QString file = QFileDialog::getOpenFileName(
        this,
        tr("Choose icon for button %1").arg(index + 1),
        QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.ico *.svg);;All files (*)"));
    if(file.isEmpty()) {
        // cancelled: put the selection back where it was
        selectIcon(index, mIcons.value(index));
        return;
    }
    if(index < mIcons.size())
        mIcons[index] = file;
    selectIcon(index, file);
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
