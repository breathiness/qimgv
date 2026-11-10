#include "fileassocsettingspage.h"
#include "fileassociations.h"
#include "settings.h"

#include <QImageReader>

#include <QCheckBox>
#include <QHBoxLayout>
#include <QSet>
#include <QHash>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QFont>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace {

// Chinese names for the labels that the generated table carries in English.
QString localizedLabel(const QString &label) {
    static const QHash<QString, QString> names = {
        {QStringLiteral("JPEG image"), QStringLiteral("JPEG 图像")},
        {QStringLiteral("PNG image"), QStringLiteral("PNG 图像")},
        {QStringLiteral("GIF image"), QStringLiteral("GIF 图像")},
        {QStringLiteral("WebP image"), QStringLiteral("WebP 图像")},
        {QStringLiteral("BMP image"), QStringLiteral("BMP 图像")},
        {QStringLiteral("TIFF image"), QStringLiteral("TIFF 图像")},
        {QStringLiteral("Icon"), QStringLiteral("图标文件")},
        {QStringLiteral("SVG image"), QStringLiteral("SVG 矢量图")},
        {QStringLiteral("AVIF image"), QStringLiteral("AVIF 图像")},
        {QStringLiteral("HEIF image"), QStringLiteral("HEIF 图像")},
        {QStringLiteral("JPEG XL image"), QStringLiteral("JPEG XL 图像")},
        {QStringLiteral("Camera raw image"), QStringLiteral("相机 RAW")},
        {QStringLiteral("OpenEXR image"), QStringLiteral("OpenEXR 图像")},
        {QStringLiteral("HDR image"), QStringLiteral("HDR 图像")},
        {QStringLiteral("DirectDraw surface"), QStringLiteral("DDS 纹理")},
        {QStringLiteral("FITS image"), QStringLiteral("FITS 图像")},
        {QStringLiteral("Portable float map"), QStringLiteral("PFM 浮点图")},
        {QStringLiteral("QOI image"), QStringLiteral("QOI 图像")},
        {QStringLiteral("Photoshop document"), QStringLiteral("Photoshop 文档")},
        {QStringLiteral("Krita document"), QStringLiteral("Krita 文档")},
        {QStringLiteral("OpenRaster image"), QStringLiteral("OpenRaster 图像")},
        {QStringLiteral("Illustrator document"), QStringLiteral("Illustrator 文档")},
        {QStringLiteral("Comic archive"), QStringLiteral("漫画压缩包")},
        {QStringLiteral("PDF document"), QStringLiteral("PDF 文档")},
        {QStringLiteral("DXF drawing"), QStringLiteral("DXF 图纸")},
        {QStringLiteral("DWG drawing"), QStringLiteral("DWG 图纸")},
        {QStringLiteral("3DS model"), QStringLiteral("3DS 模型")},
        {QStringLiteral("3MF model"), QStringLiteral("3MF 模型")},
        {QStringLiteral("OBJ model"), QStringLiteral("OBJ 模型")},
        {QStringLiteral("PLY model"), QStringLiteral("PLY 模型")},
        {QStringLiteral("STL model"), QStringLiteral("STL 模型")},
        {QStringLiteral("glTF model"), QStringLiteral("glTF 模型")},
        {QStringLiteral("FBX model"), QStringLiteral("FBX 模型")},
        {QStringLiteral("COLLADA model"), QStringLiteral("COLLADA 模型")},
        {QStringLiteral("AVI video"), QStringLiteral("AVI 视频")},
        {QStringLiteral("MP4 video"), QStringLiteral("MP4 视频")},
        {QStringLiteral("Matroska video"), QStringLiteral("Matroska 视频")},
        {QStringLiteral("WebM video"), QStringLiteral("WebM 视频")},
        {QStringLiteral("QuickTime video"), QStringLiteral("QuickTime 视频")},
        {QStringLiteral("Flash video"), QStringLiteral("Flash 视频")},
        {QStringLiteral("Transport stream"), QStringLiteral("传输流")},
        {QStringLiteral("Blu-ray stream"), QStringLiteral("蓝光流")},
    };
    return names.value(label, label);
}

// The class context, so the category names land in the translation file next
// to the rest of the page instead of in QObject's.
QString categoryName(int category) {
    switch(category) {
    case FileAssociations::CategoryImages:    return FileAssocSettingsPage::tr("Images");
    case FileAssociations::CategoryVideo:     return FileAssocSettingsPage::tr("Video");
    case FileAssociations::CategoryDocuments: return FileAssocSettingsPage::tr("Documents");
    case FileAssociations::CategoryModels:    return FileAssocSettingsPage::tr("3D models");
    default:                                  return QString();
    }
}

} // namespace

FileAssocSettingsPage::FileAssocSettingsPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *intro = new QLabel(tr("Register the formats qimgv can open so Explorer "
                                "shows the icon of each format and offers qimgv in "
                                "\"Open with\". The default application for an "
                                "extension is not changed - pick qimgv once in "
                                "Windows' \"Default apps\"."), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    machineWideBox = new QCheckBox(tr("Register for all users (needs administrator rights)"), this);
    layout->addWidget(machineWideBox);

    warning = new QLabel(tr("Writing machine-wide associations requires running qimgv "
                            "as administrator; without it the change will fail."), this);
    warning->setWordWrap(true);
    warning->setVisible(false);
    layout->addWidget(warning);

    if(!FileAssociations::isSupported())
        machineWideBox->setEnabled(false);

    tree = new QTreeWidget(this);
    tree->setColumnCount(2);
    tree->setHeaderLabels({tr("Format"), tr("Type")});
    tree->setRootIsDecorated(true);
    tree->setUniformRowHeights(true);
    tree->setSelectionMode(QAbstractItemView::NoSelection);
    tree->setAlternatingRowColors(true);
    layout->addWidget(tree, 1);

    auto *buttons = new QHBoxLayout;
    auto *all = new QPushButton(tr("Select all"), this);
    auto *defaults = new QPushButton(tr("Restore defaults"), this);
    buttons->addWidget(all);
    buttons->addWidget(defaults);
    buttons->addStretch();
    status = new QLabel(this);
    buttons->addWidget(status);
    layout->addLayout(buttons);

    connect(tree, &QTreeWidget::itemChanged, this, &FileAssocSettingsPage::onItemChanged);
    connect(machineWideBox, &QCheckBox::toggled, this, &FileAssocSettingsPage::onMachineWideToggled);
    connect(all, &QPushButton::clicked, this, &FileAssocSettingsPage::selectAll);
    connect(defaults, &QPushButton::clicked, this, &FileAssocSettingsPage::restoreDefaults);

    readSettings();
}

void FileAssocSettingsPage::readSettings() {
    machineWideBox->blockSignals(true);
    machineWideBox->setChecked(false);
    machineWideBox->blockSignals(false);
    warning->setVisible(false);
    status->clear();

    const QStringList registered = FileAssociations::registered(false);
    // Nothing registered yet (first visit): show the defaults instead of an
    // empty tree, so "Apply" right away does the sensible thing.
    rebuildTree(registered, true);
}

void FileAssocSettingsPage::saveSettings() {
    if(!FileAssociations::isSupported()) {
        status->setText(tr("File associations are only supported on Windows."));
        return;
    }

    const bool machineWide = machineWideBox->isChecked();
    const QStringList want = checkedExtensions();
    const QStringList have = FileAssociations::registered(machineWide);

    QStringList enable;
    for(const QString &ext : want) {
        if(!have.contains(ext))
            enable.append(ext);
    }
    QStringList disable;
    for(const QString &ext : have) {
        if(!want.contains(ext))
            disable.append(ext);
    }

    if(enable.isEmpty() && disable.isEmpty()) {
        status->setText(tr("Nothing to change."));
        return;
    }

    if(!FileAssociations::apply(enable, disable, machineWide)) {
        status->setText(machineWide
                        ? tr("Failed - run qimgv as administrator for machine-wide associations.")
                        : tr("Failed to write the registry."));
        return;
    }

    FileAssociations::notifyShell();
    status->setText(tr("Applied: %1 registered, %2 removed.")
                        .arg(enable.size()).arg(disable.size()));
    // Re-read so the tree reflects what actually landed (e.g. a failed machine
    // wide write leaves the boxes untouched).
    rebuildTree(FileAssociations::registered(machineWide), false);
}

void FileAssocSettingsPage::onMachineWideToggled(bool on) {
    warning->setVisible(on && !FileAssociations::isAdmin());
    // Show what is registered in the hive that would be written.
    rebuildTree(FileAssociations::registered(on), false);
}

void FileAssocSettingsPage::onItemChanged(QTreeWidgetItem *item, int column) {
    if(column != 0)
        return;
    tree->blockSignals(true);
    if(item->childCount() > 0) {
        const Qt::CheckState state = item->checkState(0);
        for(int i = 0; i < item->childCount(); ++i)
            item->child(i)->setCheckState(0, state);
    } else if(QTreeWidgetItem *parent = item->parent()) {
        updateParent(parent);
    }
    tree->blockSignals(false);
}

void FileAssocSettingsPage::updateParent(QTreeWidgetItem *parent) {
    int checked = 0;
    int partial = 0;
    for(int i = 0; i < parent->childCount(); ++i) {
        const Qt::CheckState state = parent->child(i)->checkState(0);
        if(state == Qt::Checked)
            checked++;
        else if(state == Qt::PartiallyChecked)
            partial++;
    }
    if(checked == parent->childCount())
        parent->setCheckState(0, Qt::Checked);
    else if(checked == 0 && partial == 0)
        parent->setCheckState(0, Qt::Unchecked);
    else
        parent->setCheckState(0, Qt::PartiallyChecked);
}

QStringList FileAssocSettingsPage::checkedExtensions() const {
    QStringList result;
    for(int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *parent = tree->topLevelItem(i);
        for(int j = 0; j < parent->childCount(); ++j) {
            QTreeWidgetItem *child = parent->child(j);
            if(child->checkState(0) == Qt::Checked)
                result.append(child->data(0, Qt::UserRole).toString());
        }
    }
    return result;
}

void FileAssocSettingsPage::rebuildTree(const QStringList &checked, bool defaultWhenEmpty) {
    tree->blockSignals(true);
    tree->clear();

    // Only what this installation can actually open: the table in fileformats.h
    // is the full set the code knows about, while the optional packs bring the
    // plugin, assimp and mpv that make some of them work. Registering .dds or
    // .stl from a base install would promise Explorer something qimgv then fails
    // to open.
    QSet<QString> supported;
    for(const QByteArray &format : settings->supportedFormats())
        supported.insert(QString::fromLatin1(format).toLower());
    // settings() keeps pdf/ai listed even when their provider is missing (opening
    // one then explains what to install). An association is different: Explorer
    // would hand every PDF to an installation that cannot read it, so drop them
    // unless the reader actually has the plugin.
    const QList<QByteArray> readerFormats = QImageReader::supportedImageFormats();
    if(!readerFormats.contains("pdf")) {
        supported.remove(QStringLiteral("pdf"));
        supported.remove(QStringLiteral("ai"));
    }

    // Video is compiled in but shipped as a separate plugin, so the setting
    // alone would still advertise formats a base installation cannot play.
    const bool video = FileAssociations::videoAvailable();

    QVector<FileFormatInfo> formats;
    for(const auto &info : FileAssociations::formats()) {
        if(info.category == FileAssociations::CategoryVideo && !video)
            continue;
        if(supported.contains(info.ext.toLower()))
            formats.append(info);
    }
    QList<QTreeWidgetItem *> parents;
    for(int c = 0; c < FileAssociations::CategoryCount; ++c) {
        auto *parent = new QTreeWidgetItem(tree);
        parent->setText(0, categoryName(c));
        parent->setFlags(parent->flags() | Qt::ItemIsUserCheckable);
        parent->setCheckState(0, Qt::Unchecked);
        QFont font = parent->font(0);
        font.setBold(true);
        parent->setFont(0, font);
        parent->setFirstColumnSpanned(true);
        parents.append(parent);
    }

    for(const auto &info : formats) {
        auto *item = new QTreeWidgetItem(parents.at(info.category));
        item->setText(0, info.ext.toUpper());
        item->setText(1, localizedLabel(info.label));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setData(0, Qt::UserRole, info.ext);
        const bool isChecked = checked.isEmpty()
                               ? (defaultWhenEmpty && info.category == FileAssociations::CategoryImages)
                               : checked.contains(info.ext);
        item->setCheckState(0, isChecked ? Qt::Checked : Qt::Unchecked);
    }

    for(QTreeWidgetItem *parent : parents) {
        updateParent(parent);
        parent->setExpanded(true);
        parent->setHidden(parent->childCount() == 0);
    }
    tree->blockSignals(false);
}

void FileAssocSettingsPage::selectAll() {
    tree->blockSignals(true);
    for(int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *parent = tree->topLevelItem(i);
        parent->setCheckState(0, Qt::Checked);
        for(int j = 0; j < parent->childCount(); ++j)
            parent->child(j)->setCheckState(0, Qt::Checked);
    }
    tree->blockSignals(false);
}

void FileAssocSettingsPage::restoreDefaults() {
    tree->blockSignals(true);
    for(int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *parent = tree->topLevelItem(i);
        const bool images = (i == FileAssociations::CategoryImages);
        parent->setCheckState(0, images ? Qt::Checked : Qt::Unchecked);
        for(int j = 0; j < parent->childCount(); ++j)
            parent->child(j)->setCheckState(0, images ? Qt::Checked : Qt::Unchecked);
    }
    tree->blockSignals(false);
}
