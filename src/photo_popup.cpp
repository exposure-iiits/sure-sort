#include "photo_popup.h"
#include <QLabel>
#include <QScrollArea>
#include <QPushButton>
#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QRunnable>
#include <QKeyEvent>
#include <cmath>
#include <algorithm>
namespace photocull {
HistogramData photoHistogram(const QImage& source){
    HistogramData data;
    const auto image=source.convertToFormat(QImage::Format_ARGB32);
    for(int y=0;y<image.height();++y){const auto* row=reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for(int x=0;x<image.width();++x){const auto pixel=row[x];if(!qAlpha(pixel))continue;
            ++data.channels[0][qRed(pixel)];++data.channels[1][qGreen(pixel)];++data.channels[2][qBlue(pixel)];
            ++data.channels[3][qBound(0,qRound(.2126*qRed(pixel)+.7152*qGreen(pixel)+.0722*qBlue(pixel)),255)];++data.pixels;
        }
    }return data;
}
HistogramView::HistogramView(QWidget* parent):QWidget(parent){setObjectName("photoHistogram");setMinimumHeight(130);setAccessibleName("Photo histogram: red, green, blue and luminance");}
void HistogramView::setImage(const QImage& image){data_=photoHistogram(image);update();}
void HistogramView::paintEvent(QPaintEvent*){
    QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);
    const QRectF plot=QRectF(rect()).adjusted(2,6,-2,-22);
    painter.setPen(QColor(255,255,255,22));
    for(int i=0;i<=4;++i){const double x=plot.left()+plot.width()*i/4;painter.drawLine(QPointF(x,plot.top()),QPointF(x,plot.bottom()));}
    if(!data_.pixels){painter.setPen(QColor("#a5a5af"));painter.drawText(plot,Qt::AlignCenter,"No preview available");return;}
    quint32 maximum=1;for(const auto& channel:data_.channels)for(auto value:channel)maximum=qMax(maximum,value);
    const QColor colors[]={QColor(240,111,117,130),QColor(116,211,153,130),QColor(119,164,244,145),QColor(225,225,235,125)};
    for(int c=0;c<4;++c){QPainterPath path;path.moveTo(plot.bottomLeft());
        for(int i=0;i<256;++i)path.lineTo(plot.left()+plot.width()*i/255,plot.bottom()-plot.height()*std::sqrt(double(data_.channels[c][i])/maximum));
        path.lineTo(plot.bottomRight());path.closeSubpath();painter.setPen(QPen(colors[c].lighter(120),1));painter.setBrush(QColor(colors[c].red(),colors[c].green(),colors[c].blue(),c==3?25:55));painter.drawPath(path);
    }
    painter.setPen(QColor("#9b9ba6"));painter.drawText(QRectF(0,height()-19,width()/2,18),Qt::AlignLeft,"Shadows");painter.drawText(QRectF(width()/2, height()-19,width()/2,18),Qt::AlignRight,"Highlights");
}
PhotoPopup::PhotoPopup(QWidget* parent):QWidget(parent){
    setObjectName("photoPopup");setAttribute(Qt::WA_TranslucentBackground);setAutoFillBackground(false);setFocusPolicy(Qt::StrongFocus);
    setStyleSheet("QWidget#photoPopup, QWidget#photoPopup QWidget {background:transparent;}");
    auto* layout=new QVBoxLayout(this);layout->setContentsMargins(18,14,18,14);layout->setSpacing(10);
    auto* row=new QHBoxLayout;title_=new QLabel(this);title_->setStyleSheet("font-size:15px;font-weight:600;color:#eeeef1;");row->addWidget(title_,1);
    auto* close=new QPushButton(QString::fromUtf8("×"),this);close->setObjectName("closePhotoPopup");close->setAccessibleName("Close photo popup");close->setFixedSize(24,24);close->setStyleSheet("QPushButton {background:transparent;border:0;padding:0;font-size:20px;color:#aaaab5;} QPushButton:hover {color:white;background:#45454d;border-radius:6px;}");row->addWidget(close);layout->addLayout(row);
    connect(close,&QPushButton::clicked,this,[this]{hide();emit dismissed();});
    scroll_=new QScrollArea(this);scroll_->setWidgetResizable(true);scroll_->setFrameShape(QFrame::NoFrame);scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->setStyleSheet("QScrollArea, QScrollArea > QWidget > QWidget {background:transparent;border:0;}");
    fields_=new QWidget;fieldsLayout_=new QVBoxLayout(fields_);fieldsLayout_->setContentsMargins(0,0,6,0);fieldsLayout_->setSpacing(9);scroll_->setWidget(fields_);layout->addWidget(scroll_,1);
    histogram_=new HistogramView(this);layout->addWidget(histogram_,1);
    hint_=new QLabel(this);hint_->setWordWrap(true);hint_->setStyleSheet("color:#9999a5;font-size:11px;");layout->addWidget(hint_);
    pool_.setMaxThreadCount(1);pool_.setThreadPriority(QThread::LowPriority);setMode(Information);hide();
}
PhotoPopup::~PhotoPopup(){shutdown();}
void PhotoPopup::shutdown(){stopped_=true;++generation_;if(cancel_)cancel_->store(true);pool_.clear();pool_.waitForDone();}
void PhotoPopup::setMode(Mode mode){
    mode_=mode;title_->setText(mode==Information?"Photo information":"Histogram");scroll_->setVisible(mode==Information);histogram_->setVisible(mode==Histogram);
    hint_->setText(mode==Information?"Original file metadata · Unavailable fields are omitted.":"R · G · B · Luminance\nBased on the edited preview; RAW sensor data may differ.");
    reposition();if(mode==Information)loadInformation();
}
void PhotoPopup::setPhoto(const Photo& photo,QSize sourceSize){
    photo_=photo;sourceSize_=sourceSize;
    ++generation_;if(cancel_)cancel_->store(true);pool_.clear();
    if(mode_==Information&&isVisible())loadInformation();
}
void PhotoPopup::loadInformation(){
    if(stopped_)return;
    ++generation_;if(cancel_)cancel_->store(true);pool_.clear();
    if(photo_.sources.isEmpty()){showFields({{"Photo","Open a folder to view photo information"}});return;}
    showFields({{"Photo","Reading metadata…"}});
    cancel_=std::make_shared<std::atomic_bool>(false);
    const auto cancel=cancel_;const auto photo=photo_;const auto size=sourceSize_;const auto generation=generation_;
    pool_.start(QRunnable::create([this,cancel,photo,size,generation]{
        const auto fields=readPhotoInformation(photo,size,cancel);if(cancel->load())return;
        QMetaObject::invokeMethod(this,[this,generation,fields]{if(!stopped_&&generation==generation_)showFields(fields);},Qt::QueuedConnection);
    }));
}
void PhotoPopup::showFields(const PhotoInformation& fields){
    while(auto* item=fieldsLayout_->takeAt(0)){delete item->widget();delete item;}
    for(const auto& field:fields){
        auto* row=new QWidget(fields_);auto* rowLayout=new QHBoxLayout(row);rowLayout->setContentsMargins(0,0,0,0);rowLayout->setSpacing(10);
        auto* label=new QLabel(field.first,row);label->setFixedWidth(78);label->setWordWrap(true);label->setAlignment(Qt::AlignLeft|Qt::AlignTop);label->setStyleSheet("color:#9595a1;font-size:11px;");rowLayout->addWidget(label);
        auto* value=new QLabel(field.second,row);value->setObjectName("photoInfoValue");value->setProperty("field",field.first);value->setWordWrap(true);value->setTextFormat(Qt::PlainText);value->setTextInteractionFlags(Qt::TextSelectableByMouse);value->setStyleSheet("color:#e0e0e7;font-size:12px;");value->setMinimumWidth(0);value->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);rowLayout->addWidget(value,1);fieldsLayout_->addWidget(row);
    }fieldsLayout_->addStretch();
}
void PhotoPopup::setHistogramImage(const QImage& image){histogram_->setImage(image);}
void PhotoPopup::reposition(){
    if(!parentWidget())return;
    if(property("layoutManaged").toBool()){const int width=property("cardWidth").isValid()?property("cardWidth").toInt():336;setFixedSize(width,mode_==Information?510:260);return;}
    const int width=qMin(336,qMax(160,parentWidget()->width()-24));
    const int height=qMin(mode_==Information?510:260,qMax(100,parentWidget()->height()-24));
    setGeometry(parentWidget()->width()-width-12,12,width,height);raise();
}
void PhotoPopup::paintEvent(QPaintEvent*){QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);painter.setPen(QPen(QColor(255,255,255,42),1));painter.setBrush(QColor(30,30,35,235));painter.drawRoundedRect(QRectF(rect()).adjusted(.5,.5,-.5,-.5),12,12);}
void PhotoPopup::keyPressEvent(QKeyEvent* event){if(event->key()==Qt::Key_Escape){hide();emit dismissed();event->accept();return;}QWidget::keyPressEvent(event);}
}
