#pragma once

#include "gui/customwidgets/materialdialog.h"

#include <QFileDialog>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include "components/scriptmanager/scriptmanager.h"
#include "utils/script.h"

namespace Ui {
class ScriptEditorDialog;
}

class ScriptEditorDialog : public MaterialDialog
{
    Q_OBJECT

public:
    explicit ScriptEditorDialog(QWidget *parent = nullptr);
    explicit ScriptEditorDialog(QString name, Script script, QWidget *parent = nullptr);
    ~ScriptEditorDialog();
    QString scriptName();
    Script script();

private slots:
    void onNameChanged(QString name);

    void selectScriptPath();
private:
    Ui::ScriptEditorDialog *ui;
    bool editMode;
    QString editTarget;
};
