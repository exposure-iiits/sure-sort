#pragma once
#include "photo_information.h"
#include <QWidget>
#include <QThreadPool>
#include <QImage>
#include <array>
class QLabel;
class QScrollArea;
class QVBoxLayout;
namespace photocull {
struct HistogramData {
    std::array<std::array<quint32,256>,4> channels{}; // R, G, B, luminance
    quint64 pixels=0;
};
HistogramData photoHistogram(const QImage& image);
class HistogramView : public QWidget {
public:
    explicit HistogramView(QWidget* parent=nullptr);
    void setImage(const QImage& image);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    HistogramData data_;
};
class PhotoPopup : public QWidget {
    Q_OBJECT
public:
    enum Mode { Information, Histogram };
    explicit PhotoPopup(QWidget* parent);
    ~PhotoPopup() override;
    void setMode(Mode mode);
    Mode mode() const {return mode_;}
    void setPhoto(const Photo& photo, QSize sourceSize={});
    void setHistogramImage(const QImage& image);
    void reposition();
    void shutdown();
signals:
    void dismissed();
protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
private:
    void loadInformation();
    void showFields(const PhotoInformation& fields);
    Mode mode_=Information;
    Photo photo_;
    QSize sourceSize_;
    QLabel *title_,*hint_;
    QScrollArea* scroll_;
    QWidget* fields_;
    QVBoxLayout* fieldsLayout_;
    HistogramView* histogram_;
    QThreadPool pool_;
    CancelToken cancel_;
    quint64 generation_=0;
    bool stopped_=false;
};
}
