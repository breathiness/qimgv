#pragma once

#include <QObject>
#include <QRunnable>
#include "utils/imagefactory.h"

class LoaderRunnable: public QObject, public QRunnable
{
    Q_OBJECT
public:
    // previewPass: only decode a downscaled copy and emit previewReady();
    // the full resolution pass happens later, on demand (see FullImageRunnable)
    LoaderRunnable(QString _path, bool _previewPass = false);
    void run();
private:
    QString path;
    bool previewPass;
signals:
    void previewReady(std::shared_ptr<Image>, QString);
    void finished(std::shared_ptr<Image>, QString);
    void failed(QString);
};

// decodes the full resolution data of an image that is currently being shown
// as a preview
class FullImageRunnable: public QObject, public QRunnable
{
    Q_OBJECT
public:
    FullImageRunnable(std::shared_ptr<Image> _image);
    void run();
private:
    std::shared_ptr<Image> image;
signals:
    void finished(std::shared_ptr<Image>);
};
