#include "imageinfooverlay.h"
#include "ui_imageinfooverlay.h"

ImageInfoOverlay::ImageInfoOverlay(FloatingWidgetContainer *parent) :
    OverlayWidget(parent),
    ui(new Ui::ImageInfoOverlay)
{
    ui->setupUi(this);
    ui->closeButton->setIconPath(":res/icons/common/overlay/close-dim16.png");
    ui->headerIcon->setIconPath(":res/icons/common/overlay/info16.png");
    entryStub.setFixedSize(330, 48);
    entryStub.setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    connect(ui->closeButton,  &IconButton::clicked, this, &ImageInfoOverlay::hide);
    this->setPosition(FloatingWidgetPosition::RIGHT);

    if(parent)
        setContainerSize(parent->size());
}

ImageInfoOverlay::~ImageInfoOverlay() {
    clearItems();
    delete ui;
}

void ImageInfoOverlay::clearItems() {
    for(int i = items.count() - 1; i >= 0; i--) {
        QWidget *item = items.takeAt(i);
        ui->entryLayout->removeWidget(item);
        item->deleteLater();
        // the layout keeps them alive until the event loop runs; hide right away so
        // the panel does not show both the old and the new list for a moment
        item->hide();
    }
}

void ImageInfoOverlay::setInfo(const QList<ImageInfoSection> &info) {
    const bool wasVisible = !isHidden();
    clearItems();

    for(const ImageInfoSection &section : info) {
        if(section.entries.isEmpty())
            continue;
        if(!section.title.isEmpty()) {
            auto *heading = new SectionInfoItem(this);
            heading->setTitle(section.title);
            ui->entryLayout->addWidget(heading);
            items.append(heading);
        }
        for(const ImageInfoEntry &entry : section.entries) {
            auto *item = new EntryInfoItem(this);
            item->setInfo(entry.label, entry.value);
            ui->entryLayout->addWidget(item);
            items.append(item);
        }
    }

    // Hiding/showing entryStub causes flicker,
    // so we just remove it from layout and clear the text.
    // It's still there but basically not visible
    if(!items.isEmpty()) {
        ui->entryLayout->removeWidget(&entryStub);
        entryStub.setText("");
    } else {
        ui->entryLayout->addWidget(&entryStub);
        entryStub.setText(tr("<no metadata found>"));
    }

    if(wasVisible) {
        // wait for layout change
        qApp->processEvents();
        // reposition
        recalculateGeometry();
    }
}

void ImageInfoOverlay::show() {
    OverlayWidget::show();
    adjustSize();
    recalculateGeometry();
}

void ImageInfoOverlay::wheelEvent(QWheelEvent *event) {
    event->accept();
}
