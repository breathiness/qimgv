#include "keysequenceedit.h"

KeySequenceEdit::KeySequenceEdit(QWidget *parent) : QPushButton(parent), mSequence("") {
    QPushButton::setText(tr("[Enter sequence]"));
    setToolTip(tr("Click here, then press the key or mouse button to use"));
    // the focus is set by hand when listening starts, but keep the policy explicit:
    // a keyboard user can tab to this field and arm it with Enter
    setFocusPolicy(Qt::StrongFocus);
}

QString KeySequenceEdit::sequence() {
    return mSequence;
}

void KeySequenceEdit::setText(const QString &text) {
    mSequence = text;
    mSequenceBeforeListening = text;
    updateDisplay();
}

// An empty sequence is shown as the placeholder: a blank button looks broken,
// and this is also what a cancelled edit falls back to.
void KeySequenceEdit::updateDisplay() {
    QPushButton::setText(mSequence.isEmpty() ? tr("[Enter sequence]") : mSequence);
}

void KeySequenceEdit::startListening() {
    if(mListening)
        return;
    mListening = true;
    mSequenceBeforeListening = mSequence;
    // the release that belongs to the click which armed us is not an input
    mSwallowNextRelease = true;
    QPushButton::setText(tr("[Press a key or a mouse button]"));
    setFocus(Qt::OtherFocusReason);
}

void KeySequenceEdit::cancelListening() {
    if(!mListening)
        return;
    mListening = false;
    mSwallowNextRelease = false;
    mSequence = mSequenceBeforeListening;
    updateDisplay();
}

void KeySequenceEdit::commitSequence(const QString &sequence) {
    mListening = false;
    mSwallowNextRelease = false;
    mSequence = sequence;
    updateDisplay();
    emit edited();
}

void KeySequenceEdit::keyPressEvent(QKeyEvent *e) {
    if(!mListening) {
        if(e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter || e->key() == Qt::Key_Space) {
            startListening();
            e->accept();
            return;
        }
        e->ignore();
        return;
    }
    if(e->key() == Qt::Key_Escape) {
        cancelListening();
        e->accept();
        return;
    }
    processEvent(e);
    e->accept();
}

void KeySequenceEdit::mousePressEvent(QMouseEvent *e) {
    if(!mListening) {
        if(e->button() == Qt::LeftButton) {
            startListening();
            e->accept();
            return;
        }
        // a right or middle click only means something as a shortcut, which
        // needs listening mode first
        e->ignore();
        return;
    }
    // a left click while listening is a real answer: "LMB" is a valid shortcut
    processEvent(e);
    e->accept();
}

void KeySequenceEdit::mouseReleaseEvent(QMouseEvent *e) {
    if(mSwallowNextRelease) {
        mSwallowNextRelease = false;
        e->accept();
        return;
    }
    if(mListening) {
        // right button shortcuts are reported on release, not on press
        processEvent(e);
        e->accept();
        return;
    }
    e->ignore();
}

void KeySequenceEdit::wheelEvent(QWheelEvent *e) {
    if(!mListening) {
        // let the settings page scroll instead of eating the wheel
        e->ignore();
        return;
    }
    processEvent(e);
    e->accept();
}

void KeySequenceEdit::focusOutEvent(QFocusEvent *e) {
    // leaving the field means "never mind", not "the current text is final"
    cancelListening();
    QPushButton::focusOutEvent(e);
}

// block native tab-switching so we can use it in shortcuts
bool KeySequenceEdit::focusNextPrevChild(bool) {
    return false;
}

void KeySequenceEdit::processEvent(QEvent *e) {
    QInputEvent *inputEvent = dynamic_cast<QInputEvent *>(e);
    if(!inputEvent)
        return;
    QString tmp = ShortcutBuilder::fromEvent(inputEvent);
    if(!tmp.isEmpty())
        commitSequence(tmp);
}
