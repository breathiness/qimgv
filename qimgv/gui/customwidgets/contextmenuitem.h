// A menu item that executes an action on press

#pragma once

#include "gui/customwidgets/menuitem.h"
#include "components/actionmanager/actionmanager.h"

class ContextMenuItem : public MenuItem {
    Q_OBJECT
public:
    ContextMenuItem(QWidget *parent = nullptr);
    ~ContextMenuItem();
    void setAction(QString mAction);
    // the action this row invokes, empty for rows that only open a page
    QString action() const;

signals:
    void pressed();

protected:
    virtual void onPress() override;

private:
    QString mAction;
};
