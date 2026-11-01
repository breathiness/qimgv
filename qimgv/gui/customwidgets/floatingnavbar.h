/*
 * Bottom navigation bar, modelled after the 2345 image viewer toolbar.
 *
 * Full-width bar docked to the bottom of the main window with a centred row of
 * icon buttons. Every button is an alias for a keyboard shortcut: clicking it
 * runs the same action that pressing the assigned shortcut would run.
 *
 * Configuration lives in Preferences -> "Navigation bar".
 *
 * Modes: "always" keeps it docked and visible, "auto" hides it completely and
 * shows it as soon as the mouse enters the bottom trigger strip, "never" leaves
 * it out. The strip is much taller than the bar itself and is detected from the
 * global cursor position, so it never swallows clicks meant for the image. The
 * video controls use the same strip, so both appear at the same distance from
 * the bottom edge. The bar exists for the image viewer only;
 * setViewActive(false) takes it off the folder view instead of leaving a strip
 * behind.
 *
 * Dragging works in both "always" and "auto": pressing the bar turns it into a
 * floating card, and dropping it near the bottom edge docks it again (double
 * click does the same). A bar dragged out of the strip in "auto" mode stays
 * where it was dropped instead of hiding again - the strip only owns a docked
 * bar - so it can be put anywhere and stays there.
 *
 * Just set the parent and call setButtons(); do not put it into a layout.
 */

#pragma once

#include <QWidget>
#include <QStringList>
#include <QList>
#include <QPoint>
#include <QSize>
#include <functional>

#include "settings.h"

class QHBoxLayout;
class QPushButton;

class FloatingNavBar : public QWidget {
    Q_OBJECT
public:
    // Height of the bar in pixels. Other overlays (the video controls) use it
    // to keep clear of the bar instead of hard-coding the number.
    static constexpr int kBarHeight = 60;

    explicit FloatingNavBar(QWidget *parent = nullptr);

    // replaces the whole button list (used when settings are (re)loaded)
    void setButtons(const QStringList &shortcuts, const QStringList &icons);
    QStringList shortcuts() const;
    // per-button custom icon paths; empty entry = automatic icon
    QStringList icons() const;

    // adds one empty button (only used by the "+ Add button" placeholder)
    void insertButton(int at);

    // icon that would be used automatically for a shortcut (empty = none)
    static QString defaultIconPath(const QString &shortcut);
    // name in the vector icon set that the shortcut resolves to (empty = none)
    static QString defaultThemedIconName(const QString &shortcut);
    // every icon name in the vector set a button can be given
    static QStringList themedIconNames();

    // Shortcut list entry that is not a button but a divider between two groups
    // of buttons. It is stored like any other entry, so it keeps its position.
    static QString separatorShortcut();
    static bool isSeparator(const QString &shortcut);

    // Shortcut list entry that is not bound to a key but cycles the image fit
    // mode: left click goes to the next mode, right click opens a menu with all
    // of them, and the icon shows the mode that is active.
    static QString fitCycleShortcut();
    static bool isFitCycle(const QString &shortcut);
    // modes in the order the button cycles them (action names)
    static QStringList fitCycleActions();
    static QString fitActionForMode(ImageFitMode mode);
    // Human readable name of one of fitCycleActions().
    static QString fitActionName(const QString &action);

    // The same for the mouse wheel: a button that cycles WHEEL_SWITCH_IMAGES,
    // WHEEL_ZOOM and WHEEL_SCROLL.
    static QString wheelCycleShortcut();
    static bool isWheelCycle(const QString &shortcut);
    static QString wheelModeName(WheelMode mode);
    // icon of the vector set that stands for the mode
    static QString wheelModeIconName(WheelMode mode);

    // Where the current mode comes from. The application points this at the
    // image viewer, so the button icon follows the mode that is really active
    // instead of remembering the last one it set itself.
    void setFitModeProvider(std::function<ImageFitMode()> provider);
    ImageFitMode currentFitMode() const;
    // re-reads the mode and updates the icon of the cycle buttons
    void refreshFitCycleIcon();

    // how the bar behaves: always visible, hidden until the mouse reaches the
    // bottom edge, or not shown at all
    void setMode(NavBarMode mode);
    NavBarMode mode() const;
    // the bar belongs to the image viewer; the folder view turns this off and
    // the bar disappears from that page entirely
    void setViewActive(bool active);
    // true while the bar sits in the bottom strip of its parent. In "always"
    // mode the main window keeps that strip free for the bar instead of letting
    // it cover the bottom of the image.
    bool isDocked() const;

public slots:
    // keeps the bar sized/positioned after the parent got resized
    void onContainerResized(QSize size);
    // re-tints icons and background after a theme change
    void refreshTheme();

signals:
    // buttons were added / removed / edited and should be persisted
    void shortcutsChanged();
    // mode or docked state changed: the parent may have to reserve space for
    // the bar (see MW::updateNavBarSpace)
    void layoutStateChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) override;
#else
    void enterEvent(QEvent *event) override;
#endif
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void rebuildButtons();
    void updateFromShortcuts();
    void applyTheme();
    // icon of one button: `configured` ("theme:<name>" or a file path) wins,
    // otherwise the action's own icon is used
    QPixmap iconForAction(const QString &action, const QString &configured, int iconPx);
    // vector icon of the icon set, tinted like the built-in ones
    QPixmap themedIconPixmap(const QString &name, int iconPx);
    // radio dot for the menus of the mode buttons (filled when active)
    QPixmap radioIndicator(bool checked, int px);
    // runs one of fitCycleActions() and refreshes the button afterwards
    void applyFitAction(const QString &action);
    void advanceFitMode();
    void showFitModeMenu(QPushButton *button);
    void advanceWheelMode();
    void showWheelModeMenu(QPushButton *button);
    void refreshWheelButton();
    void clampToParent();
    void dockToBottom();
    // undocked: shrink to the width of the buttons, like the video controls card
    void fitToContent();
    void expandBar();
    void collapseBar();
    // shows/hides and positions the bar for the current mode and page
    void applyState();
    // true when the cursor sits inside the bottom trigger strip of the parent
    bool withinHoverZone() const;

    QHBoxLayout *layout;
    QList<QPushButton*> buttons;
    // dividers between groups of buttons (see separatorShortcut())
    QList<QWidget*> separators;
    QStringList mShortcuts;
    QStringList mIcons;
    QPoint dragOffset;
    bool dragging;
    // true while the bar sits flush at the bottom of its parent
    bool docked;
    int barHeight;
    NavBarMode mMode;
    // false while a page without the bar (the folder view) is active
    bool mViewActive;
    // the fit-mode cycle button, when one is configured; null otherwise
    QPushButton *fitButton;
    // the wheel-mode cycle button, when one is configured; null otherwise
    QPushButton *wheelButton;
    // true while the fit mode menu is on screen (it runs its own event loop)
    bool mMenuOpen;
    std::function<ImageFitMode()> mFitModeProvider;
};
