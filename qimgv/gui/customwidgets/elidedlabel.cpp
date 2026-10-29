#include "elidedlabel.h"
#include "uistyle.h"

#include <QPainter>

ElidedLabel::ElidedLabel(QWidget *parent) : QLabel(parent) {
}

void ElidedLabel::paintEvent(QPaintEvent *event) {
    // Classic mode keeps QLabel's own painting: the stylesheet supplies the text
    // colour and the padding through the style, which this would bypass.
    if(!UiStyle::useQlementine()) {
        QLabel::paintEvent(event);
        return;
    }
    QPainter p(this);
    const QRect r = contentsRect();
    const QString elided = fontMetrics().elidedText(text(), Qt::ElideMiddle, r.width());
    p.setPen(palette().color(QPalette::WindowText));
    p.drawText(r, static_cast<int>(alignment()), elided);
}
