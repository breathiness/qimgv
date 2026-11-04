#pragma once

#include "gui/customwidgets/overlaywidget.h"
#include "gui/customwidgets/entryinfoitem.h"
#include "gui/customwidgets/sectioninfoitem.h"
#include "utils/imageinfo.h"
#include <QWheelEvent>

namespace Ui {
class ImageInfoOverlay;
}

class ImageInfoOverlay : public OverlayWidget
{
    Q_OBJECT

public:
    explicit ImageInfoOverlay(FloatingWidgetContainer *parent = nullptr);
    ~ImageInfoOverlay();
    // Entries arrive grouped: each section gets a heading before its rows.
    void setInfo(const QList<ImageInfoSection> &info);

public slots:
    void show();

protected:
    void wheelEvent(QWheelEvent *event);
private:
    void clearItems();
    Ui::ImageInfoOverlay *ui;
    QList<QWidget *> items;
    QLabel entryStub;
};
