#include "output_dialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
namespace photocull {
OutputDialog::OutputDialog(OutputSettings s,QWidget* parent):QDialog(parent){
    setWindowTitle("Output Settings");setMinimumWidth(350);
    auto* layout=new QVBoxLayout(this);auto* form=new QFormLayout;layout->addLayout(form);
    auto* quality=new QSlider(Qt::Horizontal,this);quality->setObjectName("jpegQuality");quality->setRange(80,100);quality->setValue(s.jpegQuality);
    auto* value=new QLabel(QString::number(s.jpegQuality),this);auto* row=new QHBoxLayout;row->addWidget(quality);row->addWidget(value);form->addRow("JPEG quality",row);
    auto* resolution=new QComboBox(this);resolution->setObjectName("outputResolution");resolution->addItem("Original",100);resolution->addItem("75%",75);resolution->addItem("50%",50);resolution->setCurrentIndex(resolution->findData(s.resolution));form->addRow("Resolution",resolution);
    auto* metadata=new QCheckBox("Preserve EXIF where possible",this);metadata->setObjectName("preserveMetadata");metadata->setChecked(s.preserveMetadata);form->addRow("Metadata",metadata);
    form->addRow("Output format",new QLabel("RAW → JPEG · JPEG / PNG keep their format",this));
    auto* note=new QLabel("Saved automatically for future assignments. Existing queued photos keep their captured settings.",this);note->setWordWrap(true);layout->addWidget(note);
    auto notify=[this,quality,resolution,metadata,value]{value->setText(QString::number(quality->value()));emit changed({quality->value(),resolution->currentData().toInt(),metadata->isChecked()});};
    connect(quality,&QSlider::valueChanged,this,notify);connect(resolution,&QComboBox::currentIndexChanged,this,notify);connect(metadata,&QCheckBox::toggled,this,notify);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,this);connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::close);layout->addWidget(buttons);
}
}
