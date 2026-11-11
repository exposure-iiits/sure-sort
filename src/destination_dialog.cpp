#include "destination_dialog.h"
#include "shortcut_manager.h"
#include "ui_icons.h"
#include <QSet>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace photocull {
DestinationDialog::DestinationDialog(const DestinationConfig& config,ShortcutManager* manager,QWidget* parent) : QDialog(parent) {
    setWindowTitle("Sorting destinations"); resize(760,460); setMinimumSize(660,380);
    auto* layout=new QVBoxLayout(this); layout->setContentsMargins(18,18,18,18); layout->setSpacing(12);
    auto* row=new QHBoxLayout; root_=new QLineEdit(config.outputRoot,this); root_->setObjectName("destinationOutputRoot");root_->setPlaceholderText("Output directory");
    auto* browse=new QPushButton("Choose…",this);browse->setObjectName("chooseDestinationOutput"); row->addWidget(root_,1); row->addWidget(browse); layout->addLayout(row);
    connect(browse,&QPushButton::clicked,this,[this]{const auto path=QFileDialog::getExistingDirectory(this,"Output directory",root_->text());if(!path.isEmpty())root_->setText(path);});
    table_=new QTableWidget(0,3,this); table_->setObjectName("destinationFolders");table_->setHorizontalHeaderLabels({"Subfolder name","Shortcut",""});
    table_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Fixed); table_->setColumnWidth(1,160);
    table_->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Fixed); table_->setColumnWidth(2,48);
    table_->verticalHeader()->hide(); table_->verticalHeader()->setDefaultSectionSize(52);
    table_->setAlternatingRowColors(true); table_->setShowGrid(false);
    table_->setStyleSheet("QKeySequenceEdit,QComboBox{margin:6px 4px;padding:4px;}QKeySequenceEdit QLineEdit{border:0;padding:0;margin:0;}");
    table_->setSelectionBehavior(QAbstractItemView::SelectRows); layout->addWidget(table_,1);
    for (const auto& folder:config.folders) addFolder(folder);
    auto* buttons=new QHBoxLayout;
    auto* note=new QLabel("Folder keys copy photos; press again to remove the copy.\nGreen folder outlines mark existing copies. Ctrl+Z undoes the last action.",this);
    note->setWordWrap(true); note->setStyleSheet("color:#9aabba;font-size:12px;"); buttons->addWidget(note,1);
    auto* add=new QPushButton(uiIcon("plus"),QString(),this);add->setObjectName("addDestination");add->setFixedSize(36,36);add->setStyleSheet("padding:6px;");add->setToolTip("Add folder");add->setAccessibleName("Add folder");buttons->addWidget(add,0,Qt::AlignTop);layout->addLayout(buttons);
    connect(add,&QPushButton::clicked,this,[this,manager]{
        const auto existing=configuration();QSet<QString> names;
        for(const auto& folder:existing.folders)names.insert(folder.name.toCaseFolded());
        const auto key=manager->nextDestinationKey(existing);
        int n=1;while(names.contains(("Folder "+QString::number(n)).toCaseFolded()))++n;
        const int row=table_->rowCount();addFolder({newId(),"Folder "+QString::number(n),key});table_->setCurrentCell(row,0);table_->editItem(table_->item(row,0));
    });
    auto* actions=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,this); layout->addWidget(actions);
    connect(actions,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(actions,&QDialogButtonBox::accepted,this,[this,manager]{
        const auto config=configuration(); QString error=validateDestinationConfig(config);
        QVector<QPair<QString,QKeySequence>> bindings;
        for(const auto& d:config.folders){bindings.append({d.id,QKeySequence(d.shortcut,QKeySequence::PortableText)});bindings.append({d.id,QKeySequence(d.alternateShortcut,QKeySequence::PortableText)});}
        if(error.isEmpty())manager->validateDestinations(bindings,error);
        if(!error.isEmpty()){QMessageBox::warning(this,"Check destinations",error);return;} accept();
    });
}
void DestinationDialog::addFolder(const Destination& folder) {
    const int row=table_->rowCount(); table_->insertRow(row);
    auto* name=new QTableWidgetItem(folder.name); name->setData(Qt::UserRole,folder.id);name->setData(Qt::UserRole+1,folder.alternateShortcut);table_->setItem(row,0,name);
    auto* key=new QKeySequenceEdit(QKeySequence(folder.shortcut,QKeySequence::PortableText),table_);
    key->setMaximumSequenceLength(1); table_->setCellWidget(row,1,key);
    key->ensurePolished();table_->setRowHeight(row,qMax(56,key->minimumSizeHint().height()+14));
    auto* remove=new QPushButton(uiIcon("trash"),QString(),table_);remove->setObjectName("removeDestination");remove->setToolTip("Remove folder");remove->setAccessibleName("Remove "+folder.name);remove->setStyleSheet("QPushButton{margin:8px 6px;padding:4px;border:0;background:transparent;}QPushButton:hover{background:#60383e;}");table_->setCellWidget(row,2,remove);
    connect(remove,&QPushButton::clicked,this,[this,id=folder.id]{for(int row=0;row<table_->rowCount();++row)if(table_->item(row,0)->data(Qt::UserRole).toString()==id){table_->removeRow(row);break;}});
}
DestinationConfig DestinationDialog::configuration() const {
    DestinationConfig config; config.outputRoot=root_->text().trimmed();
    for(int row=0;row<table_->rowCount();++row) {
        const auto* name=table_->item(row,0); auto* key=qobject_cast<QKeySequenceEdit*>(table_->cellWidget(row,1));
        config.folders.append({name->data(Qt::UserRole).toString(),name->text(),key->keySequence().toString(QKeySequence::PortableText),name->data(Qt::UserRole+1).toString()});
    }
    return config;
}
}
