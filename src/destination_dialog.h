#pragma once
#include "sorting_types.h"
#include <QDialog>
class QLineEdit;
class QTableWidget;
namespace photocull {
class ShortcutManager;
class DestinationDialog : public QDialog {
    Q_OBJECT
public:
    DestinationDialog(const DestinationConfig&,ShortcutManager* manager,QWidget* parent=nullptr);
    DestinationConfig configuration() const;
private:
    void addFolder(const Destination&);
    QLineEdit* root_;
    QTableWidget* table_;
};
}
