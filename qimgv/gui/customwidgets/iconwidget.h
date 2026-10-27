#pragma once

#include <QWidget>
#include <QStyleOption>
#include <QPainter>
#include <QDebug>
#include <QMouseEvent>
#include "settings.h"
#include "utils/imagelib.h"

enum IconColorMode {
    ICON_COLOR_CUSTOM,
    ICON_COLOR_THEME,
    ICON_COLOR_SOURCE
};

class IconWidget : public QWidget {
public:
    explicit IconWidget(QWidget *parent = nullptr);
    ~IconWidget();
    void setIconPath(QString path);
    void setIconOffset(int x, int y);
    void setColorMode(IconColorMode _mode);
    void setColor(QColor _color);
    QSize minimumSizeHint() const;

protected:
    void paintEvent(QPaintEvent *event);
    bool event(QEvent *event);

private slots:
    void onSettingsChanged();

private:
    void loadIcon();
    void applyColor();
    void paintChrome(QPainter &painter);
    void applyChromeMetrics();

    bool mHovered = false;
    bool mChromeMetricsDone = false;

    QString iconPath;
    QColor color;
    IconColorMode colorMode = ICON_COLOR_THEME;
    bool hiResPixmap;
    QPoint iconOffset;
    QPixmap *pixmap;
    qreal dpr, pixmapDrawScale;
};
