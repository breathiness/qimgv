#pragma once

#include <QFrame>

// A container that draws itself with the style's group-box primitive. Qlementine
// implements PE_FrameGroupBox (background, border and radius from the theme), but
// a plain QWidget never asks for it, so groups would stay flat. The stylesheet
// look is unchanged: classic mode falls through to the base painting.
class GroupPanel : public QFrame {
public:
    explicit GroupPanel(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};
