#include "edit_panel.h"
#include <QComboBox>
#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>
#include <QDoubleSpinBox>
#include <QKeySequence>
#include <QJsonObject>
#include <QApplication>

namespace photocull {
EditPanel::EditPanel(QWidget* parent):QWidget(parent){
    setObjectName("editPanel");setMinimumWidth(220);setStyleSheet("QWidget#editPanel {background:transparent;} QWidget#editPanel QLabel {background:transparent;color:#c9c9d3;font-size:12px;} QWidget#editPanel QPushButton {background:rgba(255,255,255,8);border:1px solid rgba(255,255,255,30);border-radius:6px;padding:6px 10px;} QWidget#editPanel QPushButton:hover {background:rgba(255,255,255,20);} QWidget#editPanel QComboBox {background:rgba(10,10,14,80);border:1px solid rgba(255,255,255,30);border-radius:6px;}");auto* layout=new QVBoxLayout(this);layout->setContentsMargins(14,12,14,14);layout->setSpacing(8);
    auto* title=new QLabel("Edit photo",this);title->setStyleSheet("font-size:15px;font-weight:600;color:#eeeef1;");layout->addWidget(title);
    const QStringList names={"Exposure","Brightness","Contrast","Highlights","Shadows","Saturation"};
    auto hint=[this](const QString& action){auto* label=new QLabel(this);label->setObjectName("editShortcutHint");label->setProperty("editAction",action);label->setStyleSheet("color:rgba(198,204,220,105);font-size:10px;");shortcutHints_.insert(action,label);return label;};
    auto number=[this](const QString& name,int decimals,double low,double high,const QString& suffix){
        auto* value=new QDoubleSpinBox(this);value->setObjectName(name);value->setAccessibleName(name.mid(4)+" value");value->setDecimals(decimals);value->setRange(low,high);value->setSuffix(suffix);value->setSingleStep(decimals?.1:1);value->setButtonSymbols(QAbstractSpinBox::NoButtons);value->setKeyboardTracking(false);value->setAlignment(Qt::AlignRight);value->setFixedWidth(70);value->setToolTip("Type a value · Enter to apply");value->setStyleSheet("QDoubleSpinBox {background:transparent;border:1px solid transparent;border-radius:4px;padding:3px 2px;color:#d4d4df;font-size:12px;} QDoubleSpinBox:hover {border-color:#484850;} QDoubleSpinBox:focus {background:#161619;border-color:#8ba8c9;}");return value;
    };
    for(int i=0;i<names.size();++i){
        auto* row=new QHBoxLayout;row->setSpacing(4);row->addWidget(new QLabel(names[i],this));row->addWidget(hint(names[i].toLower()));row->addStretch();auto* value=number("edit"+names[i]+"Value",i==0?1:0,i==0?-5:-100,i==0?5:100,i==0?" EV":"");row->addWidget(value);layout->addLayout(row);values_.append(value);
        auto* slider=new QSlider(Qt::Horizontal,this);slider->setObjectName("edit"+names[i]);slider->setRange(i==0?-50:-100,i==0?50:100);slider->setSingleStep(5);slider->setPageStep(5);slider->setToolTip("Double-click to reset this adjustment");slider->installEventFilter(this);layout->addWidget(slider);sliders_.append(slider);
        connect(slider,&QSlider::valueChanged,this,[this,i](int n){int* fields[]={&edits_.exposure,&edits_.brightness,&edits_.contrast,&edits_.highlights,&edits_.shadows,&edits_.saturation};*fields[i]=n;setEdits(edits_);emit changed(edits_);});
        connect(value,&QDoubleSpinBox::valueChanged,this,[this,i](double n){sliders_[i]->setValue(qRound(i==0?n*10:n));});
    }
    auto* rotationRow=new QHBoxLayout;rotationRow->setSpacing(4);rotationRow->addWidget(new QLabel("Rotation",this));rotationRow->addWidget(hint("rotation"));rotationRow->addStretch();rotationValue_=number("rotationValue",0,-180,180,"°");rotationRow->addWidget(rotationValue_);layout->addLayout(rotationRow);
    rotation_=new QSlider(Qt::Horizontal,this);rotation_->setObjectName("editRotation");rotation_->setAccessibleName("Rotation in degrees");rotation_->setRange(-180,180);rotation_->setSingleStep(1);rotation_->setPageStep(15);rotation_->setToolTip("Double-click to reset rotation");rotation_->installEventFilter(this);layout->addWidget(rotation_);
    auto* limits=new QHBoxLayout;limits->addWidget(new QLabel("−180°",this));limits->addStretch();limits->addWidget(new QLabel("0°",this));limits->addStretch();limits->addWidget(new QLabel("+180°",this));layout->addLayout(limits);
    rotation_->setToolTip("Snaps near 0°, ±90° and ±180° · Hold Alt for precision · Double-click to reset");
    connect(rotation_,&QSlider::valueChanged,this,[this](int degrees){
        if(rotation_->isSliderDown()&&!(QApplication::keyboardModifiers()&Qt::AltModifier))for(int anchor:{-180,-90,0,90,180})if(qAbs(degrees-anchor)<=4){degrees=anchor;break;}
        edits_.rotation=0;edits_.rotationDegrees=degrees;setEdits(edits_);emit changed(edits_);
    });
    connect(rotationValue_,&QDoubleSpinBox::valueChanged,this,[this](double n){rotation_->setValue(qRound(n));});
    auto* crop=new QPushButton("Crop",this);crop->setObjectName("cropPhoto");auto* cropRow=new QHBoxLayout;cropRow->setSpacing(8);cropRow->addWidget(crop,1);cropRow->addWidget(hint("crop"));layout->addLayout(cropRow);connect(crop,&QPushButton::clicked,this,&EditPanel::cropRequested);
    auto* aspect=new QComboBox(this);aspect->setObjectName("cropAspect");
    const QList<QPair<QString,double>> ratios={{"Free",0},{"Original ratio",-1},{"1:1",1},{"4:5",.8},{"5:4",1.25},{"3:2",1.5},{"2:3",2.0/3},{"4:3",4.0/3},{"3:4",.75},{"16:9",16.0/9},{"9:16",9.0/16}};
    for(const auto& ratio:ratios) aspect->addItem(ratio.first,ratio.second);
    cropRow->addWidget(aspect,1);crop->setFixedHeight(32);aspect->setFixedHeight(32);
    connect(aspect,&QComboBox::currentIndexChanged,this,[this,aspect]{emit cropAspectChanged(aspect->currentData().toDouble());});
    auto* reset=new QPushButton("Reset All",this);reset->setObjectName("resetEdits");auto* resetRow=new QHBoxLayout;resetRow->setSpacing(8);resetRow->addWidget(reset,1);layout->addLayout(resetRow);connect(reset,&QPushButton::clicked,this,[this]{setEdits({});emit changed(edits_);});
    auto* shortcuts=new QPushButton("Editing shortcuts",this);shortcuts->setObjectName("editingShortcutsButton");resetRow->addWidget(shortcuts,1);reset->setFixedHeight(32);shortcuts->setFixedHeight(32);reset->setToolTip("Reset all edits · See Editing shortcuts for the current binding");connect(shortcuts,&QPushButton::clicked,this,&EditPanel::shortcutsRequested);
    reset->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);shortcuts->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
    layout->addStretch();setEdits({});
}
void EditPanel::setEdits(const PhotoEdits& edits){
    edits_=edits;const int values[]={edits.exposure,edits.brightness,edits.contrast,edits.highlights,edits.shadows,edits.saturation};
    for(int i=0;i<sliders_.size();++i){QSignalBlocker block(sliders_[i]),valueBlock(values_[i]);sliders_[i]->setValue(values[i]);values_[i]->setValue(i==0?values[i]/10.0:values[i]);}
    const int degrees=editRotationDegrees(edits);QSignalBlocker block(rotation_),valueBlock(rotationValue_);rotation_->setValue(degrees);rotationValue_->setValue(degrees);
}
void EditPanel::setShortcutBindings(const QJsonArray& bindings){
    auto key=[](QString text){return QKeySequence(text).toString(QKeySequence::NativeText).replace("Right","→").replace("Left","←").replace("Up","↑").replace("Down","↓");};
    auto binding=[&](const QString& action){for(const auto& value:bindings){const auto row=value.toObject();const auto trigger=row["trigger"].toString().isEmpty()?row["alternate"].toString():row["trigger"].toString();if(row["action"]=="edit:"+action&&!trigger.isEmpty())return (row["hold"].toString().isEmpty()?QString():key(row["hold"].toString())+" ")+key(trigger);}return QString();};
    for(auto it=shortcutHints_.begin();it!=shortcutHints_.end();++it){
        QString text;
        if(it.key()=="reset"||it.key()=="crop")text=binding(it.key());
        else {const auto down=binding(it.key()=="rotation"?"rotateLeft":it.key()+"Down"),up=binding(it.key()=="rotation"?"rotateRight":it.key()+"Up");
            const auto space=down.lastIndexOf(' ');
            text=!down.isEmpty()&&!up.isEmpty()?(space>0&&up.left(space+1)==down.left(space+1)?down+" / "+up.mid(space+1):down+" / "+up):down+up;
        }
        it.value()->setFixedWidth(qMin(64,it.value()->fontMetrics().horizontalAdvance(text)));it.value()->setText(it.value()->fontMetrics().elidedText(text,Qt::ElideRight,64));it.value()->setToolTip(text.isEmpty()?"No shortcut assigned":"Keyboard: "+text);it.value()->setVisible(!text.isEmpty());
    }
}
bool EditPanel::eventFilter(QObject* target,QEvent* event){if(event->type()==QEvent::MouseButtonDblClick)if(auto* slider=qobject_cast<QSlider*>(target)){slider->setValue(0);return true;}return QWidget::eventFilter(target,event);}
}
