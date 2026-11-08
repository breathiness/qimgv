#include "windowresizegrip.h"

#include <QMouseEvent>
#include <QWindow>

WindowResizeGrip::WindowResizeGrip(Qt::Edges edges, QWidget *parent)
    : QWidget(parent), mEdges(edges) {
    setAttribute(Qt::WA_NoMousePropagation, true);
    applyCursor();
}

void WindowResizeGrip::setEdges(Qt::Edges edges) {
    mEdges = edges;
    applyCursor();
}

void WindowResizeGrip::applyCursor() {
    const bool left = mEdges.testFlag(Qt::LeftEdge);
    const bool right = mEdges.testFlag(Qt::RightEdge);
    const bool top = mEdges.testFlag(Qt::TopEdge);
    const bool bottom = mEdges.testFlag(Qt::BottomEdge);
    if((left && top) || (right && bottom))
        setCursor(Qt::SizeFDiagCursor);
    else if((right && top) || (left && bottom))
        setCursor(Qt::SizeBDiagCursor);
    else if(left || right)
        setCursor(Qt::SizeHorCursor);
    else if(top || bottom)
        setCursor(Qt::SizeVerCursor);
}

void WindowResizeGrip::enterEvent(QEnterEvent *event) {
    applyCursor();
    QWidget::enterEvent(event);
}

void WindowResizeGrip::mousePressEvent(QMouseEvent *event) {
    if(event->button() == Qt::LeftButton) {
        if(QWindow *handle = window()->windowHandle()) {
            handle->startSystemResize(mEdges);
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}
