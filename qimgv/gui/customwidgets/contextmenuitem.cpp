#include "contextmenuitem.h"

ContextMenuItem::ContextMenuItem(QWidget *parent)
    : MenuItem(parent),
      mAction("")
{
}

ContextMenuItem::~ContextMenuItem() {
}

void ContextMenuItem::setAction(QString text) {
    this->mAction = text;
    setShortcutText(actionManager->primaryShortcutForAction(mAction));
}

QString ContextMenuItem::action() const {
    return mAction;
}

void ContextMenuItem::onPress() {
    emit pressed();
    if(!mAction.isEmpty())
        actionManager->invokeAction(mAction);
}
