#pragma once
#include "watermark.h"
#include <QDialog>
#include <QThreadPool>
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;
namespace photocull {
class WatermarkDialog : public QDialog {
    Q_OBJECT
public:
    explicit WatermarkDialog(WatermarkSnapshot snapshot,QWidget* parent=nullptr);
    ~WatermarkDialog() override;
    const WatermarkSnapshot& snapshot() const {return snapshot_;}
    void selectPng(const QString& path);
    void showError(const QString& error);
signals:
    void changed(photocull::WatermarkSnapshot snapshot);
    void previewInteractionChanged(bool adjusting);
protected:
    bool eventFilter(QObject* target,QEvent* event) override;
private:
    void controlsChanged();
    void syncControls();
    WatermarkSnapshot snapshot_;
    QCheckBox* enabled_;
    QComboBox* position_;
    QSlider *size_=nullptr,*opacity_=nullptr,*padding_=nullptr;
    QLabel *sizeValue_,*opacityValue_,*paddingValue_,*fileLabel_,*note_;
    QPushButton* select_;
    QWidget* options_;
    QThreadPool importPool_;
    quint64 generation_=0;
};
}
