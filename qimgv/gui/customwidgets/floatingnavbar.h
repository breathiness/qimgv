/*
 * Bottom navigation bar, modelled after the 2345 image viewer toolbar.
 *
 * Full-width bar docked to the bottom of the main window with a centred row of
 * icon buttons. Every button is an alias for a keyboard shortcut: clicking it
 * runs the same action that pressing the assigned shortcut would run.
 *
 * Configuration lives in Preferences -> "Navigation bar".
 *
 * Auto-hide: with the option enabled the bar slides away leaving a thin strip
 * and comes back as soon as the mouse enters the bottom hover zone. The zone is
 * bigger than the bar itself and is detected from global cursor position, so it
 * never swallows clicks meant for the image.
 *
 * Just set the parent and call setButtons(); do not put it into a layout.
 */

#pragma once

#include <QWidget>
#include <QStringList>
#include <QList>
#include <QPoint>
#include <QSize>

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

    // when enabled only a thin strip stays visible and the bar slides up as
    // soon as the mouse enters the bottom hover zone
    void setAutoHide(bool mode);
    bool autoHide() const;

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
    void collapseBar();
    void animateTo(int y);
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
    // auto-hide state
    bool mAutoHide;
    int peekHeight;
    // extra height above the bar that still counts as "close to the bar"
    int hoverMargin;
    QPropertyAnimation *posAnim;
    QTimer *collapseTimer;
};
