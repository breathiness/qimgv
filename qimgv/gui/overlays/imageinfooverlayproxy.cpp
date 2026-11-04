#include "imageinfooverlayproxy.h"

ImageInfoOverlayProxy::ImageInfoOverlayProxy(FloatingWidgetContainer *parent)
    : container(parent),
      overlay(nullptr)
{
}

ImageInfoOverlayProxy::~ImageInfoOverlayProxy() {
    if(overlay)
        overlay->deleteLater();
}

void ImageInfoOverlayProxy::show() {
    init();
    overlay->show();
}

void ImageInfoOverlayProxy::hide() {
    if(overlay)
        overlay->hide();
}

void ImageInfoOverlayProxy::init() {
    if(overlay)
        return;
    overlay = new ImageInfoOverlay(container);
    overlay->setInfo(stateBuf.info);
}

bool ImageInfoOverlayProxy::isHidden() {
    return overlay ? overlay->isHidden() : true;
}

void ImageInfoOverlayProxy::setInfo(const QList<ImageInfoSection> &_info) {
    if(overlay)
        overlay->setInfo(_info);
    else
        stateBuf.info = _info;
}
