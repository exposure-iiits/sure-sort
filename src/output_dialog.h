#pragma once
#include "output_settings.h"
#include <QDialog>
namespace photocull {
class OutputDialog : public QDialog {
    Q_OBJECT
public:
    explicit OutputDialog(OutputSettings,QWidget* parent=nullptr);
signals:
    void changed(photocull::OutputSettings);
};
}
