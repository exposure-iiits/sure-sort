#include "shortcut_manager.h"
#include "sorting_types.h"
#include <QApplication>
#include <QKeyEvent>
#include <QWidget>
#include <QJsonObject>
#include <QAbstractSlider>
#include <QComboBox>
#include <QLineEdit>
#include <QKeySequenceEdit>
#include <QAbstractSpinBox>

namespace photocull {
ShortcutManager::ShortcutManager(QWidget* window) : QObject(window), window_(window) {
    qApp->installEventFilter(this);
    bind("next", QKeySequence(Qt::Key_Right)); bind("next", QKeySequence(Qt::Key_D));
    bind("previous", QKeySequence(Qt::Key_Left)); bind("previous", QKeySequence(Qt::Key_A));
    bind("first", QKeySequence(Qt::Key_Home)); bind("last", QKeySequence(Qt::Key_End));
    bind("open", QKeySequence(Qt::CTRL | Qt::Key_O));
    bind("settings", QKeySequence(Qt::CTRL | Qt::Key_Comma), false);
    bind("fit", QKeySequence(Qt::Key_F)); bind("actual", QKeySequence(Qt::Key_Z));
    bind("zoomIn", QKeySequence(Qt::Key_Plus));
    bind("zoomIn", QKeySequence(Qt::Key_Equal)); bind("zoomOut", QKeySequence(Qt::Key_Minus));
    bind("fullscreen", QKeySequence(Qt::Key_F11)); bind("escape", QKeySequence(Qt::Key_Escape));
    bind("skip",QKeySequence(Qt::Key_Space),false);
    bind("undoAssignment",QKeySequence(Qt::CTRL | Qt::Key_Z),false);
    bind("clearAssignment",QKeySequence(Qt::Key_Backspace),false);
    bind("confirmCrop",QKeySequence(Qt::Key_Return),false);bind("confirmCrop",QKeySequence(Qt::Key_Enter),false);
    for(const auto& binding:bindings_)appBindings_.append(QJsonObject{{"action",binding.action},{"trigger",binding.trigger.toString(QKeySequence::PortableText)}});
    QString error;setEditBindings(defaultEditBindings(),error);
}
void ShortcutManager::bind(QString action, QKeySequence sequence,bool autoRepeat) { bindings_.append({std::move(action), sequence, 0,autoRepeat}); }
QVector<QPair<QString,QString>> ShortcutManager::allShortcuts() const {
    QVector<QPair<QString,QString>> result;
    for(const auto& binding:bindings_)result.append({binding.action,(binding.heldKey?QKeySequence(binding.heldKey).toString(QKeySequence::NativeText)+" + ":QString())+binding.trigger.toString(QKeySequence::NativeText)});
    for(const auto& row:editBindings_)if(row.toObject()["trigger"].toString().isEmpty())result.append({row.toObject()["action"].toString(),"Unassigned"});
    for(const auto& row:appBindings_)if(row.toObject()["trigger"].toString().isEmpty())result.append({row.toObject()["action"].toString(),"Unassigned"});
    return result;
}
void ShortcutManager::bindChord(QString action, int heldKey, QKeySequence trigger) {
    bindings_.append({std::move(action), trigger, heldKey});
}
bool ShortcutManager::validateAppBindings(const QJsonArray& input,QString& error) const {
    QSet<QString> used,known,seen;QHash<QString,int> counts;
    for(const auto& value:appBindings_)known.insert(value.toObject()["action"].toString());
    for(int i=0;i<input.size();++i){
        const auto row=input[i].toObject();
        const auto action=row["action"].toString();
        if(!input[i].isObject()||!known.contains(action)||++counts[action]>3||!row["trigger"].isString()){
            error="Invalid application shortcut.";return false;
        }
        seen.insert(action);
        const auto key=QKeySequence::fromString(row["trigger"].toString(),QKeySequence::PortableText);
        if(key.isEmpty())continue;
        if(key.count()!=1||key[0].key()==Qt::Key_unknown||key[0].key()==0){error="Use a single key or modifier plus key.";return false;}
        const auto text=key.toString(QKeySequence::PortableText);
        if(used.contains(text)){error=text+" is already used by another application shortcut.";return false;}used.insert(text);
        for(const auto& binding:bindings_)if(binding.action.startsWith("assign:")||binding.action.startsWith("edit:")){
            if((!binding.heldKey&&binding.trigger==key)||(binding.heldKey&&QKeySequence(binding.heldKey)==key)){
                error=text+" conflicts with "+binding.action+'.';return false;
            }
        }
    }
    if(seen!=known){error="Every application action must be included.";return false;}
    return true;
}
bool ShortcutManager::setAppBindings(const QJsonArray& input,QString& error) {
    auto normalized=input;
    // Older sessions stored Shift+'+' as a third Zoom-in entry. It is the same
    // physical plus key; normalize that alias before exposing two editable slots.
    bool plus=false;for(const auto& value:input){const auto row=value.toObject();if(row["action"]=="zoomIn"&&row["trigger"]=="+")plus=true;}
    if(plus)for(int i=normalized.size()-1;i>=0;--i){const auto row=normalized[i].toObject();if(row["action"]=="zoomIn"&&QKeySequence(row["trigger"].toString())==QKeySequence(Qt::SHIFT|Qt::Key_Plus))normalized.removeAt(i);}
    if(!validateAppBindings(normalized,error))return false;
    bindings_.removeIf([](const Binding& b){return !b.action.startsWith("edit:")&&!b.action.startsWith("assign:");});
    appBindings_=normalized;
    for(const auto& value:normalized){
        const auto row=value.toObject();const auto action=row["action"].toString();
        const auto key=QKeySequence::fromString(row["trigger"].toString(),QKeySequence::PortableText);
        if(key.isEmpty())continue;
        bool repeat=action!="settings"&&action!="skip"&&action!="undoAssignment"&&action!="clearAssignment"&&action!="confirmCrop";
        bind(action,key,repeat);
    }
    held_.clear();return true;
}
bool ShortcutManager::validateDestinations(const QVector<QPair<QString,QKeySequence>>& bindings,QString& error) const {
    QSet<QString> keys;
    for (const auto& candidate:bindings) {
        const auto sequence=candidate.second;
        if (sequence.isEmpty()) continue;
        if (sequence.count()!=1 || sequence[0].key()==Qt::Key_unknown || sequence[0].key()==0) { error="Use a single key or modifier plus key."; return false; }
        const auto key=sequence.toString(QKeySequence::PortableText);
        if (keys.contains(key)) { error="Two destination folders use "+key+'.'; return false; }
        keys.insert(key);
        for (const auto& binding:bindings_) if (!binding.action.startsWith("assign:") && binding.trigger==sequence) {
            error=key+" is already used by "+binding.action+'.'; return false;
        }
        for(const auto& binding:bindings_)if(binding.heldKey&&sequence==QKeySequence(binding.heldKey)){
            error=key+" is used as an editing hold key.";return false;
        }
    }
    return true;
}
bool ShortcutManager::setDestinations(const QVector<QPair<QString,QKeySequence>>& bindings,QString& error) {
    if (!validateDestinations(bindings,error)) return false;
    bindings_.removeIf([](const Binding& b){return b.action.startsWith("assign:");});
    for (const auto& binding:bindings) if (!binding.second.isEmpty()) bind("assign:"+binding.first,binding.second,false);
    held_.clear(); return true;
}
QString ShortcutManager::nextDestinationKey(const DestinationConfig& config) const {
    QVector<QPair<QString,QKeySequence>> bindings;
    for(const auto& folder:config.folders){
        bindings.append({folder.id,QKeySequence(folder.shortcut)});
        bindings.append({folder.id,QKeySequence(folder.alternateShortcut)});
    }
    // Number keys follow the visible folder order, then modifier-number keys.
    for(const QString& prefix:{QString(),QString("Ctrl+"),QString("Alt+"),QString("Ctrl+Alt+")})
        for(int n=1;n<=10;++n){
            const auto key=prefix+QString::number(n%10);auto candidate=bindings;
            candidate.append({QString(),QKeySequence(key)});QString error;
            if(validateDestinations(candidate,error))return key;
        }
    return {};
}
bool ShortcutManager::eventFilter(QObject* target, QEvent* event) {
    if (event->type() == QEvent::ApplicationDeactivate || (target == window_ && event->type() == QEvent::WindowDeactivate)) held_.clear();
    auto* widget = qobject_cast<QWidget*>(target);
    if (!widget || widget->window() != window_ || QApplication::activeModalWidget()) return false;
    if(event->type()==QEvent::KeyPress&&static_cast<QKeyEvent*>(event)->key()==Qt::Key_Escape)
        if(auto* popup=window_->findChild<QWidget*>("photoPopup");popup&&popup->isVisible())return false;
    for(auto* parent=widget;parent;parent=parent->parentWidget())
        if(parent->property("suspendPhotoShortcuts").toBool()||qobject_cast<QLineEdit*>(parent)||qobject_cast<QKeySequenceEdit*>(parent)||qobject_cast<QAbstractSpinBox*>(parent)){
            held_.clear();return false;
        }
    if (event->type() == QEvent::KeyRelease) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (!key->isAutoRepeat()) held_.remove(key->key());
    }
    if (event->type() != QEvent::KeyPress) return false;
    const auto* key = static_cast<QKeyEvent*>(event);
    const QKeySequence sequence(key->key()==Qt::Key_Plus&&key->modifiers()==Qt::ShiftModifier ?
        QKeyCombination(Qt::NoModifier,Qt::Key_Plus) : key->keyCombination());
    // Chords take precedence over plain navigation while a hold key is down.
    for (auto it=bindings_.crbegin();it!=bindings_.crend();++it) {const auto& binding=*it;
        if (binding.heldKey && held_.contains(binding.heldKey) && binding.trigger == sequence) {
            if (!key->isAutoRepeat() || binding.autoRepeat) emit triggered(binding.action);
            return true;
        }
    }
    if((qobject_cast<QAbstractSlider*>(widget)||qobject_cast<QComboBox*>(widget))&&
       (key->key()==Qt::Key_Left||key->key()==Qt::Key_Right||key->key()==Qt::Key_Up||key->key()==Qt::Key_Down||key->key()==Qt::Key_Home||key->key()==Qt::Key_End))return false;
    held_.insert(key->key());
    for (const auto& binding : bindings_)
        if (!binding.heldKey && binding.trigger == sequence) {
            if (!key->isAutoRepeat() || binding.autoRepeat) emit triggered(binding.action);
            return true;
        }
    return false;
}
QVector<QPair<QString,QString>> ShortcutManager::editActions(){
    QVector<QPair<QString,QString>> result;
    for(const QString name:{"Exposure","Brightness","Contrast","Highlights","Shadows","Saturation"}){
        result.append({"edit:"+name.toLower()+"Up",name+" increase"});result.append({"edit:"+name.toLower()+"Down",name+" decrease"});
    }
    result.append(QPair<QString,QString>{"edit:rotateRight","Rotate clockwise"});result.append(QPair<QString,QString>{"edit:rotateLeft","Rotate counterclockwise"});
    result.append(QPair<QString,QString>{"edit:reset","Reset all edits"});result.append(QPair<QString,QString>{"edit:crop","Crop"});return result;
}
QJsonArray ShortcutManager::defaultEditBindings(){
    QJsonArray result;const auto actions=editActions();const QStringList holds={"E","B","C","H","S","T","R"};
    for(int i=0;i<14;++i)result.append(QJsonObject{{"action",actions[i].first},{"hold",holds[i/2]},{"trigger",i%2==0?"Right":"Left"}});
    result.append(QJsonObject{{"action","edit:reset"},{"hold",""},{"trigger","Ctrl+Shift+R"}});
    result.append(QJsonObject{{"action","edit:crop"},{"hold",""},{"trigger","K"}});return result;
}
bool ShortcutManager::validateEditBindings(const QJsonArray& input,QString& error) const {
    const auto config=input.isEmpty()?defaultEditBindings():input;QSet<QString> actions,pairs;
    const auto supported=editActions();if(config.size()!=supported.size()){error="Every editing action must have one binding row.";return false;}
    for(const auto& value:config){
        if(!value.isObject()){error="Invalid editing shortcut.";return false;}
        const auto row=value.toObject();const auto action=row["action"].toString();bool known=false;for(const auto& item:supported)if(item.first==action)known=true;
        if(!known||actions.contains(action)){error="Unknown or repeated editing action.";return false;}actions.insert(action);
        const auto hold=QKeySequence::fromString(row["hold"].toString(),QKeySequence::PortableText);
        for(const auto& field:QStringList{"trigger","alternate"}){
        const auto trigger=QKeySequence::fromString(row[field].toString(),QKeySequence::PortableText);
        if(trigger.isEmpty())continue; // Empty trigger explicitly disables this action.
        if(trigger.count()!=1||trigger[0].key()==Qt::Key_unknown||trigger[0].key()==0||hold.count()>1||
           (!hold.isEmpty()&&(hold[0].keyboardModifiers()!=Qt::NoModifier||hold[0].key()==Qt::Key_unknown||hold[0].key()==0||hold[0]==trigger[0]))){error="Use a single trigger and an optional unmodified hold key.";return false;}
        const auto pair=hold.toString(QKeySequence::PortableText)+"|"+trigger.toString(QKeySequence::PortableText);
        if(pairs.contains(pair)){error="Two editing actions use the same hold/trigger.";return false;}pairs.insert(pair);
        for(const auto& binding:bindings_)if(!binding.action.startsWith("edit:")){
            if((hold.isEmpty()&&binding.trigger==trigger)||(!hold.isEmpty()&&binding.trigger==hold)||
               (binding.action.startsWith("assign:")&&binding.trigger==trigger)){
                error="Editing shortcut conflicts with "+binding.action+'.';return false;
            }
        }
        }
    }
    // A plain editing action cannot also act as another action's hold key.
    for(const auto& a:config)for(const auto& b:config){const auto one=a.toObject(),two=b.toObject();
        for(const auto& field:QStringList{"trigger","alternate"})if(one["hold"].toString().isEmpty()&&!one[field].toString().isEmpty()&&one[field]==two["hold"]&&(!two["trigger"].toString().isEmpty()||!two["alternate"].toString().isEmpty())){
            error="An editing hold key is also bound to a plain editing action.";return false;
        }
    }
    return true;
}
bool ShortcutManager::setEditBindings(const QJsonArray& input,QString& error){
    if(!validateEditBindings(input,error))return false;
    editBindings_=input.isEmpty()?defaultEditBindings():input;
    bindings_.removeIf([](const Binding& b){return b.action.startsWith("edit:");});held_.clear();
    for(const auto& value:editBindings_){const auto row=value.toObject();for(const auto& field:QStringList{"trigger","alternate"}){const auto trigger=QKeySequence::fromString(row[field].toString(),QKeySequence::PortableText);if(trigger.isEmpty())continue;
        const auto hold=QKeySequence::fromString(row["hold"].toString(),QKeySequence::PortableText);
        if(hold.isEmpty())bind(row["action"].toString(),trigger,true);else bindChord(row["action"].toString(),hold[0].key(),trigger);
    }}
    return true;
}
}
