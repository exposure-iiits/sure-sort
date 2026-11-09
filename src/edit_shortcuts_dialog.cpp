#include "edit_shortcuts_dialog.h"
#include "shortcut_manager.h"
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QJsonObject>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
namespace photocull {
EditShortcutsDialog::EditShortcutsDialog(ShortcutManager* manager,QWidget* parent):QDialog(parent),manager_(manager){
    setWindowTitle("Editing shortcuts");resize(640,580);auto* layout=new QVBoxLayout(this);
    auto* hint=new QLabel("Choose a trigger and optional hold key. Leave the trigger blank to disable an action. Folder and navigation conflicts are checked.",this);hint->setWordWrap(true);layout->addWidget(hint);
    table_=new QTableWidget(this);table_->setObjectName("editingShortcuts");table_->setColumnCount(3);table_->setHorizontalHeaderLabels({"Action","Hold key","Trigger"});table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);layout->addWidget(table_);populate(manager->editBindings());
    error_=new QLabel(this);error_->setWordWrap(true);layout->addWidget(error_);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel|QDialogButtonBox::RestoreDefaults,this);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults),&QPushButton::clicked,this,[this]{populate(ShortcutManager::defaultEditBindings());});
    connect(buttons,&QDialogButtonBox::accepted,this,[this]{QString error;if(!manager_->validateEditBindings(bindings(),error)){error_->setText(error);return;}accept();});
}
void EditShortcutsDialog::populate(const QJsonArray& bindings){
    const auto actions=ShortcutManager::editActions();table_->setRowCount(actions.size());
    for(int i=0;i<actions.size();++i){QJsonObject row;for(const auto& v:bindings)if(v.toObject()["action"]==actions[i].first)row=v.toObject();
        auto* name=new QTableWidgetItem(actions[i].second);name->setFlags(name->flags()&~Qt::ItemIsEditable);table_->setItem(i,0,name);
        for(int j=1;j<=2;++j){auto* edit=new QKeySequenceEdit(QKeySequence::fromString(row[j==1?"hold":"trigger"].toString(),QKeySequence::PortableText),table_);edit->setMaximumSequenceLength(1);edit->setClearButtonEnabled(true);table_->setCellWidget(i,j,edit);}
    }
}
QJsonArray EditShortcutsDialog::bindings() const {
    QJsonArray result;const auto actions=ShortcutManager::editActions();
    for(int i=0;i<actions.size();++i)result.append(QJsonObject{{"action",actions[i].first},{"hold",qobject_cast<QKeySequenceEdit*>(table_->cellWidget(i,1))->keySequence().toString(QKeySequence::PortableText)},
        {"trigger",qobject_cast<QKeySequenceEdit*>(table_->cellWidget(i,2))->keySequence().toString(QKeySequence::PortableText)}});return result;
}
}
