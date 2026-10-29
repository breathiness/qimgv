#pragma once

#include <QLabel>

// QLabel that elides its text in the middle instead of hard-clipping it, so a
// long path keeps both its root and its last directory readable.
//
// Only the painted string is shortened: text() and the size hint stay based on
// the whole text, so the widget still takes the width it would without the
// elision and the surrounding layout does not shift as the text gets shorter.
class ElidedLabel : public QLabel {
public:
    explicit ElidedLabel(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};
