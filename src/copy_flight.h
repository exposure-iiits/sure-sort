#pragma once
#include <QImage>
#include <QRectF>
#include <QWidget>

namespace photocull {
// A bounded, mouse-transparent copy of the visible photograph, drawn as a flowing sheet.
class CopyFlight : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal progress READ progress WRITE setProgress)
public:
    CopyFlight(QImage image,QRectF source,QRectF destination,QWidget* parent);
    qreal progress() const {return progress_;}
    void setProgress(qreal value);
    const QImage& snapshot() const {return image_;}
    QRectF sourceRect() const {return source_;}
    QRectF destinationRect() const {return destination_;}
protected:
    void paintEvent(QPaintEvent*) override;
private:
    struct Row {QPointF left,right;};
    Row row(qreal position) const;
    QImage image_;
    QRectF source_,destination_;
    qreal progress_=0;
};
}
