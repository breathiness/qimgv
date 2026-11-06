#pragma once

#include <QDialog>

class QWidget;

// A QDialog that can paint itself as an MD1 surface: frameless, translucent, a
// 24dp drop shadow in a margin, a 2dp rounded card, and a small title bar the
// window manager can drag. Stock themes keep the native chrome - a dialog that
// loses its title bar for a theme that never asked for it would be a regression.
//
// Usage: inherit, then call enableMaterialChrome() once the .ui is built (the
// layout has to exist before the title bar can be inserted).
class MaterialDialog : public QDialog {
    Q_OBJECT
public:
    explicit MaterialDialog(QWidget *parent = nullptr);

    void enableMaterialChrome();

protected:
    void paintEvent(QPaintEvent *event) override;
    bool event(QEvent *event) override;

private:
    Qt::Edges resizeEdgesAt(const QPoint &pos) const;
    void updateCursor(const QPoint &pos);

    bool mChrome = false;
    QWidget *mTitleBar = nullptr;
};
