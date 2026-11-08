// A thin strip along a window edge or corner that starts a system resize when
// dragged. A frameless window has no non-client border, so the resize targets
// have to be widgets of their own; using QWindow::startSystemResize keeps the
// resize loop (and its cursor shapes) native.

#pragma once

#include <QWidget>

class WindowResizeGrip : public QWidget {
public:
    explicit WindowResizeGrip(Qt::Edges edges, QWidget *parent = nullptr);
    void setEdges(Qt::Edges edges);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;

private:
    void applyCursor();

    Qt::Edges mEdges;
};
