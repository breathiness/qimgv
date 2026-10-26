#pragma once

#include <QThreadPool>
#include "components/cache/thumbnailcache.h"
#include "loaderrunnable.h"

class Loader : public QObject {
    Q_OBJECT
public:
    explicit Loader();
    std::shared_ptr<Image> load(QString path);
    void loadAsyncPriority(QString path);
    // previewOnly: decode just a downscaled copy (used for preloading, so that
    // neighbouring large images do not sit in memory at full resolution)
    void loadAsync(QString path, bool previewOnly = false);
    // decodes the full resolution data of a preview-only image in the background
    void requestFullResolution(std::shared_ptr<Image> img);

    void clearTasks();
    bool isBusy() const;
    bool isLoading(QString path);
private:
    QHash<QString, LoaderRunnable*> tasks;
    QThreadPool *pool;    
    void clearPool();
    void doLoadAsync(QString path, int priority, bool previewPass);

signals:
    // a downscaled copy is ready; the full image will follow as loadFinished()
    void previewReady(std::shared_ptr<Image>, const QString &path);
    void loadFinished(std::shared_ptr<Image>, const QString &path);
    void loadFailed(const QString &path);
    void fullImageReady(std::shared_ptr<Image>);

private slots:
    void onLoadFinished(std::shared_ptr<Image>, const QString&);
};
