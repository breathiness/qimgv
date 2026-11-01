#include "floatingnavbar.h"

#include "uistyle.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QPainter>
#include <QPen>
#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QMap>
#include <QEnterEvent>
#include <QApplication>
#include <QCursor>
#include <QEvent>

#include "components/actionmanager/actionmanager.h"
#include "utils/imagelib.h"
#include "settings.h"

namespace {

// Thin vertical line that groups the buttons of the bar. It is not a button: it
// never reacts to the mouse, so a click there behaves like a click on the bar
// itself.
class NavBarSeparator : public QWidget {
public:
    explicit NavBarSeparator(QWidget *parent) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setFocusPolicy(Qt::NoFocus);
        setFixedWidth(11);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        Q_UNUSED(event)
        QPainter painter(this);
        // a quiet version of the icon colour: the bar's own border colour is too
        // close to its surface to read as a group divider
        QColor line = settings->colorScheme().icons;
        line.setAlpha(70);
        const int x = width() / 2;
        painter.setPen(QPen(line, 1));
        painter.drawLine(x, 9, x, qMax(9, height() - 10));
    }
};

// resolves the action bound to a shortcut, tolerating a not-yet-created manager
QString actionForShortcut(const QString &shortcut) {
    if(shortcut.isEmpty() || !actionManager)
        return QString();
    return actionManager->actionForShortcut(shortcut);
}

// action name -> icon in the themed set (qlementine-icons), used when the
// Qlementine style is active. Missing entries fall back to the PNG resources.
const QMap<QString, QString> &actionThemeIcons() {
    static const QMap<QString, QString> icons = {
        {"prevImage",         "navigation/arrow-left"},
        {"nextImage",         "navigation/arrow-right"},
        {"prevDirectory",     "navigation/arrow-left"},
        {"nextDirectory",     "navigation/arrow-right"},
        {"zoomIn",            "action/zoom-in"},
        {"zoomOut",           "action/zoom-out"},
        {"zoomInCursor",      "action/zoom-in"},
        {"zoomOutCursor",     "action/zoom-out"},
        {"fitNormal",         "action/zoom-original"},
        {"fitWindow",         "action/zoom-fit"},
        {"fitWidth",          "page/zoom-fit-page"},
        {"fitWindowStretch",  "page/zoom-fit-screen"},
        {"rotateLeft",        "action/rotate-anticlockwise"},
        {"rotateRight",       "action/rotate-clockwise"},
        {"flipH",             "action/flip-horizontal"},
        {"flipV",             "action/flip-vertical"},
        {"moveToTrash",       "action/trash"},
        {"removeFile",        "action/trash"},
        {"print",             "action/print"},
        {"toggleImageInfo",   "misc/info"},
        {"folderView",        "file/folder"},
        {"documentView",      "document/open"},
        {"toggleFolderView",  "file/folder"},
        {"openSettings",      "navigation/settings"},
        {"copyFileClipboard", "action/copy"},
        {"copyPathClipboard", "action/copy"},
        {"open",              "document/open"},
        {"crop",              "action/crop"},
        {"resize",            "action/resize-bigger"},
        {"showInDirectory",   "file/folder-open"},
        {"goUp",              "navigation/arrow-up"},
    };
    return icons;
}

// action name -> icon resource. Anything not listed falls back to a text button
// showing the shortcut itself.
const QMap<QString, QString> &actionIcons() {
    static const QMap<QString, QString> icons = {
        {"prevImage",         ":/res/icons/common/menuitem/back16.png"},
        {"nextImage",         ":/res/icons/common/menuitem/next16.png"},
        {"prevDirectory",     ":/res/icons/common/menuitem/back16.png"},
        {"nextDirectory",     ":/res/icons/common/menuitem/next16.png"},
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
      mMode(NAVBAR_AUTO), mViewActive(false), fitButton(nullptr), mMenuOpen(false)
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

    // The image widget below consumes its own mouse events, so the trigger strip
    // is detected from the global cursor position instead of from widget
    // geometry. This keeps the strip large without ever blocking clicks.
    qApp->installEventFilter(this);

    applyTheme();
    connect(settings, &Settings::settingsChanged, this, &FloatingNavBar::refreshTheme);

    hide();
}

void FloatingNavBar::setMode(NavBarMode mode) {
    if(mMode == mode)
        return;
    mMode = mode;
    // auto mode knows only the bottom strip, so a bar that was dragged away is
    // put back before the strip logic runs
    if(mode == NAVBAR_AUTO)
        docked = true;
    applyState();
    emit layoutStateChanged();
}

NavBarMode FloatingNavBar::mode() const {
    return mMode;
}

bool FloatingNavBar::isDocked() const {
    return docked;
}

void FloatingNavBar::setViewActive(bool active) {
    if(mViewActive == active)
        return;
    mViewActive = active;
    applyState();
}

// Decides whether the bar is on screen at all and where it sits. Called when the
// mode, the active page or the parent size changes.
void FloatingNavBar::applyState() {
    // the folder view has no navigation bar: take it off instead of leaving a
    // strip or letting it appear when the cursor reaches the bottom edge
    if(!mViewActive || mMode == NAVBAR_NEVER) {
        hide();
        return;
    }
    show();
    raise();
    // the fit mode may have changed while the bar was away
    refreshFitCycleIcon();
    if(mMode == NAVBAR_ALWAYS || !docked) {
        if(docked)
            dockToBottom();
        return;
    }
    // auto: out of sight, unless the cursor is already down there
    if(withinHoverZone())
        expandBar();
    else
        collapseBar();
}

// The strip that brings the bar back is exactly the area the bar occupies when it
// is docked: reaching the bottom edge of the window is enough, no extra margin.
bool FloatingNavBar::withinHoverZone() const {
    QWidget *container = parentWidget();
    if(!container)
        return false;
    const QPoint p = container->mapFromGlobal(QCursor::pos());
    if(!container->rect().contains(p))
        return false;
    return p.y() >= container->height() - barHeight;
}

// Both directions move the bar in one step: no slide and no idle delay, the same
// reaction the video controls have. Before, the bar waited 450 ms before it even
// started a 170 ms slide, which is what made it feel sluggish.
void FloatingNavBar::expandBar() {
    // a bar that was dragged out of the strip stays where the user put it
    if(mMode != NAVBAR_AUTO || !mViewActive || !docked)
        return;
    QWidget *container = parentWidget();
    if(!container)
        return;
    const int y = qMax(0, container->height() - barHeight);
    if(pos().y() != y || width() != container->width())
        setGeometry(0, y, container->width(), barHeight);
}

// Moves the bar fully below the lower edge: nothing of it stays visible, and the
// trigger strip above the edge is what brings it back.
void FloatingNavBar::collapseBar() {
    if(mMode != NAVBAR_AUTO || !docked || !mViewActive)
        return;
    if(withinHoverZone())
        return;
    QWidget *container = parentWidget();
    if(!container)
        return;
    const int y = container->height();
    if(pos().y() != y || width() != container->width())
        setGeometry(0, y, container->width(), barHeight);
}

// Watches app-wide mouse moves so the trigger strip also works while the image
// widget below is handling the mouse. Nothing is intercepted: the event is
// always passed through.
bool FloatingNavBar::eventFilter(QObject *watched, QEvent *event) {
    // A context menu request over the bar belongs to the bar. The image viewer
    // would otherwise open its file menu on top of it, and on the fit button the
    // bar opens its mode menu instead.
    if(event->type() == QEvent::ContextMenu && isVisible()) {
        const QPoint cursor = QCursor::pos();
        if(rect().contains(mapFromGlobal(cursor))) {
            // already showing the menu for this very click
            if(!mMenuOpen && fitButton && fitButton->isVisible()
               && fitButton->rect().contains(fitButton->mapFromGlobal(cursor)))
                showFitModeMenu(fitButton);
            return true;
        }
    }
    Q_UNUSED(watched)
    // Cheap guards first: this filter sees every event in the application, so
    // avoid the coordinate math unless the app is actually interacting.
    if(event->type() != QEvent::MouseMove)
        return QWidget::eventFilter(watched, event);
    if(mMode != NAVBAR_AUTO || !mViewActive || !docked || !isVisible() || !window()->isActiveWindow())
        return QWidget::eventFilter(watched, event);

    if(withinHoverZone())
        expandBar();
    else
        collapseBar();
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
    // the card is only as wide as its buttons
    if(!docked)
        fitToContent();
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
        "  border-radius: %7px;"
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
     .arg(pressed.alpha())
     .arg(UiStyle::controlRadius()));
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

QString FloatingNavBar::defaultThemedIconName(const QString &shortcut) {
    const QString action = actionForShortcut(shortcut);
    if(action.isEmpty())
        return QString();
    return actionThemeIcons().value(action);
}

QStringList FloatingNavBar::themedIconNames() {
    QStringList names;
    for(const QString &name : actionThemeIcons())
        if(!names.contains(name))
            names << name;
    return names;
}

QString FloatingNavBar::separatorShortcut() {
    return QStringLiteral("|");
}

bool FloatingNavBar::isSeparator(const QString &shortcut) {
    return shortcut == separatorShortcut();
}

// Stored like a button, but instead of a key it cycles through the image fit
// modes: left click goes to the next one, right click opens a menu.
QString FloatingNavBar::fitCycleShortcut() {
    return QStringLiteral("@fit");
}

bool FloatingNavBar::isFitCycle(const QString &shortcut) {
    return shortcut == fitCycleShortcut();
}

QStringList FloatingNavBar::fitCycleActions() {
    return {QStringLiteral("fitWindow"), QStringLiteral("fitWidth"),
            QStringLiteral("fitNormal"), QStringLiteral("fitWindowStretch")};
}

QString FloatingNavBar::fitActionForMode(ImageFitMode mode) {
    switch(mode) {
    case FIT_WIDTH:
        return QStringLiteral("fitWidth");
    case FIT_ORIGINAL:
        return QStringLiteral("fitNormal");
    case FIT_WINDOW_STRETCH:
        return QStringLiteral("fitWindowStretch");
    default:
        // FIT_FREE (the image was zoomed away from a fit mode) shows the mode a
        // click would apply first
        return QStringLiteral("fitWindow");
    }
}

QString FloatingNavBar::fitActionName(const QString &action) {
    if(action == QLatin1String("fitWidth"))
        return tr("Fit width");
    if(action == QLatin1String("fitNormal"))
        return tr("Original size");
    if(action == QLatin1String("fitWindowStretch"))
        return tr("Stretch to window");
    return tr("Fit window");
}

void FloatingNavBar::setFitModeProvider(std::function<ImageFitMode()> provider) {
    mFitModeProvider = std::move(provider);
    refreshFitCycleIcon();
}

ImageFitMode FloatingNavBar::currentFitMode() const {
    return mFitModeProvider ? mFitModeProvider() : FIT_WINDOW;
}

void FloatingNavBar::refreshFitCycleIcon() {
    if(!fitButton)
        return;
    const int iconPx = 28;
    const QStringList actions = fitCycleActions();
    const QString action = fitActionForMode(currentFitMode());
    const QStringList configured = settings->navBarFitIcons();
    const QPixmap pixmap = iconForAction(action, configured.value(actions.indexOf(action)), iconPx);
    if(!pixmap.isNull()) {
        fitButton->setIcon(QIcon(pixmap));
        fitButton->setIconSize(pixmap.size());
        fitButton->setText(QString());
    } else {
        fitButton->setIcon(QIcon());
        fitButton->setText(fitActionName(action));
    }
    fitButton->setToolTip(tr("Image fit mode: %1\nLeft click: next mode - Right click: pick one")
                              .arg(fitActionName(action)));
}

void FloatingNavBar::applyFitAction(const QString &action) {
    if(action.isEmpty() || !actionManager)
        return;
    actionManager->invokeAction(action);
    // the viewer has the new mode by now, so the icon can follow right away
    refreshFitCycleIcon();
}

void FloatingNavBar::advanceFitMode() {
    const QStringList actions = fitCycleActions();
    if(actions.isEmpty())
        return;
    int next = actions.indexOf(fitActionForMode(currentFitMode())) + 1;
    if(next <= 0 || next >= actions.size())
        next = 0;
    applyFitAction(actions.at(next));
}

void FloatingNavBar::showFitModeMenu(QPushButton *button) {
    if(!button || mMenuOpen)
        return;
    const QString current = fitActionForMode(currentFitMode());
    QMenu menu(this);
    for(const QString &action : fitCycleActions()) {
        QAction *entry = menu.addAction(fitActionName(action));
        entry->setCheckable(true);
        entry->setChecked(action == current);
        connect(entry, &QAction::triggered, this, [this, action]() { applyFitAction(action); });
    }
    // above the button: the bar normally sits on the bottom edge, so a menu
    // dropped below the cursor would run off the screen
    const QPoint anchor = button->mapToGlobal(QPoint(button->width() / 2, 0));
    const QSize hint = menu.sizeHint();
    mMenuOpen = true;
    menu.exec(QPoint(qMax(0, anchor.x() - hint.width() / 2), anchor.y() - hint.height() - 4));
    mMenuOpen = false;
}

// Resolves the icon of a button: a configured "theme:<name>" or file path wins,
// otherwise the action's own icon (the vector set under Qlementine, the png
// resources otherwise). A configured icon that no longer exists falls back to the
// automatic one, and built-in icons are masks, so they get tinted.
QPixmap FloatingNavBar::iconForAction(const QString &action, const QString &configured, int iconPx) {
    const bool userThemed = configured.startsWith(QLatin1String("theme:"));
    bool custom = !configured.isEmpty();
    const QString customPath = userThemed ? QString() : configured;
    QString iconPath = custom ? customPath : actionIcons().value(action);
    bool themed = userThemed
                  || (!custom && UiStyle::useQlementine() && actionThemeIcons().contains(action));
    QPixmap pixmap;

    if(themed) {
        const QString name = userThemed ? configured.mid(6)
                                        : actionThemeIcons().value(action);
        iconPath = QStringLiteral(":/qlementine/icons/16/%1.svg").arg(name);
        pixmap = QIcon(iconPath).pixmap(iconPx * 2, iconPx * 2);
    } else if(!iconPath.isEmpty()) {
        // built-in icons ship an @2x variant which stays crisp at this size
        if(!custom) {
            QString hiRes = iconPath;
            hiRes.replace(QLatin1String(".png"), QLatin1String("@2x.png"));
            pixmap = QPixmap(hiRes);
        }
        if(pixmap.isNull())
            pixmap = QPixmap(iconPath);
    }

    if(pixmap.isNull() && custom) {
        custom = false;
        // the replacement is a PNG mask, not a vector icon: it needs the same
        // rotation/tint treatment the built-in icons get
        themed = false;
        iconPath = actionIcons().value(action);
        QString hiRes = iconPath;
        hiRes.replace(QLatin1String(".png"), QLatin1String("@2x.png"));
        pixmap = QPixmap(hiRes);
        if(pixmap.isNull())
            pixmap = QPixmap(iconPath);
    }

    if(!iconPath.isEmpty() && !pixmap.isNull()) {
        if(!custom || userThemed) {
            // built-in and vector icons are masks, so they take the theme colour
            ImageLib::recolor(pixmap, settings->colorScheme().icons);
        }
        // custom image files keep the colours the user picked
        pixmap = pixmap.scaled(iconPx, iconPx, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return pixmap;
}

void FloatingNavBar::rebuildButtons() {
    while(!buttons.isEmpty())
        delete buttons.takeFirst();
    while(!separators.isEmpty())
        delete separators.takeFirst();
    // the cycle button is one of `buttons` and was just deleted with them
    fitButton = nullptr;

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

        // a divider between two groups of buttons
        if(isSeparator(shortcut)) {
            NavBarSeparator *separator = new NavBarSeparator(this);
            layout->insertWidget(layout->count() - 1, separator);
            separators.append(separator);
            continue;
        }

        // a button that cycles the fit mode instead of running one shortcut
        if(isFitCycle(shortcut)) {
            QPushButton *button = new QPushButton(this);
            button->setFocusPolicy(Qt::NoFocus);
            button->setCursor(Qt::PointingHandCursor);
            button->setFlat(true);
            button->setAutoDefault(false);
            fitButton = button;
            connect(button, &QPushButton::clicked, this, [this]() { advanceFitMode(); });
            layout->insertWidget(layout->count() - 1, button);
            buttons.append(button);
            refreshFitCycleIcon();
            continue;
        }

        const QString action = actionForShortcut(shortcut);

        QPushButton *button = new QPushButton(this);
        button->setFocusPolicy(Qt::NoFocus);
        button->setCursor(Qt::PointingHandCursor);
        button->setFlat(true);
        button->setAutoDefault(false);

        const int iconPx = 28;

        const QPixmap pixmap = iconForAction(action, mIcons.value(i), iconPx);
        if(!pixmap.isNull()) {
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
    const bool wasDocked = docked;
    docked = true;
    // auto mode rests fully out of sight; applyState()/the trigger strip brings
    // it back in
    const int y = (mMode == NAVBAR_AUTO) ? container->height()
                                         : container->height() - barHeight;
    setGeometry(0, qMax(0, y), container->width(), barHeight);
    if(!wasDocked)
        emit layoutStateChanged();
}

// Undocked the bar is a floating card like the video controls: only as wide as
// its buttons, rounded, with the same surface. Docked it goes back to the full
// width strip that sits on the bottom edge.
void FloatingNavBar::fitToContent() {
    QWidget *container = parentWidget();
    if(!container)
        return;
    const int w = qMin(sizeHint().width(), container->width());
    // keep the current centre, so a resize does not make it jump sideways
    const int x = qBound(0, pos().x() + (width() - w) / 2,
                         qMax(0, container->width() - w));
    const int y = qBound(0, pos().y(), qMax(0, container->height() - barHeight));
    if(geometry() != QRect(x, y, w, barHeight))
        setGeometry(x, y, w, barHeight);
}

void FloatingNavBar::clampToParent() {
    QWidget *container = parentWidget();
    if(!container)
        return;
    const int y = qBound(0, pos().y(), qMax(0, container->height() - barHeight));
    const int w = docked ? container->width() : qMin(width(), container->width());
    const int x = docked ? 0 : qBound(0, pos().x(), qMax(0, container->width() - w));
    setGeometry(x, y, w, barHeight);
}

void FloatingNavBar::onContainerResized(QSize size) {
    Q_UNUSED(size)
    if(!docked) {
        fitToContent();
        clampToParent();
        return;
    }
    // follow the container without animating, so a resize does not make the bar
    // slide in or out
    if(mMode == NAVBAR_AUTO) {
        QWidget *container = parentWidget();
        if(!container)
            return;
        const int y = (mViewActive && withinHoverZone()) ? qMax(0, container->height() - barHeight)
                                                         : container->height();
        setGeometry(0, y, container->width(), barHeight);
        return;
    }
    dockToBottom();
}

void FloatingNavBar::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter painter(this);
    // Same surface as the video controls: their card is filled with
    // QPalette::Window and bordered with QPalette::Mid (see
    // UiStyle::paintBackground and the "FloatingWidget" rule in the stylesheet),
    // so the bar uses exactly those colours instead of the neutral button colour
    // it used to look like one big button.
    const ColorScheme &colors = settings->colorScheme();
    const QColor fill = UiStyle::useQlementine() ? palette().color(QPalette::Window)
                                                 : colors.widget;
    const QColor line = UiStyle::useQlementine() ? palette().color(QPalette::Mid)
                                                 : colors.widget_border;
    if(!docked) {
        // free-floating: the whole card, rounded, like the video controls
        const int radius = UiStyle::useQlementine() ? UiStyle::controlRadius() : 3;
        const QRectF card = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(line, 1.0));
        painter.setBrush(fill);
        painter.drawRoundedRect(card, radius, radius);
        return;
    }
    // docked: a strip across the bottom edge, only the top edge is a border
    painter.fillRect(rect(), fill);
    painter.setPen(QPen(line, 1));
    painter.drawLine(QPoint(0, 0), QPoint(width(), 0));
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void FloatingNavBar::enterEvent(QEnterEvent *event) {
#else
void FloatingNavBar::enterEvent(QEvent *event) {
#endif
    if(mMode == NAVBAR_AUTO) {
        // the bar comes back into view: make sure the fit button shows the mode
        // the viewer is in now
        refreshFitCycleIcon();
        expandBar();
    }
    QWidget::enterEvent(event);
}

void FloatingNavBar::leaveEvent(QEvent *event) {
    // the trigger strip reaches above the bar, so this only hides the bar once
    // the cursor left the strip too (collapseBar checks that itself)
    if(mMode == NAVBAR_AUTO)
        collapseBar();
    QWidget::leaveEvent(event);
}

void FloatingNavBar::mousePressEvent(QMouseEvent *event) {
    // A right click belongs to the bar, not to the image under it: the bar is an
    // overlay, so the press is swallowed here and the image never opens its file
    // menu on top of the bar. On the fit button this opens the mode menu.
    if(event->button() == Qt::RightButton) {
        if(fitButton && fitButton->isVisible()
           && fitButton->rect().contains(fitButton->mapFromGlobal(eventGlobalPos(event))))
            showFitModeMenu(fitButton);
        event->accept();
        return;
    }
    if(event->button() == Qt::LeftButton) {
        QWidget *container = parentWidget();
        const bool wasDocked = docked;
        docked = false;
        dragging = true;
        if(wasDocked && container) {
            // undocked: shrink to a card under the cursor, centred on it so the
            // drag starts where the user grabbed. This is also how the bar is
            // taken out of the bottom strip in auto mode: the strip no longer owns
            // it once it is undocked, so it stays where it is dropped instead of
            // hiding again.
            const int w = qMin(sizeHint().width(), container->width());
            const QPoint grab = container->mapFromGlobal(eventGlobalPos(event));
            const int x = qBound(0, grab.x() - w / 2, qMax(0, container->width() - w));
            setGeometry(x, pos().y(), w, barHeight);
        }
        dragOffset = eventGlobalPos(event) - pos();
        setCursor(Qt::ClosedHandCursor);
        // the bar is a free-floating overlay now, so its strip can go back to
        // the image while it is being dragged
        emit layoutStateChanged();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void FloatingNavBar::mouseMoveEvent(QMouseEvent *event) {
    if(dragging && (event->buttons() & Qt::LeftButton)) {
        QWidget *container = parentWidget();
        const QPoint target = eventGlobalPos(event) - dragOffset;
        int y = target.y();
        int x = target.x();
        if(container) {
            y = qBound(0, y, qMax(0, container->height() - barHeight));
            x = qBound(0, x, qMax(0, container->width() - width()));
        }
        move(x, y);
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void FloatingNavBar::mouseReleaseEvent(QMouseEvent *event) {
    // see mousePressEvent: the release must not travel on to the image either,
    // or its context menu opens on top of the bar
    if(event->button() == Qt::RightButton) {
        event->accept();
        return;
    }
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
