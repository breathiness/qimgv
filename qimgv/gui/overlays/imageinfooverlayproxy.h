#pragma once

#include "gui/overlays/imageinfooverlay.h"

struct ImageInfoOverlayStateBuffer {
    QList<ImageInfoSection> info;
};

class ImageInfoOverlayProxy {
public:
    explicit ImageInfoOverlayProxy(FloatingWidgetContainer *parent = nullptr);
    ~ImageInfoOverlayProxy();
    void init();
    void show();
    void hide();

    void setInfo(const QList<ImageInfoSection> &info);
    bool isHidden();
private:
    FloatingWidgetContainer *container;
    ImageInfoOverlay *overlay;
    ImageInfoOverlayStateBuffer stateBuf;
};
