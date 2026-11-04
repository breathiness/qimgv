#include "sectioninfoitem.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QStyleOption>

SectionInfoItem::SectionInfoItem(QWidget *parent) : QWidget(parent) {
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(9, 0, 9, 0);
    layout->setSpacing(0);
    layout->addWidget(&titleLabel);
    setLayout(layout);

    titleLabel.setFixedHeight(24);
    QFont font = titleLabel.font();
    font.setBold(true);
    titleLabel.setFont(font);
    titleLabel.setContentsMargins(0, 6, 0, 0);
}

void SectionInfoItem::setTitle(const QString &title) {
    titleLabel.setText(title);
}

void SectionInfoItem::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
