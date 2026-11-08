// The window frame qimgv draws itself, in place of the system title bar. Only
// used while the Qlementine styles are active; the window is frameless then and
// this bar provides the title, the window buttons and the drag handle.
//
// Colours come from the active theme (UiStyle::chromePalette), so switching
// Material Light / Material Dark / Dark / Light at runtime repaints the bar
// without rebuilding the window.

#pragma once

#include <QPixmap>
#include <QString>
#include <QWidget>

class QHBoxLayout;

class WindowTitleBar : public QWidget {
    Q_OBJECT
public:
    explicit WindowTitleBar(QWidget *parent = nullptr);
    ~WindowTitleBar();

    void setTitle(const QString &title);
    // swaps the middle button between "maximize" and "restore"
    void setMaximized(bool maximized);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void toggleMaximized();
    void createButtons();

    QString mTitle;
    QWidget *mButtonBox = nullptr;
    QHBoxLayout *mButtonLayout = nullptr;
    QWidget *mMinimizeButton = nullptr;
    QWidget *mMaximizeButton = nullptr;
    QWidget *mCloseButton = nullptr;
    QPixmap mIcon;
    bool mMaximized = false;
};
