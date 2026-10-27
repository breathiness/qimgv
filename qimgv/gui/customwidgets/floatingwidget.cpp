#include "floatingwidget.h"
#include "uistyle.h"

FloatingWidget::FloatingWidget(FloatingWidgetContainer *parent) : QWidget(parent), mAcceptKeyboardFocus(false) {
    this->setAccessibleName("OverlayWidget");
    connect(parent, &FloatingWidgetContainer::resized, this, &FloatingWidget::onContainerResized);
    hide();
}

QSize FloatingWidget::containerSize() {
    return container;
}

void FloatingWidget::setContainerSize(QSize container) {
    this->container = container;
    recalculateGeometry();
}

void FloatingWidget::onContainerResized(QSize size) {
    setContainerSize(size);
}

void FloatingWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    // Qlementine mode has no stylesheet, so the card is drawn from the palette.
    // Subclasses that paint their own content override this anyway.
    QPainter p(this);
    if(UiStyle::paintBackground(this, p, true))
        return;
    QStyleOption opt;
    opt.initFrom(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

bool FloatingWidget::acceptKeyboardFocus() const {
    return mAcceptKeyboardFocus;

}
void FloatingWidget::setAcceptKeyboardFocus(bool mode) {
    mAcceptKeyboardFocus = mode;
}

void FloatingWidget::recalculateGeometry() {
}

void FloatingWidget::mousePressEvent(QMouseEvent *event) {
    event->accept();
}

void FloatingWidget::mouseReleaseEvent(QMouseEvent *event) {
    event->accept();
}

void FloatingWidget::wheelEvent(QWheelEvent *event) {
    event->accept();
}

void FloatingWidget::hide() {
    QWidget::hide();
    if(this->hasFocus() || isAncestorOf(qApp->focusWidget()))
        parentWidget()->setFocus();
}
