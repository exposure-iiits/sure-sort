#include "watermark_dialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPushButton>
#include <QRunnable>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QWheelEvent>
#include <QVBoxLayout>

namespace photocull {
WatermarkDialog::WatermarkDialog(WatermarkSnapshot snapshot,QWidget* parent):QDialog(parent),snapshot_(std::move(snapshot)) {
    setWindowTitle("Watermark");setModal(false);setMinimumWidth(380);
    auto* layout=new QVBoxLayout(this);
    enabled_=new QCheckBox("Enable watermark",this);enabled_->setObjectName("watermarkEnabled");layout->addWidget(enabled_);
    options_=new QWidget(this);options_->setObjectName("watermarkOptions");
    auto* optionsLayout=new QVBoxLayout(options_);optionsLayout->setContentsMargins(0,0,0,0);layout->addWidget(options_);
    select_=new QPushButton("Select PNG…",this);select_->setObjectName("selectWatermark");
    auto* fileRow=new QHBoxLayout;fileRow->addWidget(select_);
    fileLabel_=new QLabel(this);fileLabel_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);fileRow->addWidget(fileLabel_,1);optionsLayout->addLayout(fileRow);
    auto* form=new QFormLayout;
    position_=new QComboBox(this);position_->setObjectName("watermarkPosition");
    position_->addItems({"Top Left","Top Right","Bottom Left","Bottom Right"});form->addRow("Position",position_);
    auto slider=[&](const QString& title,const QString& name,int minimum,int maximum,QSlider*& control,QLabel*& label){
        auto* row=new QHBoxLayout;control=new QSlider(Qt::Horizontal,this);control->setObjectName(name);control->setRange(minimum,maximum);
        label=new QLabel(this);label->setMinimumWidth(38);row->addWidget(control,1);row->addWidget(label);form->addRow(title,row);
        control->installEventFilter(this);
        connect(control,&QSlider::valueChanged,this,[this]{controlsChanged();});
    };
    slider("Size","watermarkSize",1,100,size_,sizeValue_);
    slider("Opacity","watermarkOpacity",0,100,opacity_,opacityValue_);
    slider("Padding","watermarkPadding",0,25,padding_,paddingValue_);
    size_->setToolTip("Logo’s longest edge as a percentage of the photo’s shorter side. Automatically constrained to fit.");
    padding_->setToolTip("Distance from photo edges as a percentage of the photo’s shorter side.");
    optionsLayout->addLayout(form);
    auto* hint=new QLabel("Size and padding use the photo’s shorter side.\nChanges preview and save automatically.",this);
    hint->setWordWrap(true);optionsLayout->addWidget(hint);
    note_=new QLabel(this);note_->setWordWrap(true);note_->hide();optionsLayout->addWidget(note_);
    auto* close=new QDialogButtonBox(QDialogButtonBox::Close,this);layout->addWidget(close);
    connect(close,&QDialogButtonBox::rejected,this,&QDialog::close);
    connect(enabled_,&QCheckBox::toggled,this,[this](bool enabled){options_->setVisible(enabled);controlsChanged();});
    connect(position_,&QComboBox::currentIndexChanged,this,[this]{controlsChanged();});
    connect(select_,&QPushButton::clicked,this,[this]{
        const auto path=QFileDialog::getOpenFileName(this,"Select watermark PNG",{},"PNG images (*.png)");if(!path.isEmpty())selectPng(path);
    });
    importPool_.setMaxThreadCount(1);syncControls();
    if(snapshot_.settings.enabled&&!snapshot_.asset)showError("Saved PNG unavailable. Select a PNG or disable watermarking.");
}
WatermarkDialog::~WatermarkDialog(){++generation_;importPool_.clear();importPool_.waitForDone();}
bool WatermarkDialog::eventFilter(QObject* target,QEvent* event){
    if(target==size_||target==opacity_||target==padding_){
        auto adjustmentKey=[](int key){
            return key==Qt::Key_Left||key==Qt::Key_Right||key==Qt::Key_Up||key==Qt::Key_Down||
                   key==Qt::Key_PageUp||key==Qt::Key_PageDown||key==Qt::Key_Home||key==Qt::Key_End;
        };
        switch(event->type()){
        case QEvent::MouseButtonPress:
            if(static_cast<QMouseEvent*>(event)->button()==Qt::LeftButton)emit previewInteractionChanged(true);
            break;
        case QEvent::MouseButtonRelease:
            if(static_cast<QMouseEvent*>(event)->button()==Qt::LeftButton)emit previewInteractionChanged(false);
            break;
        case QEvent::KeyPress: {
            if(adjustmentKey(static_cast<QKeyEvent*>(event)->key()))emit previewInteractionChanged(true);
            break;
        }
        case QEvent::KeyRelease: {
            const auto* key=static_cast<QKeyEvent*>(event);
            if(!key->isAutoRepeat()&&adjustmentKey(key->key()))emit previewInteractionChanged(false);
            break;
        }
        case QEvent::Wheel:
            emit previewInteractionChanged(static_cast<QWheelEvent*>(event)->phase()!=Qt::ScrollEnd);
            if(static_cast<QWheelEvent*>(event)->phase()==Qt::NoScrollPhase)
                QTimer::singleShot(0,this,[this]{emit previewInteractionChanged(false);});
            break;
        case QEvent::FocusOut:
        case QEvent::Hide:
            emit previewInteractionChanged(false);break;
        default:break;
        }
    }
    return QDialog::eventFilter(target,event);
}
void WatermarkDialog::showError(const QString& error){note_->setText(error);note_->setStyleSheet("color:#edbc81;");note_->show();}
void WatermarkDialog::syncControls(){
    const QSignalBlocker a(enabled_),b(position_),c(size_),d(opacity_),e(padding_);
    enabled_->setChecked(snapshot_.settings.enabled);position_->setCurrentIndex(int(snapshot_.settings.position));
    options_->setVisible(enabled_->isChecked());
    size_->setValue(snapshot_.settings.sizePercent);opacity_->setValue(snapshot_.settings.opacityPercent);padding_->setValue(snapshot_.settings.paddingPercent);
    sizeValue_->setText(QString::number(size_->value())+'%');opacityValue_->setText(QString::number(opacity_->value())+'%');paddingValue_->setText(QString::number(padding_->value())+'%');
    fileLabel_->setText(snapshot_.asset?snapshot_.asset->name:"No PNG selected");fileLabel_->setToolTip(snapshot_.asset?snapshot_.asset->name:QString());
}
void WatermarkDialog::controlsChanged(){
    auto next=snapshot_;next.settings.enabled=enabled_->isChecked();next.settings.position=WatermarkPosition(position_->currentIndex());
    next.settings.sizePercent=size_->value();next.settings.opacityPercent=opacity_->value();next.settings.paddingPercent=padding_->value();
    const auto error=validateWatermark(next);
    if(!error.isEmpty()){
        showError(error);
        // Keep PNG selection available when enabling watermark for the first time.
        if(next.settings.enabled&&!next.asset)return;
        syncControls();return;
    }
    snapshot_=next;note_->clear();note_->hide();syncControls();emit changed(snapshot_);
}
void WatermarkDialog::selectPng(const QString& path){
    const quint64 generation=++generation_;select_->setEnabled(false);note_->setStyleSheet({});note_->setText("Reading PNG…");note_->show();
    importPool_.start(QRunnable::create([this,path,generation]{
        QString error;std::shared_ptr<const WatermarkAsset> asset;
        try{asset=importWatermarkPng(path,error);}catch(...){error="Not enough memory or an error reading this PNG.";}
        QMetaObject::invokeMethod(this,[this,generation,asset=std::move(asset),error]{
            if(generation!=generation_)return;
            select_->setEnabled(true);if(!asset){showError(error);return;}
            snapshot_.asset=asset;snapshot_.assetId=asset->id;snapshot_.settings.enabled=enabled_->isChecked();
            note_->clear();note_->hide();syncControls();emit changed(snapshot_);
        },Qt::QueuedConnection);
    }));
}
}
