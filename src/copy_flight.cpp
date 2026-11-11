#include "copy_flight.h"
#include <QPainter>
#include <QPainterPath>
#include <QTransform>
#include <cmath>

namespace photocull {
namespace {
qreal smooth(qreal t){t=qBound(qreal(0),t,qreal(1));return t*t*(3-2*t);}
QPointF bezier(QPointF a,QPointF b,QPointF c,QPointF d,qreal t){const auto u=1-t;return a*(u*u*u)+b*(3*u*u*t)+c*(3*u*t*t)+d*(t*t*t);}
}
CopyFlight::CopyFlight(QImage image,QRectF source,QRectF destination,QWidget* parent)
    :QWidget(parent),image_(std::move(image)),source_(source),destination_(destination){
    setObjectName("copyFlight");setAttribute(Qt::WA_TransparentForMouseEvents);setAttribute(Qt::WA_TranslucentBackground);setAutoFillBackground(false);setFocusPolicy(Qt::NoFocus);
    setStyleSheet("QWidget#copyFlight {background:transparent;border:0;}");
    if(parent)setGeometry(parent->rect());
}
void CopyFlight::setProgress(qreal value){progress_=qBound(qreal(0),value,qreal(1));update();}
CopyFlight::Row CopyFlight::row(qreal position) const {
    // The edge nearest the folder starts first, pulling the rest of the image into a funnel.
    const qreal delay=.18*(1-position);
    const qreal t=smooth((progress_-delay)/(1-delay));
    const QPointF start(source_.center().x(),source_.top()+source_.height()*position);
    const QPointF end(destination_.center().x(),destination_.top()+destination_.height()*position);
    const qreal dx=end.x()-start.x(),arc=qBound(qreal(64),source_.height()*.25,qreal(160));
    const auto center=bezier(start,start+QPointF(dx*.12,0),end-QPointF(dx*.08,arc),end,t);
    const qreal contraction=smooth(qBound(qreal(0),(progress_-delay)/(1-delay)*1.65,qreal(1)));
    const qreal width=source_.width()*(1-contraction)+destination_.width()*contraction;
    return {center-QPointF(width/2,0),center+QPointF(width/2,0)};
}
void CopyFlight::paintEvent(QPaintEvent*){
    if(image_.isNull()||!source_.isValid()||!destination_.isValid()||progress_>=1)return;
    constexpr int bands=64;
    QPainter painter(this);painter.setRenderHint(QPainter::SmoothPixmapTransform);painter.setRenderHint(QPainter::Antialiasing);
    painter.setOpacity(1-smooth((progress_-.88)/.12));
    QPainterPath outline;outline.moveTo(row(0).left);
    for(int i=1;i<=bands;++i)outline.lineTo(row(qreal(i)/bands).left);
    for(int i=bands;i>=0;--i)outline.lineTo(row(qreal(i)/bands).right);
    outline.closeSubpath();
    painter.setPen(QPen(QColor(0,0,0,45),6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));painter.setBrush(QColor("#161617"));painter.drawPath(outline);
    painter.setPen(Qt::NoPen);painter.setRenderHint(QPainter::Antialiasing,false);
    for(int i=0;i<bands;++i){
        const auto top=row(qreal(i)/bands),bottom=row(qreal(i+1)/bands);
        const qreal y=image_.height()*qreal(i)/bands,next=image_.height()*qreal(i+1)/bands;
        const QRectF strip(0,y,image_.width(),next-y);
        const QPolygonF from{strip.topLeft(),strip.topRight(),strip.bottomRight(),strip.bottomLeft()},to{top.left,top.right,bottom.right,bottom.left};
        QTransform warp;if(!QTransform::quadToQuad(from,to,warp))continue;
        painter.save();painter.setTransform(warp,true);painter.drawImage(strip,image_,strip);painter.restore();
    }
}
}
