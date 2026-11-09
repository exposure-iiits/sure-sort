#include "edit_panel.h"
#include <QComboBox>
#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

namespace photocull {
EditPanel::EditPanel(QWidget* parent):QWidget(parent){
    setObjectName("editPanel");setFixedWidth(238);auto* layout=new QVBoxLayout(this);layout->setContentsMargins(12,12,12,12);layout->setSpacing(8);
    auto* title=new QLabel("Edit photo",this);title->setStyleSheet("font-weight:600;");layout->addWidget(title);
    const QStringList names={"Exposure","Brightness","Contrast","Highlights","Shadows","Saturation"};
    for(int i=0;i<names.size();++i){
        auto* row=new QHBoxLayout;row->addWidget(new QLabel(names[i],this));auto* value=new QLabel(this);value->setAlignment(Qt::AlignRight);row->addWidget(value);layout->addLayout(row);values_.append(value);
        auto* slider=new QSlider(Qt::Horizontal,this);slider->setObjectName("edit"+names[i]);slider->setRange(i==0?-50:-100,i==0?50:100);slider->setSingleStep(5);slider->setPageStep(5);slider->setToolTip("Double-click to reset this adjustment");slider->installEventFilter(this);layout->addWidget(slider);sliders_.append(slider);
        connect(slider,&QSlider::valueChanged,this,[this,i](int n){int* fields[]={&edits_.exposure,&edits_.brightness,&edits_.contrast,&edits_.highlights,&edits_.shadows,&edits_.saturation};*fields[i]=n;setEdits(edits_);emit changed(edits_);});
    }
    auto* rotationRow=new QHBoxLayout;rotationRow->addWidget(new QLabel("Rotation",this));rotationValue_=new QLabel(this);rotationValue_->setObjectName("rotationValue");rotationValue_->setAlignment(Qt::AlignRight);rotationRow->addWidget(rotationValue_);layout->addLayout(rotationRow);
    rotation_=new QSlider(Qt::Horizontal,this);rotation_->setObjectName("editRotation");rotation_->setAccessibleName("Rotation in degrees");rotation_->setRange(-180,180);rotation_->setSingleStep(1);rotation_->setPageStep(15);rotation_->setToolTip("Double-click to reset rotation");rotation_->installEventFilter(this);layout->addWidget(rotation_);
    auto* limits=new QHBoxLayout;limits->addWidget(new QLabel("−180°",this));limits->addStretch();limits->addWidget(new QLabel("0°",this));limits->addStretch();limits->addWidget(new QLabel("+180°",this));layout->addLayout(limits);
    connect(rotation_,&QSlider::valueChanged,this,[this](int degrees){edits_.rotation=0;edits_.rotationDegrees=degrees;setEdits(edits_);emit changed(edits_);});
    auto* crop=new QPushButton("Crop",this);crop->setObjectName("cropPhoto");layout->addWidget(crop);connect(crop,&QPushButton::clicked,this,&EditPanel::cropRequested);
    auto* aspect=new QComboBox(this);aspect->setObjectName("cropAspect");
    const QList<QPair<QString,double>> ratios={{"Free",0},{"Original ratio",-1},{"1:1",1},{"4:5",.8},{"5:4",1.25},{"3:2",1.5},{"2:3",2.0/3},{"4:3",4.0/3},{"3:4",.75},{"16:9",16.0/9},{"9:16",9.0/16}};
    for(const auto& ratio:ratios) aspect->addItem(ratio.first,ratio.second);
    layout->addWidget(aspect);
    connect(aspect,&QComboBox::currentIndexChanged,this,[this,aspect]{emit cropAspectChanged(aspect->currentData().toDouble());});
    auto* reset=new QPushButton("Reset All",this);reset->setObjectName("resetEdits");layout->addWidget(reset);connect(reset,&QPushButton::clicked,this,[this]{setEdits({});emit changed(edits_);});
    auto* shortcuts=new QPushButton("Editing shortcuts",this);shortcuts->setObjectName("editingShortcutsButton");layout->addWidget(shortcuts);connect(shortcuts,&QPushButton::clicked,this,&EditPanel::shortcutsRequested);
    layout->addStretch();setEdits({});
}
void EditPanel::setEdits(const PhotoEdits& edits){
    edits_=edits;const int values[]={edits.exposure,edits.brightness,edits.contrast,edits.highlights,edits.shadows,edits.saturation};
    for(int i=0;i<sliders_.size();++i){QSignalBlocker block(sliders_[i]);sliders_[i]->setValue(values[i]);values_[i]->setText(i==0?QString::number(values[i]/10.0,'f',1)+" EV":QString::number(values[i]));}
    const int degrees=editRotationDegrees(edits);QSignalBlocker block(rotation_);rotation_->setValue(degrees);rotationValue_->setText((degrees>0?"+":"")+QString::number(degrees)+"°");
}
bool EditPanel::eventFilter(QObject* target,QEvent* event){if(event->type()==QEvent::MouseButtonDblClick)if(auto* slider=qobject_cast<QSlider*>(target)){slider->setValue(0);return true;}return QWidget::eventFilter(target,event);}
}
