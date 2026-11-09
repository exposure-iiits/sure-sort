#pragma once
#include "photo_edits.h"
#include <QWidget>
class QSlider;
class QLabel;
class QComboBox;
class QPushButton;
namespace photocull {
class ShortcutManager;
class EditPanel:public QWidget {
    Q_OBJECT
public:
    explicit EditPanel(QWidget* parent=nullptr);
    void setEdits(const PhotoEdits& edits);
signals:
    void changed(photocull::PhotoEdits edits);
    void cropRequested();
    void cropAspectChanged(double ratio);
    void shortcutsRequested();
protected:
    bool eventFilter(QObject*,QEvent*) override;
private:
    PhotoEdits edits_;
    QVector<QSlider*> sliders_;
    QVector<QLabel*> values_;
    QSlider* rotation_;
    QLabel* rotationValue_;
};
}
