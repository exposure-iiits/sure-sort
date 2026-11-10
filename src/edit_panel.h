#pragma once
#include "photo_edits.h"
#include <QWidget>
#include <QJsonArray>
class QDoubleSpinBox;
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
    void setShortcutBindings(const QJsonArray& bindings);
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
    QVector<QDoubleSpinBox*> values_;
    QHash<QString,QLabel*> shortcutHints_;
    QSlider* rotation_;
    QDoubleSpinBox* rotationValue_;
};
}
