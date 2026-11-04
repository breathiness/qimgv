#include "entryinfoitem.h"

#include <QFontMetrics>

namespace {
const int kNameWidth = 108;
const int kValueWidth = 196;
const int kRowHeight = 30;
}

EntryInfoItem::EntryInfoItem(QWidget *parent) : QWidget(parent) {
    layout.setContentsMargins(9,0,9,0);
    layout.setSpacing(0);
    layout.addWidget(&nameLabel);
    layout.addWidget(&valueLabel);
    setLayout(&layout);

    nameLabel.setFixedSize(kNameWidth,kRowHeight);
    valueLabel.setFixedSize(kValueWidth,kRowHeight);

    // add some padding for easier text selection
    valueLabel.setContentsMargins(3,0,0,0);
    valueLabel.setTextInteractionFlags(Qt::TextSelectableByMouse);
    valueLabel.setCursor(Qt::IBeamCursor);
}

void EntryInfoItem::setInfo(QString _name, QString _value) {
    name = _name;
    value = _value;
    nameLabel.setText(name);
    // A long path or comment does not fit on one row: elide it here and keep the
    // whole text in the tooltip.
    const QFontMetrics metrics(valueLabel.font());
    valueLabel.setText(metrics.elidedText(value, Qt::ElideMiddle, kValueWidth - 6));
    valueLabel.setToolTip(value);
    nameLabel.setToolTip(name);
};

void EntryInfoItem::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
