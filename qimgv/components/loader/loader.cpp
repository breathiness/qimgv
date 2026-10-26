#include "loader.h"

#include <QThread>

Loader::Loader() {
    pool = new QThreadPool(this);
    // 2 was very conservative for preloading adjacent images: the work is I/O
    // plus decode bound and benefits from a couple more workers, while staying
    // low enough not to spike memory on large files
    const int idealThreads = QThread::idealThreadCount();
    pool->setMaxThreadCount(qBound(2, idealThreads / 2, 4));
}

void Loader::clearTasks() {
    clearPool();
    pool->waitForDone();
}

bool Loader::isBusy() const {
    return (tasks.count() != 0);
}

bool Loader::isLoading(QString path) {
    return tasks.contains(path);
}

std::shared_ptr<Image> Loader::load(QString path) {
    return ImageFactory::createImage(path);
}

// clears all buffered tasks before loading
void Loader::loadAsyncPriority(QString path) {
    clearPool();
    doLoadAsync(path, 1, true);
}

void Loader::loadAsync(QString path, bool previewOnly) {
    doLoadAsync(path, 0, previewOnly);
}

void Loader::requestFullResolution(std::shared_ptr<Image> img) {
    if(!img || !img->isPreviewOnly())
        return;
    auto runnable = new FullImageRunnable(img);
    runnable->setAutoDelete(false);
    connect(runnable, &FullImageRunnable::finished, this, [this, runnable](std::shared_ptr<Image> image) {
        runnable->deleteLater();
        emit fullImageReady(image);
    });
    pool->start(runnable, 1);
}

void Loader::doLoadAsync(QString path, int priority, bool previewPass) {
    if(tasks.contains(path)) {
        return;
    }

    auto runnable = new LoaderRunnable(path, previewPass);
    runnable->setAutoDelete(false);
    tasks.insert(path, runnable);
    connect(runnable, &LoaderRunnable::previewReady, this, &Loader::previewReady, Qt::UniqueConnection);
    connect(runnable, &LoaderRunnable::finished, this, &Loader::onLoadFinished, Qt::UniqueConnection);
    pool->start(runnable, priority);
}

void Loader::onLoadFinished(std::shared_ptr<Image> image, const QString &path) {
    auto task = tasks.take(path);
    delete task;
    if(!image)
        emit loadFailed(path);
    else
        emit loadFinished(image, path);
}

void Loader::clearPool() {
    QHashIterator<QString, LoaderRunnable*> i(tasks);
    while (i.hasNext()) {
        i.next();
        if(pool->tryTake(i.value())) {
            delete tasks.take(i.key());
        }
    }
}
