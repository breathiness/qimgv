#include "loaderrunnable.h"

#include <QElapsedTimer>

LoaderRunnable::LoaderRunnable(QString _path, bool _previewPass)
    : path(_path), previewPass(_previewPass)
{
}

void LoaderRunnable::run() {
    //QElapsedTimer t;
    //t.start();
    QSize previewTarget = previewPass ? ImageStatic::suggestPreviewTarget(path) : QSize();
    auto image = ImageFactory::createImage(path, previewTarget);
    if(image && image->isPreviewOnly())
        emit previewReady(image, path);
    //qDebug() << "L: " << t.elapsed();
    emit finished(image, path);
}

FullImageRunnable::FullImageRunnable(std::shared_ptr<Image> _image)
    : image(_image)
{
}

void FullImageRunnable::run() {
    if(image)
        image->loadFull();
    emit finished(image);
}
