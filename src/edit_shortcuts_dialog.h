#pragma once
#include <QDialog>
#include <QJsonArray>
class QTableWidget;
class QLabel;
namespace photocull {
class ShortcutManager;
class EditShortcutsDialog:public QDialog {
    Q_OBJECT
public:
    explicit EditShortcutsDialog(ShortcutManager* manager,QWidget* parent=nullptr);
    QJsonArray bindings() const;
private:
    void populate(const QJsonArray& bindings);
    ShortcutManager* manager_;
    QTableWidget* table_;
    QLabel* error_;
};
}
