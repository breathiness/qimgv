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
 * slides it up as soon as the mouse enters the bottom hover zone, "never"
 * leaves it out. The zone is bigger than the bar itself and is detected from
 * the global cursor position, so it never swallows clicks meant for the image.
 * The bar exists for the image viewer only; setViewActive(false) takes it off
 * the folder view instead of leaving a strip behind.
 *
 * Just set the parent and call setButtons(); do not put it into a layout.
 */

#pragma once

#include <QWidget>
#include <QStringList>
#include <QList>
#include <QPoint>
#include <QSize>

#include "settings.h"

class QHBoxLayout;
class QPushButton;
class QPropertyAnimation;
class QTimer;

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

    // how the bar behaves: always visible, hidden until the mouse reaches the
    // bottom edge, or not shown at all
    void setMode(NavBarMode mode);
    NavBarMode mode() const;
    // the bar belongs to the image viewer; the folder view turns this off and
    // the bar disappears from that page entirely
    void setViewActive(bool active);

public slots:
    // keeps the bar sized/positioned after the parent got resized
    void onContainerResized(QSize size);
    // re-tints icons and background after a theme change
    void refreshTheme();

signals:
    // buttons were added / removed / edited and should be persisted
    void shortcutsChanged();

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
    void clampToParent();
    void dockToBottom();
    void expandBar();
    void collapseBar(bool instant = false);
    void animateTo(int y);
    // shows/hides and positions the bar for the current mode and page
    void applyState();
    // true when the cursor sits inside the bottom hover zone of the parent
    bool withinHoverZone() const;

    QHBoxLayout *layout;
    QList<QPushButton*> buttons;
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
    // extra height above the bar that still counts as "close to the bar"
    int hoverMargin;
    QPropertyAnimation *posAnim;
    QTimer *collapseTimer;
};
