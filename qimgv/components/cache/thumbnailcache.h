#pragma once

#include <QObject>
#include <QDir>
#include <QDebug>
#include "settings.h"
#include "sourcecontainers/thumbnail.h"

class ThumbnailCache : public QObject
{
    Q_OBJECT
public:
    explicit ThumbnailCache();

    void saveThumbnail(QImage *image, QString id);
    QImage* readThumbnail(QString id);
    QString thumbnailPath(QString id);

signals:

public slots:

private:
    QString cacheDirPath;
};
