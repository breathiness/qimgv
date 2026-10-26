#include "scalerrunnable.h"

#include <QElapsedTimer>

ScalerRunnable::ScalerRunnable() {
}

void ScalerRunnable::setRequest(ScalerRequest r) {
    req = r;
}

void ScalerRunnable::run() {
    emit started(req);
    //QElapsedTimer t;
    //t.start();
    QImage *scaled = nullptr;
    // scale from whatever is held in memory: for big images that may be the
    // preview, which is all that is needed at these zoom levels
    std::shared_ptr<const QImage> src = req.image ? req.image->getPreviewImage() : nullptr;
    if(!src) {
        emit finished(scaled, req);
        return;
    }
    if(req.filter == 0 || (req.size.width() > req.image->width() && !settings->smoothUpscaling())) {
        scaled = ImageLib::scaled(src, req.size, QI_FILTER_NEAREST);
    } else {
        scaled = ImageLib::scaled(src, req.size, req.filter);
    }
    //qDebug() << ">> " << req.size << ": " << t.elapsed();
    emit finished(scaled, req);
}
