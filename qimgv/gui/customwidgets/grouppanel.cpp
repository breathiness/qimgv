#include "grouppanel.h"
#include "uistyle.h"

#include <QPainter>

GroupPanel::GroupPanel(QWidget *parent) : QFrame(parent) {
    setFrameShape(QFrame::NoFrame);
}

void GroupPanel::paintEvent(QPaintEvent *event) {
    UiStyle::ChromePalette pal;
    if(!UiStyle::chromePalette(pal)) {
        // classic: the stylesheet paints [accessibleName="SGroup"]
        QWidget::paintEvent(event);
        return;
    }
    // The card Qlementine draws for a QGroupBox, in the same colours and with the
    // same radius, drawn here directly: these containers are not QGroupBoxes
    // (they carry no title), so no style primitive would be asked for them.
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setBrush(pal.groupFill);
    p.setPen(QPen(pal.groupLine, 1.0));
    p.drawRoundedRect(r, pal.radius, pal.radius);
}
