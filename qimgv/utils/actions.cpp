#include "actions.h"

#include <QCoreApplication>
#include <QHash>

Actions *appActions = nullptr;

Actions::Actions() {
    init();
}

Actions *Actions::getInstance() {
    if(!appActions)
        appActions  = new Actions();
    return appActions;
}

const QMap<QString, QVersionNumber> &Actions::getMap() {
    return mActions;
}

QList<QString> Actions::getList() {
    return mActions.keys();
}

void Actions::init() {
    mActions.insert("nextImage", QVersionNumber(0,6,2));
    mActions.insert("prevImage", QVersionNumber(0,6,2));
    mActions.insert("toggleFullscreen", QVersionNumber(0,6,2));
    mActions.insert("fitWindow", QVersionNumber(0,6,2));
    mActions.insert("fitWidth", QVersionNumber(0,6,2));
    mActions.insert("fitNormal", QVersionNumber(0,6,2));
    mActions.insert("fitWindowStretch", QVersionNumber(1,0,3));
    mActions.insert("toggleFitMode", QVersionNumber(0,6,2));
    mActions.insert("resize", QVersionNumber(0,6,2));
    mActions.insert("rotateRight", QVersionNumber(0,6,2));
    mActions.insert("rotateLeft", QVersionNumber(0,6,2));
    mActions.insert("scrollUp", QVersionNumber(0,6,2));
    mActions.insert("scrollDown", QVersionNumber(0,6,2));
    mActions.insert("scrollLeft", QVersionNumber(0,6,2));
    mActions.insert("scrollRight", QVersionNumber(0,6,2));
    mActions.insert("zoomIn", QVersionNumber(0,6,2));
    mActions.insert("zoomOut", QVersionNumber(0,6,2));
    mActions.insert("zoomInCursor", QVersionNumber(0,6,2));
    mActions.insert("zoomOutCursor", QVersionNumber(0,6,2));
    mActions.insert("open", QVersionNumber(0,6,2));
    mActions.insert("save", QVersionNumber(0,6,2));
    mActions.insert("saveAs", QVersionNumber(0,6,2));
    mActions.insert("setWallpaper", QVersionNumber(0,9,3));
    mActions.insert("crop", QVersionNumber(0,6,2));
    mActions.insert("removeFile", QVersionNumber(0,6,2));
    mActions.insert("copyFile", QVersionNumber(0,6,2));
    mActions.insert("moveFile", QVersionNumber(0,6,2));
    mActions.insert("jumpToFirst", QVersionNumber(0,6,2));
    mActions.insert("jumpToLast", QVersionNumber(0,6,2));
    mActions.insert("openSettings", QVersionNumber(0,6,2));
    mActions.insert("closeFullScreenOrExit", QVersionNumber(0,6,2));
    mActions.insert("exit", QVersionNumber(0,6,2));
    mActions.insert("flipH", QVersionNumber(0,6,3));
    mActions.insert("flipV", QVersionNumber(0,6,3));
    mActions.insert("folderView", QVersionNumber(0,6,85));
    mActions.insert("pauseVideo", QVersionNumber(0,6,85));
    mActions.insert("frameStep", QVersionNumber(0,6,85));
    mActions.insert("frameStepBack", QVersionNumber(0,6,85));
    mActions.insert("folderView", QVersionNumber(0,6,85));
    mActions.insert("documentView", QVersionNumber(0,6,88));
    mActions.insert("toggleFolderView", QVersionNumber(0,6,88));
    mActions.insert("moveToTrash", QVersionNumber(0,6,89));
    mActions.insert("reloadImage", QVersionNumber(0,7,80));
    mActions.insert("copyFileClipboard", QVersionNumber(0,7,80));
    mActions.insert("copyPathClipboard", QVersionNumber(0,7,80));
    mActions.insert("renameFile", QVersionNumber(0,7,80));
    mActions.insert("contextMenu", QVersionNumber(0,7,81));
    mActions.insert("copySelection", QVersionNumber(1,0,4));
    mActions.insert("cutSelection", QVersionNumber(1,0,4));
    mActions.insert("pasteSelection", QVersionNumber(1,0,4));
    mActions.insert("copyImage", QVersionNumber(1,0,5));
    mActions.insert("pdfNextPage", QVersionNumber(1,0,4));
    mActions.insert("pdfPrevPage", QVersionNumber(1,0,4));
    mActions.insert("toggleTransparencyGrid", QVersionNumber(0,7,82));
    mActions.insert("sortByName", QVersionNumber(0,7,83));
    mActions.insert("sortByTime", QVersionNumber(0,7,83));
    mActions.insert("sortBySize", QVersionNumber(0,7,83));
    mActions.insert("toggleImageInfo", QVersionNumber(0,7,84));
    mActions.insert("toggleShuffle", QVersionNumber(0,8,3));
    mActions.insert("toggleScalingFilter", QVersionNumber(0,8,3));
    mActions.insert("toggleMute", QVersionNumber(0,8,7));
    mActions.insert("volumeUp", QVersionNumber(0,8,7));
    mActions.insert("volumeDown", QVersionNumber(0,8,7));
    mActions.insert("toggleSlideshow", QVersionNumber(0,8,81));
    mActions.insert("showInDirectory", QVersionNumber(0,8,82));
    mActions.insert("goUp", QVersionNumber(0,9,2));
    mActions.insert("discardEdits", QVersionNumber(0,9,2));
    mActions.insert("nextDirectory", QVersionNumber(0,9,2));
    mActions.insert("prevDirectory", QVersionNumber(0,9,2));
    mActions.insert("seekVideoForward", QVersionNumber(0,9,2));
    mActions.insert("seekVideoBackward", QVersionNumber(0,9,2));
    mActions.insert("lockZoom", QVersionNumber(0,9,2));
    mActions.insert("lockView", QVersionNumber(0,9,2));
    mActions.insert("print", QVersionNumber(1,0,0));
    mActions.insert("toggleFullscreenInfoBar", QVersionNumber(1,0,0));
    mActions.insert("pasteFile", QVersionNumber(1,0,3));
}

QString Actions::displayName(const QString &action) const {
    // shown in the shortcut pages; the ids are what the rest of the program uses
    static const QHash<QString, const char *> names = {
        {"closeFullScreenOrExit", QT_TR_NOOP("Close fullscreen or exit")},
        {"contextMenu", QT_TR_NOOP("Context menu")},
        {"copyFile", QT_TR_NOOP("Copy file")},
        {"copyFileClipboard", QT_TR_NOOP("Copy image to clipboard")},
        {"copyPathClipboard", QT_TR_NOOP("Copy path")},
        {"copySelection", QT_TR_NOOP("Copy selection")},
        {"crop", QT_TR_NOOP("Crop")},
        {"cutSelection", QT_TR_NOOP("Cut selection")},
        {"discardEdits", QT_TR_NOOP("Discard edits")},
        {"documentView", QT_TR_NOOP("Document view")},
        {"exit", QT_TR_NOOP("Exit")},
        {"fitNormal", QT_TR_NOOP("Original size")},
        {"fitWidth", QT_TR_NOOP("Fit width")},
        {"fitWindow", QT_TR_NOOP("Fit window")},
        {"fitWindowStretch", QT_TR_NOOP("Stretch to window")},
        {"flipH", QT_TR_NOOP("Flip horizontally")},
        {"flipV", QT_TR_NOOP("Flip vertically")},
        {"folderView", QT_TR_NOOP("Folder view")},
        {"frameStep", QT_TR_NOOP("Next frame")},
        {"frameStepBack", QT_TR_NOOP("Previous frame")},
        {"goUp", QT_TR_NOOP("Go up one folder")},
        {"jumpToFirst", QT_TR_NOOP("Jump to first")},
        {"jumpToLast", QT_TR_NOOP("Jump to last")},
        {"lockView", QT_TR_NOOP("Lock view")},
        {"lockZoom", QT_TR_NOOP("Lock zoom")},
        {"moveFile", QT_TR_NOOP("Move file")},
        {"moveToTrash", QT_TR_NOOP("Move to trash")},
        {"nextDirectory", QT_TR_NOOP("Next folder")},
        {"nextImage", QT_TR_NOOP("Next image")},
        {"open", QT_TR_NOOP("Open")},
        {"openSettings", QT_TR_NOOP("Open settings")},
        {"pasteFile", QT_TR_NOOP("Open from clipboard")},
        {"pasteSelection", QT_TR_NOOP("Paste selection")},
        {"copyImage", QT_TR_NOOP("Copy image")},
        {"pdfNextPage", QT_TR_NOOP("Next PDF page")},
        {"pdfPrevPage", QT_TR_NOOP("Previous PDF page")},
        {"pauseVideo", QT_TR_NOOP("Pause video")},
        {"prevDirectory", QT_TR_NOOP("Previous folder")},
        {"prevImage", QT_TR_NOOP("Previous image")},
        {"print", QT_TR_NOOP("Print")},
        {"reloadImage", QT_TR_NOOP("Reload image")},
        {"removeFile", QT_TR_NOOP("Delete permanently")},
        {"renameFile", QT_TR_NOOP("Rename")},
        {"resize", QT_TR_NOOP("Resize")},
        {"rotateLeft", QT_TR_NOOP("Rotate left")},
        {"rotateRight", QT_TR_NOOP("Rotate right")},
        {"save", QT_TR_NOOP("Save")},
        {"saveAs", QT_TR_NOOP("Save as")},
        {"scrollDown", QT_TR_NOOP("Scroll down")},
        {"scrollLeft", QT_TR_NOOP("Scroll left")},
        {"scrollRight", QT_TR_NOOP("Scroll right")},
        {"scrollUp", QT_TR_NOOP("Scroll up")},
        {"seekVideoBackward", QT_TR_NOOP("Seek video backward")},
        {"seekVideoForward", QT_TR_NOOP("Seek video forward")},
        {"setWallpaper", QT_TR_NOOP("Set as wallpaper")},
        {"showInDirectory", QT_TR_NOOP("Show in file manager")},
        {"sortByName", QT_TR_NOOP("Sort by name")},
        {"sortBySize", QT_TR_NOOP("Sort by size")},
        {"sortByTime", QT_TR_NOOP("Sort by date")},
        {"toggleFitMode", QT_TR_NOOP("Toggle fit mode")},
        {"toggleFolderView", QT_TR_NOOP("Toggle folder view")},
        {"toggleFullscreen", QT_TR_NOOP("Toggle fullscreen")},
        {"toggleFullscreenInfoBar", QT_TR_NOOP("Toggle fullscreen info bar")},
        {"toggleImageInfo", QT_TR_NOOP("Toggle image info")},
        {"toggleMute", QT_TR_NOOP("Toggle mute")},
        {"toggleScalingFilter", QT_TR_NOOP("Toggle scaling filter")},
        {"toggleShuffle", QT_TR_NOOP("Toggle shuffle")},
        {"toggleSlideshow", QT_TR_NOOP("Toggle slideshow")},
        {"toggleTransparencyGrid", QT_TR_NOOP("Toggle transparency grid")},
        {"volumeDown", QT_TR_NOOP("Volume down")},
        {"volumeUp", QT_TR_NOOP("Volume up")},
        {"zoomIn", QT_TR_NOOP("Zoom in")},
        {"zoomInCursor", QT_TR_NOOP("Zoom in at cursor")},
        {"zoomOut", QT_TR_NOOP("Zoom out")},
        {"zoomOutCursor", QT_TR_NOOP("Zoom out at cursor")},
    };
    const auto it = names.constFind(action);
    if(it == names.constEnd())
        return action;
    return QCoreApplication::translate("Actions", it.value());
}
