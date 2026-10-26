#pragma once

#include <QString>
#include <QDebug>
#include <QPixmap>
#include <memory>
#include "utils/imagelib.h"
#include "utils/stuff.h"
#include "sourcecontainers/documentinfo.h"

class Image {
public:
    Image(QString);
    Image(std::unique_ptr<DocumentInfo>);
    virtual ~Image() = 0;
    virtual std::unique_ptr<QPixmap> getPixmap() = 0;
    virtual std::shared_ptr<const QImage> getImage() = 0;
    DocumentType type() const;
    QString filePath() const;
    virtual int height() = 0;
    virtual int width() = 0;
    virtual QSize size() = 0;
    bool isLoaded() const;
    // Two-pass loading support. A "preview only" image holds a downscaled copy
    // of itself; the full resolution data is decoded later by loadFull().
    virtual bool isPreviewOnly() const { return false; }
    virtual bool loadFull() { return false; }
    // whatever is currently held, without triggering a decode
    virtual std::shared_ptr<const QImage> getPreviewImage() { return getImage(); }
    virtual bool save() = 0;
    virtual bool save(QString destPath) = 0;
    QString fileName() const;
    QString baseName() const;
    bool isEdited() const;
    qint64 fileSize() const;
    QDateTime lastModified() const;
    QMap<QString, QString> getExifTags();

protected:
    virtual void load() = 0;
    std::unique_ptr<DocumentInfo> mDocInfo;
    bool mLoaded, mEdited;
    QString mPath;
    QSize resolution;
};
