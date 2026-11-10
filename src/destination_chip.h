#pragma once
#include <QPushButton>
#include <QStylePainter>
#include <QStyleOptionButton>
namespace photocull {
class DestinationChip : public QPushButton {
public:
    DestinationChip(QString key,QString name,QWidget* parent):QPushButton((key.isEmpty()?QString():key+"  ")+name,parent),key_(std::move(key)),name_(std::move(name)){
        setProperty("destinationKey",key_);setAccessibleName(text());setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    }
    QSize sizeHint() const override {
        return {qBound(60,fontMetrics().horizontalAdvance(name_)+28+(key_.isEmpty()?0:keyWidth()+8),280),30};
    }
    QSize minimumSizeHint() const override{return sizeHint();}
protected:
    void paintEvent(QPaintEvent*) override {
        QStylePainter painter(this);QStyleOptionButton option;initStyleOption(&option);option.text.clear();painter.drawControl(QStyle::CE_PushButton,option);
        painter.setRenderHint(QPainter::Antialiasing);int left=12;
        const QColor textColor=palette().buttonText().color();
        if(!key_.isEmpty()){
            const QRectF box(8,5,keyWidth(),height()-10);painter.setPen(QPen(QColor(190,199,223,110),.8));painter.setBrush(QColor(15,15,22,38));painter.drawRoundedRect(box,3,3);
            auto small=font();small.setPixelSize(11);painter.setFont(small);painter.setPen(textColor);painter.drawText(box,Qt::AlignCenter,fontMetrics().elidedText(key_,Qt::ElideRight,keyWidth()-6));left=8+keyWidth()+8;
        }
        painter.setFont(font());painter.setPen(textColor);painter.drawText(QRect(left,0,width()-left-10,height()),Qt::AlignVCenter|Qt::AlignLeft,fontMetrics().elidedText(name_,Qt::ElideRight,width()-left-10));
    }
private:
    int keyWidth() const{return qBound(20,fontMetrics().horizontalAdvance(key_)+10,115);}
    QString key_,name_;
};
}
