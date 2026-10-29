#include "directorypresenter.h"

#include <QApplication>
#include <QFileInfo>
#include <QHash>
#include <QPainter>
#include <QPolygonF>
#include <QSet>
#include <QtSvg/QSvgRenderer>

#include "settings.h"
#include "sourcecontainers/thumbnail.h"

namespace {

bool isVideoFilePath(const QString &filePath) {
    // The list of video extensions is static, build it once instead of copying
    // it out of the settings for every file in the directory.
    static const QSet<QString> videoSuffixes = []() {
        QSet<QString> suffixes;
        const QList<QByteArray> formats = settings->videoFormats().values();
        for(const QByteArray &format : formats)
            suffixes.insert(QString::fromLatin1(format).toLower());
        return suffixes;
    }();

    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return !suffix.isEmpty() && videoSuffixes.contains(suffix);
}

// Small film-strip glyph shown while the real frame is being extracted. Video
// thumbnails spawn an mpv process, which takes a while, so without this the
// tile looks like an empty slot for several seconds.
QPixmap makeFilmGlyph(int size) {
    if(size <= 0)
        size = 100;
    const qreal dpr = qApp->devicePixelRatio();
    QPixmap pixmap(QSize(size, size) * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QColor color = settings->colorScheme().folderview_hc2;
    const qreal side = size * 0.46;
    const QRectF film((size - side) / 2.0, (size - side) / 2.0, side, side);
    const qreal stroke = qMax(1.0, size * 0.035);

    p.setPen(QPen(color, stroke));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(film, side * 0.10, side * 0.10);

    // sprocket holes along the top and bottom edges
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    const qreal holeW = side * 0.11;
    const qreal holeH = side * 0.09;
    for(int i = 0; i < 3; i++) {
        const qreal x = film.left() + side * (0.15 + i * 0.26);
        p.drawRoundedRect(QRectF(x, film.top() + side * 0.075, holeW, holeH), 1.0, 1.0);
        p.drawRoundedRect(QRectF(x, film.bottom() - side * 0.075 - holeH, holeW, holeH), 1.0, 1.0);
    }

    // play triangle in the middle
    const qreal cx = film.center().x() + side * 0.03;
    const qreal cy = film.center().y();
    const qreal t = side * 0.13;
    QPolygonF triangle;
    triangle << QPointF(cx - t, cy - t) << QPointF(cx - t, cy + t) << QPointF(cx + t * 1.15, cy);
    p.drawPolygon(triangle);

    p.end();
    return pixmap;
}

// One shared pixmap for every video placeholder: a folder can hold hundreds of
// videos and each placeholder thumbnail points at the same image.
std::shared_ptr<QPixmap> filmGlyphPixmap(int size) {
    static QHash<int, std::shared_ptr<QPixmap>> cache;
    auto it = cache.constFind(size);
    if(it != cache.constEnd())
        return it.value();
    auto pixmap = std::make_shared<QPixmap>(makeFilmGlyph(size));
    cache.insert(size, pixmap);
    return pixmap;
}

std::shared_ptr<Thumbnail> videoPlaceholderThumbnail(const QString &filePath) {
    const int size = settings->folderViewIconSize();
    return std::make_shared<Thumbnail>(QFileInfo(filePath).fileName(), " [v]", size,
                                       filmGlyphPixmap(size));
}

} // namespace

DirectoryPresenter::DirectoryPresenter(QObject *parent) : QObject(parent), mShowDirs(false) {
    connect(&thumbnailer, &Thumbnailer::thumbnailReady, this, &DirectoryPresenter::onThumbnailReady);
}

void DirectoryPresenter::unsetModel() {
    disconnect(model.get(), &DirectoryModel::fileRemoved,  this, &DirectoryPresenter::onFileRemoved);
    disconnect(model.get(), &DirectoryModel::fileAdded,    this, &DirectoryPresenter::onFileAdded);
    disconnect(model.get(), &DirectoryModel::fileRenamed,  this, &DirectoryPresenter::onFileRenamed);
    disconnect(model.get(), &DirectoryModel::fileModified, this, &DirectoryPresenter::onFileModified);
    disconnect(model.get(), &DirectoryModel::dirRemoved,   this, &DirectoryPresenter::onDirRemoved);
    disconnect(model.get(), &DirectoryModel::dirAdded,     this, &DirectoryPresenter::onDirAdded);
    disconnect(model.get(), &DirectoryModel::dirRenamed,   this, &DirectoryPresenter::onDirRenamed);
    model = nullptr;
    // also empty view?
}

void DirectoryPresenter::setView(std::shared_ptr<IDirectoryView> _view) {
    if(view)
        return;
    view = _view;
    if(model)
        view->populate(mShowDirs ? model->totalCount() : model->fileCount());
    connect(dynamic_cast<QObject *>(view.get()), SIGNAL(itemActivated(int)),
            this, SLOT(onItemActivated(int)));
    connect(dynamic_cast<QObject *>(view.get()), SIGNAL(thumbnailsRequested(QList<int>, int, bool, bool)),
            this, SLOT(generateThumbnails(QList<int>, int, bool, bool)));
    connect(dynamic_cast<QObject *>(view.get()), SIGNAL(draggedOut()),
            this, SLOT(onDraggedOut()));
    connect(dynamic_cast<QObject *>(view.get()), SIGNAL(draggedOver(int)),
            this, SLOT(onDraggedOver(int)));
    connect(dynamic_cast<QObject *>(view.get()), SIGNAL(droppedInto(const QMimeData*,QObject*,int)),
            this, SLOT(onDroppedInto(const QMimeData*,QObject*,int)));
}

void DirectoryPresenter::setModel(std::shared_ptr<DirectoryModel> newModel) {
    if(model)
        unsetModel();
    if(!newModel)
        return;
    model = newModel;
    populateView();

    // filesystem changes
    connect(model.get(), &DirectoryModel::fileRemoved,  this, &DirectoryPresenter::onFileRemoved);
    connect(model.get(), &DirectoryModel::fileAdded,    this, &DirectoryPresenter::onFileAdded);
    connect(model.get(), &DirectoryModel::fileRenamed,  this, &DirectoryPresenter::onFileRenamed);
    connect(model.get(), &DirectoryModel::fileModified, this, &DirectoryPresenter::onFileModified);
    connect(model.get(), &DirectoryModel::dirRemoved,   this, &DirectoryPresenter::onDirRemoved);
    connect(model.get(), &DirectoryModel::dirAdded,     this, &DirectoryPresenter::onDirAdded);
    connect(model.get(), &DirectoryModel::dirRenamed,   this, &DirectoryPresenter::onDirRenamed);
}

void DirectoryPresenter::reloadModel() {
    populateView();
}

void DirectoryPresenter::populateView() {
    if(!model || !view)
        return;
    view->populate(mShowDirs ? model->totalCount() : model->fileCount());
    // put a film glyph on video items right away; the real frame takes a while
    const int offset = mShowDirs ? model->dirCount() : 0;
    for(int i = 0; i < model->fileCount(); i++) {
        const QString path = model->filePathAt(i);
        if(isVideoFilePath(path))
            view->setThumbnail(offset + i, videoPlaceholderThumbnail(path));
    }
    selectAndFocus(0);
}

void DirectoryPresenter::disconnectView() {
   // todo
}

//------------------------------------------------------------------------------

void DirectoryPresenter::onFileRemoved(QString filePath, int index) {
    Q_UNUSED(filePath)
    if(!view)
        return;
    view->removeItem(mShowDirs ? index + model->dirCount() : index);
}

void DirectoryPresenter::onFileRenamed(QString fromPath, int indexFrom, QString toPath, int indexTo) {
    Q_UNUSED(fromPath)
    Q_UNUSED(toPath)
    if(!view)
        return;
    if(mShowDirs) {
        indexFrom += model->dirCount();
        indexTo += model->dirCount();
    }
    auto oldSelection = view->selection();
    view->removeItem(indexFrom);
    view->insertItem(indexTo);
    // re-select if needed
    if(oldSelection.contains(indexFrom)) {
        if(oldSelection.count() == 1) {
            view->select(indexTo);
            view->focusOn(indexTo);
        } else if(oldSelection.count() > 1) {
            view->select(view->selection() << indexTo);
        }
    }
}

void DirectoryPresenter::onFileAdded(QString filePath) {
    if(!view)
        return;
    int index = model->indexOfFile(filePath);
    const int pos = mShowDirs ? model->dirCount() + index : index;
    view->insertItem(pos);
    if(isVideoFilePath(filePath))
        view->setThumbnail(pos, videoPlaceholderThumbnail(filePath));
}

void DirectoryPresenter::onFileModified(QString filePath) {
    if(!view)
        return;
    int index = model->indexOfFile(filePath);
    view->reloadItem(mShowDirs ? model->dirCount() + index : index);
}

void DirectoryPresenter::onDirRemoved(QString dirPath, int index) {
    Q_UNUSED(dirPath)
    if(!view || !mShowDirs)
        return;
    view->removeItem(index);
}

void DirectoryPresenter::onDirRenamed(QString fromPath, int indexFrom, QString toPath, int indexTo) {
    Q_UNUSED(fromPath)
    Q_UNUSED(toPath)
    if(!view || !mShowDirs)
        return;
    auto oldSelection = view->selection();
    view->removeItem(indexFrom);
    view->insertItem(indexTo);
    // re-select if needed
    if(oldSelection.contains(indexFrom)) {
        if(oldSelection.count() == 1) {
            view->select(indexTo);
            view->focusOn(indexTo);
        } else if(oldSelection.count() > 1) {
            view->select(view->selection() << indexTo);
        }
    }
}

void DirectoryPresenter::onDirAdded(QString dirPath) {
    if(!view || !mShowDirs)
        return;
    int index = model->indexOfDir(dirPath);
    view->insertItem(index);
}

bool DirectoryPresenter::showDirs() {
    return mShowDirs;
}

void DirectoryPresenter::setShowDirs(bool mode) {
    if(mode == mShowDirs)
        return;
    mShowDirs = mode;
    populateView();
}

QList<QString> DirectoryPresenter::selectedPaths() const {
    QList<QString> paths;
    if(!view)
        return paths;
    if(mShowDirs) {
        for(auto i : view->selection()) {
            if(i < model->dirCount())
                paths << model->dirPathAt(i);
            else
                paths << model->filePathAt(i - model->dirCount());
        }
    } else {
        for(auto i : view->selection()) {
            paths << model->filePathAt(i);
        }
    }
    return paths;
}

void DirectoryPresenter::generateThumbnails(QList<int> indexes, int size, bool crop, bool force) {
    if(!view || !model)
        return;
    thumbnailer.clearTasks();
    if(!mShowDirs) {
        for(int i : indexes)
            thumbnailer.getThumbnailAsync(model->filePathAt(i), size, crop, force);
        return;
    }
    for(int i : indexes) {
        if(i < model->dirCount()) {
            // Every directory row shows the same icon, so it is rendered once per
            // size and colour and then shared: the SVG parse plus the recolor for
            // each row added up to most of the time a folder-heavy listing took.
            const QColor iconColor = settings->colorScheme().icons;
            const QString cacheKey = QString::number(size) + "|" + iconColor.name(QColor::HexArgb);
            std::shared_ptr<QPixmap> pixmap = mDirIconCache.value(cacheKey);
            if(!pixmap) {
                QSvgRenderer svgRenderer;
                svgRenderer.load(QString(":/res/icons/common/other/folder32-scalable.svg"));
                int factor = (size * 0.90f) / svgRenderer.defaultSize().width();
                pixmap = std::make_shared<QPixmap>(svgRenderer.defaultSize() * factor);
                pixmap->fill(Qt::transparent);
                QPainter pixPainter(pixmap.get());
                svgRenderer.render(&pixPainter);
                pixPainter.end();
                ImageLib::recolor(*pixmap, iconColor);
                // a handful of sizes is all this ever holds
                if(mDirIconCache.size() > 8)
                    mDirIconCache.clear();
                mDirIconCache.insert(cacheKey, pixmap);
            }

            std::shared_ptr<Thumbnail> thumb(new Thumbnail(model->dirNameAt(i),
                                                           "Folder",
                                                           size,
                                                           pixmap));
            view->setThumbnail(i, thumb);
        } else {
            QString path = model->filePathAt(i - model->dirCount());
            thumbnailer.getThumbnailAsync(path, size, crop, force);
        }
    }
}

void DirectoryPresenter::onThumbnailReady(std::shared_ptr<Thumbnail> thumb, QString filePath) {
    if(!view || !model)
        return;
    int index = model->indexOfFile(filePath);
    if(index == -1)
        return;
    view->setThumbnail(mShowDirs ? model->dirCount() + index : index, thumb);
}

void DirectoryPresenter::onItemActivated(int absoluteIndex) {
    if(!model)
        return;
    if(!mShowDirs) {
        emit fileActivated(model->filePathAt(absoluteIndex));
        return;
    }
    if(absoluteIndex < model->dirCount())
        emit dirActivated(model->dirPathAt(absoluteIndex));
    else
        emit fileActivated(model->filePathAt(absoluteIndex - model->dirCount()));
}

void DirectoryPresenter::onDraggedOut() {
    emit draggedOut(selectedPaths());
}

void DirectoryPresenter::onDraggedOver(int index) {
    if(!model || view->selection().contains(index))
        return;
    if(showDirs() && index < model->dirCount())
        view->setDragHover(index);

}

void DirectoryPresenter::onDroppedInto(const QMimeData *data, QObject *source, int targetIndex) {
    if(!data->hasUrls() || model->source() != SOURCE_DIRECTORY)
        return;

    // ignore drops into selected / current folder when we are the source of dropEvent
    if(source && (view->selection().contains(targetIndex) || targetIndex == -1) )
        return;
    // ignore drops into a file
    // todo: drop into a current dir when target is a file
    if(showDirs() && targetIndex >= model->dirCount())
        return;

    // convert urls to qstrings
    QStringList pathList;
    QList<QUrl> urlList = data->urls();
    for(int i = 0; i < urlList.size(); ++i)
        pathList.append(urlList.at(i).toLocalFile());

    // get target dir path
    QString destDir;
    if(showDirs() && targetIndex < model->dirCount())
       destDir = model->dirPathAt(targetIndex);
    if(destDir.isEmpty()) // fallback to the current dir
        destDir = model->directoryPath();
    pathList.removeAll(destDir); // remove target dir from source list

    // pass to core
    emit droppedInto(pathList, destDir);
}

void DirectoryPresenter::selectAndFocus(QString path) {
    if(!model || !view || path.isEmpty())
        return;
    if(model->containsDir(path) && showDirs()) {
        int dirIndex = model->indexOfDir(path);
        view->select(dirIndex);
        view->focusOn(dirIndex);
    } else if(model->containsFile(path)) {
        int fileIndex = showDirs() ? model->indexOfFile(path) + model->dirCount() : model->indexOfFile(path);
        view->select(fileIndex);
        view->focusOn(fileIndex);
    }
}

void DirectoryPresenter::selectAndFocus(int absoluteIndex) {
    if(!model || !view)
        return;
    view->select(absoluteIndex);
    view->focusOn(absoluteIndex);
}
