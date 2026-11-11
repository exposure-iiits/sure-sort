#pragma once
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <cmath>
namespace photocull {
inline QIcon uiIcon(const QString& name){
    QPixmap pixmap(48,48);pixmap.fill(Qt::transparent);pixmap.setDevicePixelRatio(2);QPainter p(&pixmap);p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor("#d4d4d8"),1.7,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
    if(name=="settings"){
        for(int i=0;i<8;++i){const double a=i*3.141592653589793/4;p.drawLine(QPointF(12+7.5*std::cos(a),12+7.5*std::sin(a)),QPointF(12+10*std::cos(a),12+10*std::sin(a)));}
        p.drawEllipse(QPointF(12,12),7.5,7.5);p.drawEllipse(QPointF(12,12),3,3);
    }else if(name=="info"){
        p.drawEllipse(QPointF(12,12),9,9);p.drawPoint(12,7);p.drawLine(12,11,12,17);
    }else if(name=="histogram"){
        p.drawLine(3,20,21,20);p.drawLine(3,20,3,4);
        p.drawLine(7,19,7,13);p.drawLine(11,19,11,7);p.drawLine(15,19,15,10);p.drawLine(19,19,19,5);
    }else if(name=="keyboard"){
        p.drawRoundedRect(QRectF(2,5,20,14),3,3);for(int y:{9,12})for(int x:{6,10,14,18})p.drawPoint(x,y);p.drawLine(8,16,16,16);
    }else if(name=="plus"){
        p.drawLine(12,5,12,19);p.drawLine(5,12,19,12);
    }else if(name=="edit"){
        QPainterPath path;path.moveTo(4,16);path.lineTo(15,5);path.lineTo(19,9);path.lineTo(8,20);path.lineTo(3,21);path.closeSubpath();p.drawPath(path);
        p.drawLine(12,8,16,12);p.drawLine(4,16,8,20);
    }else if(name=="trash"){
        p.drawLine(4,6,20,6);p.drawLine(9,3,15,3);p.drawLine(6,6,7,21);p.drawLine(7,21,17,21);p.drawLine(17,21,18,6);p.drawLine(10,10,10,17);p.drawLine(14,10,14,17);
    }else if(name=="reset"){
        QPainterPath path;path.moveTo(5,8);path.cubicTo(9,0,22,5,21,13);path.cubicTo(20,23,5,23,3,14);p.drawPath(path);p.drawLine(5,3,5,8);p.drawLine(5,8,10,8);
    }else if(name=="folder"){
        QPainterPath path;path.moveTo(3,7);path.lineTo(3,5);path.lineTo(9,5);path.lineTo(11,7);path.lineTo(21,7);path.lineTo(21,19);path.lineTo(3,19);path.closeSubpath();p.drawPath(path);p.drawLine(3,10,21,10);
    }else if(name=="exitFullscreen"){
        for(int sx:{-1,1})for(int sy:{-1,1}){const QPointF c(12+sx*3,12+sy*3);p.drawLine(c,c+QPointF(sx*6,0));p.drawLine(c,c+QPointF(0,sy*6));}
    }else{
        for(int sx:{-1,1})for(int sy:{-1,1}){const QPointF c(12+sx*9,12+sy*9);p.drawLine(c,c-QPointF(sx*6,0));p.drawLine(c,c-QPointF(0,sy*6));}
    }return QIcon(pixmap);
}
}
