#include "filmstrip.h"
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QRunnable>
namespace photocull {
namespace {constexpr int tile=96;constexpr int inset=8;}
Filmstrip::Filmstrip(QWidget* parent):QAbstractScrollArea(parent){
    setObjectName("photoFilmstrip");setAccessibleName("Photo thumbnails");setFixedHeight(72);setMinimumWidth(0);setFrameShape(QFrame::NoFrame);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);setFocusPolicy(Qt::NoFocus);viewport()->setMouseTracking(true);
    pool_.setMaxThreadCount(1);pool_.setThreadPriority(QThread::LowPriority);
    connect(horizontalScrollBar(),&QScrollBar::valueChanged,this,[this]{viewport()->update();pump();});
    connect(verticalScrollBar(),&QScrollBar::valueChanged,this,[this]{viewport()->update();pump();});
}
QScrollBar* Filmstrip::scrollBar() const{return vertical_?verticalScrollBar():horizontalScrollBar();}
int Filmstrip::extent() const{return vertical_?viewport()->height():viewport()->width();}
void Filmstrip::setVertical(bool vertical){
    if(vertical_==vertical)return;
    vertical_=vertical;setMinimumSize(0,0);setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);
    if(vertical_)setFixedWidth(96);else setFixedHeight(72);
    horizontalScrollBar()->setValue(0);verticalScrollBar()->setValue(0);updateRange();setCurrentIndex(current_);
}
Filmstrip::~Filmstrip(){shutdown();}
void Filmstrip::shutdown(){stopped_=true;++generation_;if(cancel_)cancel_->store(true);pool_.clear();pool_.waitForDone();}
void Filmstrip::setCatalog(Catalog photos){
    ++generation_;if(cancel_)cancel_->store(true);pool_.clear();active_=false;cache_.clear();photos_=std::move(photos);current_=-1;
    scrollBar()->setValue(0);updateRange();viewport()->update();pump();
}
void Filmstrip::setCurrentIndex(int index){
    current_=index;
    if(index>=0){const int left=index*tile;const int offset=scrollBar()->value();
        if(left<offset)scrollBar()->setValue(left);
        else if(left+tile>offset+extent())scrollBar()->setValue(left+tile-extent());
    }viewport()->update();pump();
}
void Filmstrip::invalidate(const QString& id){++generation_;if(cancel_)cancel_->store(true);pool_.clear();active_=false;cache_.remove(id);viewport()->update();pump();}
void Filmstrip::updateRange(){scrollBar()->setPageStep(extent());scrollBar()->setSingleStep(tile);scrollBar()->setRange(0,qMax(0,int(photos_.size())*tile-extent()));}
void Filmstrip::resizeEvent(QResizeEvent* event){QAbstractScrollArea::resizeEvent(event);updateRange();pump();}
void Filmstrip::showEvent(QShowEvent* event){QAbstractScrollArea::showEvent(event);updateRange();setCurrentIndex(current_);pump();}
void Filmstrip::hideEvent(QHideEvent* event){if(cancel_)cancel_->store(true);QAbstractScrollArea::hideEvent(event);}
void Filmstrip::paintEvent(QPaintEvent*){
    QPainter painter(viewport());painter.fillRect(viewport()->rect(),QColor("#19191c"));painter.setRenderHint(QPainter::Antialiasing);painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const int offset=scrollBar()->value();const int first=offset/tile,last=qMin(int(photos_.size())-1,(offset+extent())/tile);
    for(int i=first;i<=last;++i){const QRectF frame=vertical_?QRectF(inset,i*tile-offset+8,80,80):QRectF(i*tile-offset+inset,8,tile-2*inset,56);
        const bool completed=completionResolver_&&completionResolver_(photos_[i].id);
        if(completed&&i==current_){painter.setBrush(Qt::NoBrush);painter.setPen(QPen(QColor("#afcbee"),1));painter.drawRoundedRect(frame.adjusted(-3,-3,3,3),7,7);}
        painter.setPen(QPen(completed?QColor("#7bd99b"):i==current_?QColor("#afcbee"):QColor("#3c3c45"),completed||i==current_?2:1));painter.setBrush(QColor("#242428"));painter.drawRoundedRect(frame,5,5);
        if(const auto* image=cache_.object(photos_[i].id);image&&!image->isNull()){
            const auto size=image->size().scaled(frame.size().toSize()-QSize(6,6),Qt::KeepAspectRatio);const QRectF target(frame.center()-QPointF(size.width()/2.0,size.height()/2.0),size);painter.drawImage(target,*image);
        }else {painter.setPen(QColor("#686873"));painter.drawText(frame,Qt::AlignCenter,"· · ·");}
    }
    if(photos_.isEmpty()){painter.setPen(QColor("#777782"));painter.drawText(viewport()->rect(),Qt::AlignCenter,"Photos appear here when you open a folder");}
}
void Filmstrip::pump(){
    if(stopped_||active_||!isVisible()||photos_.isEmpty())return;
    const int first=scrollBar()->value()/tile,last=qMin(int(photos_.size())-1,(scrollBar()->value()+extent())/tile);
    int next=-1;
    if(current_>=first&&current_<=last&&!cache_.contains(photos_[current_].id))next=current_;
    else for(int i=first;i<=last;++i)if(!cache_.contains(photos_[i].id)){next=i;break;}
    if(next<0)return;
    auto photo=photos_[next];const auto id=photo.id;
    cancel_=std::make_shared<std::atomic_bool>(false);const auto cancel=cancel_;const auto generation=generation_;active_=true;
    pool_.start(QRunnable::create([this,photo,id,cancel,generation]{
        auto result=ImageDecoder::decode({photo,{192,128},false,false},cancel);
        QMetaObject::invokeMethod(this,[this,id,cancel,generation,image=result.image]{
            if(stopped_||generation!=generation_)return;
            active_=false;
            if(!cancel->load()){const int cost=qMax(1,int(image.sizeInBytes()/1024));cache_.insert(id,new QImage(image),cost);viewport()->update();}
            pump();
        },Qt::QueuedConnection);
    }));
}
void Filmstrip::mousePressEvent(QMouseEvent* event){
    if(event->button()==Qt::LeftButton){const int index=(scrollBar()->value()+qRound(vertical_?event->position().y():event->position().x()))/tile;if(index>=0&&index<photos_.size())emit photoSelected(index);event->accept();return;}QAbstractScrollArea::mousePressEvent(event);
}
void Filmstrip::mouseMoveEvent(QMouseEvent* event){QAbstractScrollArea::mouseMoveEvent(event);}
void Filmstrip::wheelEvent(QWheelEvent* event){const auto pixel=event->pixelDelta(),angle=event->angleDelta();const int delta=!pixel.isNull()?(pixel.x()?pixel.x():pixel.y()):(angle.x()?angle.x():angle.y());scrollBar()->setValue(scrollBar()->value()-delta);event->accept();}
}
