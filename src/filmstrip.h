#pragma once
#include "image_decoder.h"
#include <QAbstractScrollArea>
#include <QCache>
#include <QThreadPool>
#include <functional>
namespace photocull {
class Filmstrip : public QAbstractScrollArea {
    Q_OBJECT
public:
    explicit Filmstrip(QWidget* parent=nullptr);
    ~Filmstrip() override;
    void setCatalog(Catalog photos);
    void setCurrentIndex(int index);
    int currentIndex() const {return current_;}
    void setVertical(bool vertical);
    void setCompletionResolver(std::function<bool(const QString&)> resolver){completionResolver_=std::move(resolver);viewport()->update();}
    void refreshCompletion(){viewport()->update();}
    void invalidate(const QString& id);
    void shutdown();
signals:
    void photoSelected(int index);
protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void showEvent(QShowEvent*) override;
    void hideEvent(QHideEvent*) override;
private:
    void updateRange();
    void pump();
    QScrollBar* scrollBar() const;
    int extent() const;
    Catalog photos_;
    int current_=-1;
    QCache<QString,QImage> cache_{12*1024};
    QThreadPool pool_;
    CancelToken cancel_;
    quint64 generation_=0;
    bool active_=false,stopped_=false;
    bool vertical_=false;
    std::function<bool(const QString&)> completionResolver_;
};
}
