#pragma once

#include <QPushButton>
#include "shortcutbuilder.h"
#include "utils/actions.h"

class KeySequenceEdit : public QPushButton {
    Q_OBJECT
public:
    KeySequenceEdit(QWidget *parent);
    QString sequence();
    void setText(const QString &text);

protected:
    void keyPressEvent(QKeyEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void focusOutEvent(QFocusEvent *e) override;
    bool focusNextPrevChild(bool) override;
private:
    // Arming is a separate step from capturing. A click used to be recorded as
    // the shortcut ("LMB") because the press was fed to ShortcutBuilder, and the
    // same override skipping QPushButton's handler also skipped the focus it
    // gives, so the keys pressed afterwards never reached this widget.
    void startListening();
    void cancelListening();
    void commitSequence(const QString &sequence);
    void updateDisplay();
    void processEvent(QEvent *e);

    QString mSequence;
    QString mSequenceBeforeListening;
    bool mListening = false;
    bool mSwallowNextRelease = false;

signals:
    void edited();
};
