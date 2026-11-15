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
#include <QPainter>
#include <QStyledItemDelegate>
#include <QPushButton>
#include <QHBoxLayout>
#include <QScrollBar>
#include "ui_icons.h"
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
QStringList keyCaps(const QString& key,const QString& hold={}){
    QStringList caps;
    if(key.isEmpty())return caps;
    if(!hold.isEmpty())caps.append(keyCaps(hold));
    const auto sequence=QKeySequence::fromString(key,QKeySequence::PortableText);
    if(sequence.isEmpty())return caps;
    const auto combination=sequence[0];const auto modifiers=combination.keyboardModifiers();
    if(modifiers.testFlag(Qt::ControlModifier))caps.append("Ctrl");
    if(modifiers.testFlag(Qt::AltModifier))caps.append("Alt");
    if(modifiers.testFlag(Qt::ShiftModifier))caps.append(QString::fromUtf8("⇧"));
    if(modifiers.testFlag(Qt::MetaModifier))caps.append("Meta");
    const QHash<int,QString> symbols={{Qt::Key_Left,QString::fromUtf8("←")},{Qt::Key_Right,QString::fromUtf8("→")},{Qt::Key_Up,QString::fromUtf8("↑")},{Qt::Key_Down,QString::fromUtf8("↓")},{Qt::Key_Return,QString::fromUtf8("↵")},{Qt::Key_Enter,QString::fromUtf8("↵")},{Qt::Key_Backspace,QString::fromUtf8("⌫")},{Qt::Key_Delete,"Del"},{Qt::Key_Escape,"Esc"},{Qt::Key_Space,"Space"},{Qt::Key_PageUp,"PgUp"},{Qt::Key_PageDown,"PgDn"}};
    caps.append(symbols.value(combination.key(),QKeySequence(combination.key()).toString(QKeySequence::NativeText)));
    return caps;
}
class KeycapDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem& option,const QModelIndex& index) const override {
        const auto* table=qobject_cast<QTableWidget*>(parent());
        const int width=table?table->columnWidth(index.column()):option.rect.width();
        auto font=option.font;font.setPixelSize(index.column()==0?12:11);font.setWeight(index.column()==0?QFont::Normal:QFont::DemiBold);const QFontMetrics metrics(font);
        int height=60;
        if(index.column()==0)height=qMax(height,metrics.boundingRect(QRect(0,0,qMax(1,width-8),1000),Qt::TextWordWrap,index.data().toString()).height()+12);
        else {int lines=1,used=0;for(const auto& cap:index.data(Qt::UserRole).toStringList()){const int capWidth=qMax(23,metrics.horizontalAdvance(cap)+12);if(used&&used+capWidth+4>width-8){++lines;used=0;}used+=capWidth+4;}height=qMax(height,lines*26+12);}
        return {width,height};
    }
    void paint(QPainter* painter,const QStyleOptionViewItem& option,const QModelIndex& index) const override {
        painter->save();painter->setClipRect(option.rect);painter->setRenderHint(QPainter::Antialiasing);
        if(option.state.testFlag(QStyle::State_Selected))painter->fillRect(option.rect,QColor(255,255,255,9));
        painter->setPen(QColor(255,255,255,14));painter->drawLine(option.rect.bottomLeft(),option.rect.bottomRight());
        auto font=option.font;font.setPixelSize(12);painter->setFont(font);
        if(index.column()==0){painter->setPen(QColor("#c9c9d3"));painter->drawText(option.rect.adjusted(2,4,-6,-4),Qt::AlignVCenter|Qt::TextWordWrap,index.data().toString());}
        else {
            const auto caps=index.data(Qt::UserRole).toStringList();
            if(caps.isEmpty()){painter->setPen(QColor("#787882"));painter->drawText(option.rect,Qt::AlignCenter,QString::fromUtf8("—"));}
            else {
                font.setPixelSize(11);font.setWeight(QFont::DemiBold);painter->setFont(font);const QFontMetrics metrics(font);
                QVector<QVector<QString>> lines(1);int used=0;const int available=option.rect.width()-8;
                for(const auto& cap:caps){const int width=qMax(23,metrics.horizontalAdvance(cap)+12);if(used&&used+width+4>available){lines.append(QVector<QString>{});used=0;}lines.last().append(cap);used+=width+4;}
                int y=option.rect.center().y()-(lines.size()*23+(lines.size()-1)*3)/2;
                for(const auto& line:lines){int width=0;for(const auto& cap:line)width+=qMax(23,metrics.horizontalAdvance(cap)+12)+4;int x=option.rect.center().x()-(width-4)/2;
                    for(const auto& cap:line){const int capWidth=qMax(23,metrics.horizontalAdvance(cap)+12);const QRectF box(x,y,capWidth,23);painter->setPen(QPen(QColor("#5d5d69"),1));painter->setBrush(QColor("#34343c"));painter->drawRoundedRect(box,4,4);painter->setPen(QColor("#202026"));painter->drawLine(QPointF(x+4,y+21),QPointF(x+capWidth-4,y+21));painter->setPen(QColor("#e1e1ea"));painter->drawText(box.adjusted(0,-1,0,-1),Qt::AlignCenter,cap);x+=capWidth+4;}y+=26;
                }
            }
        }
        painter->restore();
    }
};
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
    setWindowTitle("Sure Sort · Keyboard shortcuts"); resize(324,590);setMinimumWidth(0);
    setStyleSheet("QDialog#shortcutReference {background:transparent;} QDialog#shortcutReference QLabel {background:transparent;color:#c9c9d3;font-size:12px;} QTableWidget#allShortcutBindings {background:transparent;border:0;font-size:12px;outline:0;} QHeaderView::section {background:transparent;border:0;color:#8f8f9b;font-size:10px;padding:5px 0;} QTabBar::tab {font-size:11px;padding:7px 4px;min-width:0;} QLineEdit#shortcutCapture {font-size:11px;padding:2px;border:1px solid #8ba8c9;border-radius:4px;background:#24242c;color:#eeeef1;} QScrollBar:vertical {width:6px;background:transparent;}");
    auto* layout=new QVBoxLayout(this); layout->setContentsMargins(14,12,14,14);layout->setSpacing(8);
    auto* heading=new QHBoxLayout;auto* title=new QLabel("Keyboard shortcuts",this);title->setWordWrap(true);title->setStyleSheet("font-size:15px;font-weight:600;color:#eeeef1;");heading->addWidget(title,1);
    auto* close=new QPushButton(QString::fromUtf8("×"),this);close->setObjectName("closeShortcuts");close->setAccessibleName("Close shortcuts");close->setFixedSize(24,24);close->setStyleSheet("font-size:18px;border:0;background:transparent;color:#aaaab5;padding:0;");connect(close,&QPushButton::clicked,this,&QDialog::reject);heading->addWidget(close);layout->addLayout(heading);
    auto* help=new QLabel("Click a key to change it.\nDelete clears · Esc cancels",this);help->setObjectName("shortcutEditHint");help->setWordWrap(true);help->setStyleSheet("color:#9c9ca8;font-size:11px;");layout->addWidget(help);
    categories_=new QTabBar(this);categories_->setObjectName("shortcutCategories");
    categories_->setDrawBase(false);categories_->setUsesScrollButtons(false);categories_->setMinimumWidth(0);categories_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);categories_->setElideMode(Qt::ElideRight);
    for(const QString title:{"Navigation","Viewing","Editing"})categories_->addTab(title);
    categories_->setExpanding(true);layout->addWidget(categories_);
    table_=new QTableWidget(0,3,this);table_->setObjectName("allShortcutBindings");
    table_->setHorizontalHeaderLabels({"Action","Primary","Alternate"});
    table_->horizontalHeader()->setMinimumSectionSize(0);table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);table_->setItemDelegate(new KeycapDelegate(table_));
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);table_->setShowGrid(false);table_->verticalHeader()->hide();
    table_->installEventFilter(this);table_->viewport()->installEventFilter(this);layout->addWidget(table_,1);
    toast_=new QLabel(this);toast_->setObjectName("shortcutToast");toast_->setWordWrap(true);toast_->hide();layout->addWidget(toast_);
    toastTimer_=new QTimer(this);toastTimer_->setSingleShot(true);toastTimer_->setInterval(4200);
    connect(toastTimer_,&QTimer::timeout,toast_,&QWidget::hide);
    connect(categories_,&QTabBar::currentChanged,this,[this]{cancelCapture();populate();});
    connect(sorting_,&SortingController::configChanged,this,&ShortcutsDialog::populate);
    connect(table_,&QTableWidget::cellClicked,this,&ShortcutsDialog::beginCapture);
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
    if(target==table_->viewport()&&event->type()==QEvent::Resize){const int width=table_->viewport()->width();const int keys=width*29/100;table_->setColumnWidth(0,width-keys*2);table_->setColumnWidth(1,keys);table_->setColumnWidth(2,keys);table_->resizeRowsToContents();}
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
        {"clearAssignment","Clear latest placement"},{"confirmCrop","Confirm crop"}
    };
    auto add=[this](const QString& title,const QString& primary,const QString& alternate,const QString& kind,const QString& action,const QString& hold=QString()){
        const int row=table_->rowCount();table_->insertRow(row);
        auto* label=new QTableWidgetItem(title);label->setData(Qt::UserRole,kind);label->setData(Qt::UserRole+1,action);table_->setItem(row,0,label);
        table_->setItem(row,1,new QTableWidgetItem(nativeKey(primary,hold)));table_->setItem(row,2,new QTableWidgetItem(nativeKey(alternate,hold)));table_->setRowHeight(row,60);
        table_->item(row,1)->setData(Qt::UserRole,keyCaps(primary,hold));table_->item(row,2)->setData(Qt::UserRole,keyCaps(alternate,hold));
        label->setToolTip(title);
        for(int col:{1,2})table_->item(row,col)->setToolTip(table_->item(row,col)->text()+"\nClick to change · Delete clears · Esc cancels");
    };
    const QVector<QStringList> groups={
        {"next","previous","first","last","open"},{"actual","zoomIn","zoomOut","fullscreen"},
        {"confirmCrop"}
    };
    const int category=categories_->currentIndex();
    for(const auto& action:groups[category]){
        QStringList keys;for(const auto& value:shortcuts_->appBindings()){const auto row=value.toObject();if(row["action"]==action)keys.append(row["trigger"].toString());}
        add(names.value(action),keys.value(0),keys.value(1),"app",action);
    }
    if(category==Editing)for(const auto& action:ShortcutManager::editActions())for(const auto& value:shortcuts_->editBindings()){
        const auto row=value.toObject();if(row["action"]!=action.first)continue;
        add(action.second,row["trigger"].toString(),row["alternate"].toString(),"edit",action.first,row["hold"].toString());
    }
    table_->resizeRowsToContents();
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
