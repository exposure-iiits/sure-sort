#include "shortcuts_dialog.h"
#include "shortcut_manager.h"
#include "sorting_controller.h"
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QTabBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QFocusEvent>
#include <functional>

namespace photocull {
namespace {
QString nativeKey(const QString& key, const QString& hold = {}) {
    auto result = QKeySequence(key).toString(QKeySequence::NativeText);
    if (result.isEmpty()) return "Unassigned";
    if (!hold.isEmpty()) result = QKeySequence(hold).toString(QKeySequence::NativeText) + " + " + result;
    return result;
}
bool modifier(int key) {
    return key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Meta;
}
class ShortcutCapture : public QLineEdit {
public:
    ShortcutCapture(bool allowHold, QWidget* parent) : QLineEdit(parent), allowHold_(allowHold) {
        setObjectName("shortcutCapture");
        setReadOnly(true); setPlaceholderText("Press shortcut…");
        setAccessibleName("Press a shortcut; Escape cancels; Delete clears");
    }
    std::function<void(QString,QString)> accepted;
    std::function<void()> canceled;
protected:
    void keyPressEvent(QKeyEvent* event) override {
        event->accept();
        if (finished_ || event->isAutoRepeat() || modifier(event->key())) return;
        if (event->key() == Qt::Key_Escape) { cancel(); return; }
        if (event->key() == Qt::Key_Delete && event->modifiers() == Qt::NoModifier) { finish({},{}); return; }
        const QKeySequence key(event->key() == Qt::Key_Plus && event->modifiers() == Qt::ShiftModifier
            ? QKeyCombination(Qt::NoModifier,Qt::Key_Plus) : event->keyCombination());
        if (first_.isEmpty()) { first_=key; firstKey_=event->key(); }
        else if (allowHold_ && event->key()!=firstKey_ && first_[0].keyboardModifiers()==Qt::NoModifier) {
            hold_=first_.toString(QKeySequence::PortableText); trigger_=key;
        }
        if (trigger_.isEmpty()) trigger_=key;
        setText(nativeKey(trigger_.toString(QKeySequence::PortableText),hold_));
    }
    void keyReleaseEvent(QKeyEvent* event) override {
        event->accept();
        if (!finished_ && !event->isAutoRepeat() && !modifier(event->key()) && !trigger_.isEmpty())
            finish(trigger_.toString(QKeySequence::PortableText),hold_);
    }
    void focusOutEvent(QFocusEvent* event) override { QLineEdit::focusOutEvent(event); cancel(); }
private:
    void finish(const QString& key,const QString& hold) { finished_=true; if(accepted)accepted(key,hold); }
    void cancel() { if(finished_)return; finished_=true; if(canceled)canceled(); }
    bool allowHold_, finished_=false;
    int firstKey_=0;
    QKeySequence first_,trigger_;
    QString hold_;
};
}
ShortcutsDialog::ShortcutsDialog(ShortcutManager* shortcuts, SortingController* sorting, QWidget* parent)
    : QDialog(parent), shortcuts_(shortcuts), sorting_(sorting) {
    setObjectName("shortcutReference");
    setWindowTitle("Sure Sort · Keyboard shortcuts"); resize(820,610);
    setWindowModality(Qt::WindowModal);
    auto* layout=new QVBoxLayout(this); layout->setContentsMargins(24,18,24,18);layout->setSpacing(12);
    auto* title=new QLabel("Keyboard shortcuts",this);title->setObjectName("settingsTitle");layout->addWidget(title);
    auto* help=new QLabel("Double-click to edit a shortcut · Press your new keys · Delete clears · Esc cancels",this);help->setObjectName("shortcutEditHint");help->setWordWrap(true);help->setStyleSheet("color:#9c9ca8;font-size:12px;");layout->addWidget(help);
    categories_=new QTabBar(this);categories_->setObjectName("shortcutCategories");
    categories_->setDrawBase(false);
    for(const QString title:{"Navigation","Viewing","Sorting","Editing","Application"})categories_->addTab(title);
    categories_->setExpanding(true);layout->addWidget(categories_);
    table_=new QTableWidget(0,3,this);table_->setObjectName("allShortcutBindings");
    table_->setHorizontalHeaderLabels({"Action","Primary","Alternate"});
    table_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
    for(int col:{1,2}){table_->horizontalHeader()->setSectionResizeMode(col,QHeaderView::Stretch);}
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);table_->setShowGrid(false);table_->verticalHeader()->hide();
    table_->installEventFilter(this);layout->addWidget(table_,1);
    toast_=new QLabel(this);toast_->setObjectName("shortcutToast");toast_->setWordWrap(true);toast_->hide();layout->addWidget(toast_);
    toastTimer_=new QTimer(this);toastTimer_->setSingleShot(true);toastTimer_->setInterval(4200);
    connect(toastTimer_,&QTimer::timeout,toast_,&QWidget::hide);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,this);
    connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::close);layout->addWidget(buttons);
    connect(categories_,&QTabBar::currentChanged,this,[this]{cancelCapture();populate();});
    connect(sorting_,&SortingController::configChanged,this,&ShortcutsDialog::populate);
    connect(table_,&QTableWidget::cellDoubleClicked,this,&ShortcutsDialog::beginCapture);
    populate();
}
void ShortcutsDialog::showCategory(Category category){categories_->setCurrentIndex(category);populate();}
void ShortcutsDialog::cancelCapture(){
    if(capture_){
        auto* editor=capture_.data();capture_=nullptr;
        if(auto* capture=dynamic_cast<ShortcutCapture*>(editor)){capture->accepted={};capture->canceled={};}
        table_->removeCellWidget(captureRow_,captureColumn_);editor->deleteLater();
    }
    captureRow_=captureColumn_=-1;
}
void ShortcutsDialog::showToast(const QString& message){toast_->setText(message);toast_->show();toastTimer_->start();}
void ShortcutsDialog::keyPressEvent(QKeyEvent* event){
    if(event->key()==Qt::Key_Escape&&capture_){cancelCapture();event->accept();return;}
    QDialog::keyPressEvent(event);
}
bool ShortcutsDialog::eventFilter(QObject* target,QEvent* event){
    if(target==table_&&event->type()==QEvent::KeyPress){
        auto* key=static_cast<QKeyEvent*>(event);
        if(key->key()==Qt::Key_Return||key->key()==Qt::Key_Enter){beginCapture(table_->currentRow(),table_->currentColumn());return true;}
    }
    return QDialog::eventFilter(target,event);
}
void ShortcutsDialog::populate(){
    cancelCapture();
    table_->setAccessibleName(categories_->tabText(categories_->currentIndex())+" shortcuts");
    table_->setRowCount(0);
    const QHash<QString,QString> names={
        {"next","Next photo"},{"previous","Previous photo"},{"first","First photo"},{"last","Last photo"},
        {"open","Open folder"},{"settings","Settings"},{"fit","Fit photo"},{"actual","Toggle 100% / Fit"},
        {"zoomIn","Zoom in"},{"zoomOut","Zoom out"},{"fullscreen","Toggle fullscreen"},
        {"escape","Cancel crop / leave fullscreen"},{"skip","Skip photo"},
        {"undoAssignment","Undo last copy / move"},{"clearAssignment","Clear latest placement"},{"confirmCrop","Confirm crop"}
    };
    auto add=[this](const QString& title,const QString& primary,const QString& alternate,const QString& kind,const QString& action){
        const int row=table_->rowCount();table_->insertRow(row);
        auto* label=new QTableWidgetItem(title);label->setData(Qt::UserRole,kind);label->setData(Qt::UserRole+1,action);table_->setItem(row,0,label);
        table_->setItem(row,1,new QTableWidgetItem(primary));table_->setItem(row,2,new QTableWidgetItem(alternate));table_->setRowHeight(row,40);
        for(int col:{1,2})table_->item(row,col)->setToolTip("Double-click to change · Delete clears · Esc cancels");
    };
    const QVector<QStringList> groups={
        {"next","previous","first","last","open"},{"fit","actual","zoomIn","zoomOut","fullscreen"},
        {"skip","undoAssignment","clearAssignment"},{"confirmCrop"},{"escape","settings"}
    };
    const int category=categories_->currentIndex();
    for(const auto& action:groups[category]){
        QStringList keys;for(const auto& value:shortcuts_->appBindings()){const auto row=value.toObject();if(row["action"]==action)keys.append(row["trigger"].toString());}
        add(names.value(action),nativeKey(keys.value(0)),nativeKey(keys.value(1)),"app",action);
    }
    if(category==Sorting)for(const auto& folder:sorting_->config().folders)
        add((folder.action=="move"?"Move to ":"Copy to ")+folder.name,nativeKey(folder.shortcut),nativeKey(folder.alternateShortcut),"folder",folder.id);
    if(category==Editing)for(const auto& action:ShortcutManager::editActions())for(const auto& value:shortcuts_->editBindings()){
        const auto row=value.toObject();if(row["action"]!=action.first)continue;
        add(action.second,nativeKey(row["trigger"].toString(),row["hold"].toString()),nativeKey(row["alternate"].toString(),row["hold"].toString()),"edit",action.first);
    }
}
void ShortcutsDialog::beginCapture(int row,int column){
    if(row<0||column<1||column>2)return;
    cancelCapture();
    toast_->hide();toastTimer_->stop();
    const auto kind=table_->item(row,0)->data(Qt::UserRole).toString(),action=table_->item(row,0)->data(Qt::UserRole+1).toString();
    auto* editor=new ShortcutCapture(kind=="edit",table_);capture_=editor;captureRow_=row;captureColumn_=column;
    editor->accepted=[this,kind,action,slot=column-1](QString key,QString hold){
        QTimer::singleShot(0,this,[this,kind,action,slot,key,hold]{cancelCapture();saveShortcut(kind,action,slot,key,hold);});
    };
    editor->canceled=[this]{QTimer::singleShot(0,this,[this]{cancelCapture();});};
    table_->setCellWidget(row,column,editor);editor->setFocus();
}
void ShortcutsDialog::saveShortcut(const QString& kind,const QString& action,int slot,const QString& key,const QString& hold){
    QString message;bool saved=false;
    if(kind=="folder"){
        auto config=sorting_->config();bool found=false;
        for(auto& folder:config.folders)if(folder.id==action){(slot==0?folder.shortcut:folder.alternateShortcut)=key;found=true;}
        QVector<QPair<QString,QKeySequence>> bindings;
        for(const auto& folder:config.folders){bindings.append({folder.id,QKeySequence(folder.shortcut)});bindings.append({folder.id,QKeySequence(folder.alternateShortcut)});}
        message=validateDestinationConfig(config);
        if(found&&message.isEmpty()&&shortcuts_->validateDestinations(bindings,message))saved=sorting_->configure(config);
        if(!found)message="This sorting folder no longer exists.";
    }else if(kind=="edit"){
        auto edits=shortcuts_->editBindings();
        for(int i=0;i<edits.size();++i){auto row=edits[i].toObject();if(row["action"]==action){
            // Editing alternates share the hold key in the persisted binding model.
            if(!key.isEmpty())row["hold"]=hold;
            row[slot==0?"trigger":"alternate"]=key;edits[i]=row;
        }}
        if(shortcuts_->validateEditBindings(edits,message)&&sorting_->saveEditBindings(edits))saved=shortcuts_->setEditBindings(edits,message);
    }else{
        auto app=shortcuts_->appBindings();int index=0;bool found=false;
        for(int i=0;i<app.size();++i){auto row=app[i].toObject();if(row["action"]==action){if(index++==slot){row["trigger"]=key;app[i]=row;found=true;}}}
        if(!found)app.append(QJsonObject{{"action",action},{"trigger",key}});
        if(shortcuts_->validateAppBindings(app,message)){
            auto preferences=sorting_->uiPreferences();preferences["appBindings"]=app;
            if(sorting_->saveUiPreferences(preferences))saved=shortcuts_->setAppBindings(app,message);
        }
    }
    populate();
    if(!saved)showToast(message.isEmpty()?"Shortcut could not be saved. Check the processing status.":message);
}
}
