#include "floatingnavbar.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QPainter>
#include <QPen>
#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QMap>
#include <QTransform>
#include <QPropertyAnimation>
#include <QTimer>
#include <QEnterEvent>
#include <QApplication>
#include <QCursor>
#include <QEvent>

#include "components/actionmanager/actionmanager.h"
#include "utils/imagelib.h"
#include "settings.h"

namespace {

// resolves the action bound to a shortcut, tolerating a not-yet-created manager
QString actionForShortcut(const QString &shortcut) {
    if(shortcut.isEmpty() || !actionManager)
        return QString();
    return actionManager->actionForShortcut(shortcut);
}

// action name -> icon resource. Anything not listed falls back to a text button
// showing the shortcut itself.
const QMap<QString, QString> &actionIcons() {
    static const QMap<QString, QString> icons = {
        {"prevImage",         ":/res/icons/common/menuitem/back16.png"},
        // "next" is the SAME left arrow, turned 180 degrees in rebuildButtons()
        {"nextImage",         ":/res/icons/common/menuitem/back16.png"},
        {"prevDirectory",     ":/res/icons/common/menuitem/back16.png"},
        {"nextDirectory",     ":/res/icons/common/menuitem/back16.png"},
        {"zoomIn",            ":/res/icons/common/buttons/contextmenu/zoom-in18.png"},
        {"zoomOut",           ":/res/icons/common/buttons/contextmenu/zoom-out18.png"},
        {"zoomInCursor",      ":/res/icons/common/buttons/contextmenu/zoom-in18.png"},
        {"zoomOutCursor",     ":/res/icons/common/buttons/contextmenu/zoom-out18.png"},
        {"fitNormal",         ":/res/icons/common/buttons/contextmenu/zoom-original18.png"},
        {"fitWindow",         ":/res/icons/common/buttons/contextmenu/fit-window18.png"},
        {"fitWidth",          ":/res/icons/common/buttons/contextmenu/fit-width18.png"},
        {"fitWindowStretch",  ":/res/icons/common/buttons/contextmenu/fit-height-stretch18.png"},
        {"rotateLeft",        ":/res/icons/common/menuitem/rotate-left16.png"},
        {"rotateRight",       ":/res/icons/common/menuitem/rotate-right16.png"},
        {"flipH",             ":/res/icons/common/menuitem/flip-h16.png"},
        {"flipV",             ":/res/icons/common/menuitem/flip-v16.png"},
        {"moveToTrash",       ":/res/icons/common/menuitem/trash16.png"},
        {"removeFile",        ":/res/icons/common/menuitem/trash16.png"},
        {"print",             ":/res/icons/common/menuitem/print16.png"},
        {"toggleImageInfo",   ":/res/icons/common/overlay/info16.png"},
        {"folderView",        ":/res/icons/common/menuitem/folderview16.png"},
        {"documentView",      ":/res/icons/common/menuitem/document-view16.png"},
        {"toggleFolderView",  ":/res/icons/common/menuitem/folderview16.png"},
        {"openSettings",      ":/res/icons/common/menuitem/settings16.png"},
        {"copyFileClipboard", ":/res/icons/common/menuitem/copy16.png"},
        {"copyPathClipboard", ":/res/icons/common/menuitem/copy16.png"},
        {"open",              ":/res/icons/common/menuitem/open16.png"},
        {"crop",              ":/res/icons/common/menuitem/image-crop16.png"},
        {"resize",            ":/res/icons/common/menuitem/resize16.png"},
        {"showInDirectory",   ":/res/icons/common/menuitem/folder16.png"},
        {"goUp",              ":/res/icons/common/buttons/panel/up16.png"},
    };
    return icons;
}

QPoint eventGlobalPos(QMouseEvent *event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->globalPosition().toPoint();
#else
    return event->globalPos();
#endif
}

}

FloatingNavBar::FloatingNavBar(QWidget *parent)
    : QWidget(parent), layout(nullptr), dragging(false), docked(true), barHeight(kBarHeight),
      mAutoHide(false), peekHeight(12), hoverMargin(40), posAnim(nullptr), collapseTimer(nullptr)
{
    setObjectName("FloatingNavBar");
    // never take keyboard focus, otherwise the shortcuts handled by the main
    // window would stop working while the bar is visible
    setFocusPolicy(Qt::NoFocus);
    setAccessibleName("FloatingNavBar");
    setMouseTracking(true);
    setCursor(Qt::ArrowCursor);

    layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 6, 12, 6);
    layout->setSpacing(8);
    layout->addStretch();
    layout->addStretch();

    posAnim = new QPropertyAnimation(this, "pos", this);
    posAnim->setDuration(170);
    posAnim->setEasingCurve(QEasingCurve::OutCubic);

    collapseTimer = new QTimer(this);
    collapseTimer->setSingleShot(true);
    collapseTimer->setInterval(450);
    connect(collapseTimer, &QTimer::timeout, this, &FloatingNavBar::collapseBar);

    // The image widget below consumes its own mouse events, so the hover zone
    // is detected from the global cursor position instead of from widget
    // geometry. This keeps the zone large without ever blocking clicks.
    qApp->installEventFilter(this);

    applyTheme();
    connect(settings, &Settings::settingsChanged, this, &FloatingNavBar::refreshTheme);

    hide();
}

void FloatingNavBar::setAutoHide(bool mode) {
    if(mAutoHide == mode)
        return;
    mAutoHide = mode;
    if(docked) {
        dockToBottom();
        if(mAutoHide && withinHoverZone())
            expandBar();
    }
}

bool FloatingNavBar::autoHide() const {
    return mAutoHide;
}

bool FloatingNavBar::withinHoverZone() const {
    QWidget *container = parentWidget();
    if(!container)
        return false;
    const QPoint p = container->mapFromGlobal(QCursor::pos());
    if(!container->rect().contains(p))
        return false;
    return p.y() >= container->height() - (barHeight + hoverMargin);
}

void FloatingNavBar::expandBar() {
    if(!mAutoHide)
        return;
    collapseTimer->stop();
    QWidget *container = parentWidget();
    if(!container)
        return;
    animateTo(qMax(0, container->height() - barHeight));
}

void FloatingNavBar::collapseBar() {
    if(!mAutoHide || !docked)
        return;
    if(withinHoverZone())
        return;
    QWidget *container = parentWidget();
    if(!container)
        return;
    animateTo(qMax(0, container->height() - peekHeight));
}

void FloatingNavBar::animateTo(int y) {
    if(!posAnim)
        return;
    posAnim->stop();
    if(pos().y() == y)
        return;
    posAnim->setStartValue(pos());
    posAnim->setEndValue(QPoint(0, y));
    posAnim->start();
}

// Watches app-wide mouse moves so the hover zone also works while the image
// widget below is handling the mouse. Nothing is intercepted: the event is
// always passed through.
bool FloatingNavBar::eventFilter(QObject *watched, QEvent *event) {
    Q_UNUSED(watched)
    // Cheap guards first: this filter sees every event in the application, so
    // avoid the coordinate math unless the app is actually interacting.
    if(event->type() != QEvent::MouseMove)
        return QWidget::eventFilter(watched, event);
    if(!mAutoHide || !docked || !isVisible() || !window()->isActiveWindow())
        return QWidget::eventFilter(watched, event);

    if(withinHoverZone()) {
        expandBar();
    } else if(!collapseTimer->isActive()) {
        collapseTimer->start();
    }
    return QWidget::eventFilter(watched, event);
}

void FloatingNavBar::setButtons(const QStringList &shortcuts, const QStringList &icons) {
    mShortcuts = shortcuts;
    mIcons = icons;
    while(mIcons.size() < mShortcuts.size())
        mIcons.append(QString());
    while(mIcons.size() > mShortcuts.size())
        mIcons.removeLast();
    updateFromShortcuts();
}

QStringList FloatingNavBar::shortcuts() const {
    return mShortcuts;
}

QStringList FloatingNavBar::icons() const {
    return mIcons;
}

void FloatingNavBar::refreshTheme() {
    applyTheme();
    rebuildButtons();
    update();
}

void FloatingNavBar::applyTheme() {
    const ColorScheme &colors = settings->colorScheme();
    QColor hover = colors.accent;
    hover.setAlpha(70);
    QColor pressed = colors.accent;
    pressed.setAlpha(130);

    setStyleSheet(QString(
        "#FloatingNavBar QPushButton {"
        "  background: transparent;"
        "  border: none;"
        "  border-radius: 7px;"
        "  padding: 6px 13px;"
        "  color: %1;"
        "}"
        "#FloatingNavBar QPushButton:hover   { background: rgba(%2,%3,%4,%5); }"
        "#FloatingNavBar QPushButton:pressed { background: rgba(%2,%3,%4,%6); }"
    ).arg(colors.icons.name())
     .arg(colors.accent.red())
     .arg(colors.accent.green())
     .arg(colors.accent.blue())
     .arg(hover.alpha())
     .arg(pressed.alpha()));
}

void FloatingNavBar::updateFromShortcuts() {
    rebuildButtons();
    if(docked)
        dockToBottom();
    else
        clampToParent();
    // always visible: with no buttons at all a "+" placeholder is shown
    show();
    raise();
}

void FloatingNavBar::insertButton(int at) {
    at = qBound(0, at, mShortcuts.size());
    while(mIcons.size() < mShortcuts.size())
        mIcons.append(QString());
    mShortcuts.insert(at, QString());
    mIcons.insert(at, QString());
    updateFromShortcuts();
    emit shortcutsChanged();
}

QString FloatingNavBar::defaultIconPath(const QString &shortcut) {
    const QString action = actionForShortcut(shortcut);
    if(action.isEmpty())
        return QString();
    return actionIcons().value(action);
}

void FloatingNavBar::rebuildButtons() {
    while(!buttons.isEmpty())
        delete buttons.takeFirst();

    const QColor iconColor = settings->colorScheme().icons;

    // Nothing configured: show a placeholder so the bar can be brought back.
    if(mShortcuts.isEmpty()) {
        QPushButton *addButton = new QPushButton(this);
        addButton->setFocusPolicy(Qt::NoFocus);
        addButton->setCursor(Qt::PointingHandCursor);
        addButton->setText(tr("+ Add button"));
        addButton->setToolTip(tr("Add a button to the navigation bar"));
        connect(addButton, &QPushButton::clicked, this, [this]() { insertButton(0); });
        layout->insertWidget(layout->count() - 1, addButton);
        buttons.append(addButton);
        return;
    }

    for(int i = 0; i < mShortcuts.size(); i++) {
        const QString shortcut = mShortcuts.at(i);
        const QString action = actionForShortcut(shortcut);

        QPushButton *button = new QPushButton(this);
        button->setFocusPolicy(Qt::NoFocus);
        button->setCursor(Qt::PointingHandCursor);
        button->setFlat(true);
        button->setAutoDefault(false);

        const int iconPx = 28;

        bool custom = !mIcons.value(i).isEmpty();
        QString iconPath = custom ? mIcons.at(i) : defaultIconPath(shortcut);
        QPixmap pixmap;

        if(!iconPath.isEmpty()) {
            // built-in icons ship an @2x variant which stays crisp at this size
            if(!custom) {
                QString hiRes = iconPath;
                hiRes.replace(QLatin1String(".png"), QLatin1String("@2x.png"));
                pixmap = QPixmap(hiRes);
            }
            if(pixmap.isNull())
                pixmap = QPixmap(iconPath);
        }

        // a custom icon that no longer exists falls back to the automatic one
        if(pixmap.isNull() && custom) {
            custom = false;
            iconPath = defaultIconPath(shortcut);
            QString hiRes = iconPath;
            hiRes.replace(QLatin1String(".png"), QLatin1String("@2x.png"));
            pixmap = QPixmap(hiRes);
            if(pixmap.isNull())
                pixmap = QPixmap(iconPath);
        }

        if(!iconPath.isEmpty() && !pixmap.isNull()) {
            if(!custom) {
                // built-in icon: "next" reuses the "previous" arrow turned
                // 180 degrees, and everything is tinted with the theme
                if(action == QLatin1String("nextImage") || action == QLatin1String("nextDirectory"))
                    pixmap = pixmap.transformed(QTransform().rotate(180));
                ImageLib::recolor(pixmap, iconColor);
            }
            // custom icons keep the colours the user picked
            pixmap = pixmap.scaled(iconPx, iconPx, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            button->setIcon(QIcon(pixmap));
            button->setIconSize(pixmap.size());
        } else {
            button->setText(shortcut.isEmpty() ? tr("(empty)") : shortcut);
        }

        if(shortcut.isEmpty())
            button->setToolTip(tr("Not assigned - right-click to set a shortcut"));
        else if(action.isEmpty())
            button->setToolTip(tr("%1 - no action is bound to this shortcut").arg(shortcut));
        else
            button->setToolTip(QString("%1\n%2").arg(action, shortcut));

        // left click behaves exactly like pressing the assigned shortcut
        connect(button, &QPushButton::clicked, this, [shortcut]() {
            const QString action = actionForShortcut(shortcut);
            if(!action.isEmpty())
                actionManager->invokeAction(action);
        });

        layout->insertWidget(layout->count() - 1, button);
        buttons.append(button);
    }
}

void FloatingNavBar::dockToBottom() {
    QWidget *container = parentWidget();
    if(!container)
        return;
    docked = true;
    if(posAnim)
        posAnim->stop();
    // auto-hide: rest with only a thin strip visible, unless the mouse is in
    // the hover zone right now
    int y = container->height() - barHeight;
    if(mAutoHide && !withinHoverZone())
        y = container->height() - peekHeight;
    setGeometry(0, qMax(0, y), container->width(), barHeight);
}

void FloatingNavBar::clampToParent() {
    QWidget *container = parentWidget();
    if(!container)
        return;
    const int y = qBound(0, pos().y(), qMax(0, container->height() - barHeight));
    setGeometry(0, y, container->width(), barHeight);
}

void FloatingNavBar::onContainerResized(QSize size) {
    Q_UNUSED(size)
    if(docked)
        dockToBottom();
    else
        clampToParent();
}

void FloatingNavBar::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    const ColorScheme &colors = settings->colorScheme();
    QPainter painter(this);
    // only the lower part of the widget is the visible bar; everything above it
    // is a transparent hover/trigger area
    const QRect barRect(0, height() - barHeight, width(), barHeight);
    painter.fillRect(barRect, colors.widget);
    painter.setPen(QPen(colors.widget_border, 1));
    painter.drawLine(barRect.topLeft(), barRect.topRight());
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void FloatingNavBar::enterEvent(QEnterEvent *event) {
#else
void FloatingNavBar::enterEvent(QEvent *event) {
#endif
    if(mAutoHide)
        expandBar();
    QWidget::enterEvent(event);
}

void FloatingNavBar::leaveEvent(QEvent *event) {
    if(mAutoHide && collapseTimer)
        collapseTimer->start();
    QWidget::leaveEvent(event);
}

void FloatingNavBar::mousePressEvent(QMouseEvent *event) {
    // in auto-hide mode the bar stays docked and is not draggable
    if(mAutoHide) {
        QWidget::mousePressEvent(event);
        return;
    }
    if(event->button() == Qt::LeftButton) {
        docked = false;
        dragging = true;
        dragOffset = eventGlobalPos(event) - pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void FloatingNavBar::mouseMoveEvent(QMouseEvent *event) {
    if(dragging && (event->buttons() & Qt::LeftButton)) {
        QWidget *container = parentWidget();
        int y = eventGlobalPos(event).y() - dragOffset.y();
        if(container)
            y = qBound(0, y, qMax(0, container->height() - barHeight));
        move(0, y);
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void FloatingNavBar::mouseReleaseEvent(QMouseEvent *event) {
    if(dragging) {
        dragging = false;
        setCursor(Qt::ArrowCursor);
        QWidget *container = parentWidget();
        if(container && (container->height() - (pos().y() + barHeight)) < 24)
            dockToBottom();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void FloatingNavBar::mouseDoubleClickEvent(QMouseEvent *event) {
    if(event->button() == Qt::LeftButton) {
        dockToBottom();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

// The bar is configured from Preferences now, so right-clicking it does
// nothing - it only swallows the event so the image context menu does not pop
// up on top of the bar.
void FloatingNavBar::contextMenuEvent(QContextMenuEvent *event) {
    event->accept();
}
