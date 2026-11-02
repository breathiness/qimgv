#include "filesystemmodelcustom.h"

#include "uistyle.h"

FileSystemModelCustom::FileSystemModelCustom(QObject *parent) : QFileSystemModel(parent) {
    qreal dpr = qApp->devicePixelRatio();
    const QString iconPath = ":/res/icons/common/menuitem/folder16.png";
    loadFolderIcon(dpr);

    connect(settings, &Settings::settingsChanged, [this, dpr]() {
        loadFolderIcon(dpr);
    });
}

// The folder icon of the tree: the vector icon when the theme has one, the png
// resource otherwise.
void FileSystemModelCustom::loadFolderIcon(qreal dpr) {
    const QString resource = ":/res/icons/common/menuitem/folder16.png";
    const QString themed = UiStyle::themedIconName(resource);
    if(!themed.isEmpty()) {
        const qreal scale = qMax(1.0, dpr);
        QPixmap pm = UiStyle::vectorIcon(themed).pixmap(QSize(16, 16) * scale);
        if(!pm.isNull()) {
            pm.setDevicePixelRatio(scale);
            folderIcon = pm;
            ImageLib::recolor(folderIcon, settings->colorScheme().icons);
            return;
        }
    }
    QString path = resource;
    if(dpr >= (1.0 + 0.001))
        path.replace(".", "@2x.");
    folderIcon.load(path);
    ImageLib::recolor(folderIcon, settings->colorScheme().icons);
}

QVariant FileSystemModelCustom::data( const QModelIndex& index, int role ) const {
    if(role == Qt::DecorationRole)
        return folderIcon;
    return QFileSystemModel::data(index, role);
}

Qt::ItemFlags FileSystemModelCustom::flags(const QModelIndex& index) const {
    if(!index.isValid()) {
        return Qt::NoItemFlags; // 0
        //return Qt::ItemIsDropEnabled;    // Allow drops in the top-level (no parent)
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled;
}
