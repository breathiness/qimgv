#pragma once

#include <QObject>
#include <QDir>
#include <QDebug>
#include <memory>
#include "settings.h"
#include "sourcecontainers/thumbnail.h"

class ThumbnailCache : public QObject
{
    Q_OBJECT
public:
    explicit ThumbnailCache();

    void saveThumbnail(const QImage *image, QString id);
    // null when there is no usable cached thumbnail. The result is owned by the
    // caller; it used to be a raw pointer that every path out had to delete.
    std::unique_ptr<QImage> readThumbnail(QString id);
    QString thumbnailPath(QString id);

signals:

public slots:

private:
    QString cacheDirPath;
};
