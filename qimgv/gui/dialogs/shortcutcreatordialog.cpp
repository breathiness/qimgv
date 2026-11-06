#include "shortcutcreatordialog.h"
#include "ui_shortcutcreatordialog.h"
#include "utils/actions.h"

ShortcutCreatorDialog::ShortcutCreatorDialog(QWidget *parent) :
    MaterialDialog(parent),
    ui(new Ui::ShortcutCreatorDialog)
{
    ui->setupUi(this);
    enableMaterialChrome();
    // the title comes from the .ui file, which is already translated
    actionList = appActions->getList();
    scriptList = scriptManager->scriptNames();

    // the combo shows the readable name and carries the action id
    for(const QString &action : actionList)
        ui->actionsComboBox->addItem(appActions->displayName(action), action);
    ui->actionsComboBox->setCurrentIndex(0);

    ui->scriptsComboBox->addItems(scriptList);
    ui->scriptsComboBox->setCurrentIndex(0);
}

ShortcutCreatorDialog::~ShortcutCreatorDialog() {
    delete ui;
}

QString ShortcutCreatorDialog::selectedAction() {
    if(ui->actionsRadioButton->isChecked())
        return ui->actionsComboBox->currentData().toString();
    else
        return "s:"+ui->scriptsComboBox->currentText();
}

QString ShortcutCreatorDialog::selectedShortcut() {
    return ui->sequenceEdit->sequence();
}

void ShortcutCreatorDialog::onShortcutEdited() {
    QString action = actionManager->actionForShortcut(ui->sequenceEdit->sequence());
    if(!action.isEmpty())
        ui->warningLabel->setText(tr("This shortcut is already used for: %1. Replace?")
                                      .arg(appActions->displayName(action)));
    else
        ui->warningLabel->setText("");
}

void ShortcutCreatorDialog::setAction(QString action) {
    auto cbox = ui->actionsComboBox;
    if(action.startsWith("s:")) {
        action = action.remove(0,2);
        cbox = ui->scriptsComboBox;
        ui->scriptsRadioButton->setChecked(true);
    }
    const int index = cbox->findData(action);
    if(index != -1)
       cbox->setCurrentIndex(index);
}

void ShortcutCreatorDialog::setShortcut(QString shortcut) {
    ui->sequenceEdit->setText(shortcut);
}
