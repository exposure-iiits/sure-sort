#pragma once
#include <QDialog>
#include <QPointer>

class QTableWidget;
class QTabBar;
class QLabel;
class QTimer;
namespace photocull {
class ShortcutManager;
class SortingController;

class ShortcutsDialog : public QDialog {
    Q_OBJECT
public:
    enum Category { Navigation, Viewing, Sorting, Editing, Application };
    ShortcutsDialog(ShortcutManager* shortcuts, SortingController* sorting, QWidget* parent);
    void showCategory(Category category);
protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* target, QEvent* event) override;
private:
    void populate();
    void cancelCapture();
    void beginCapture(int row, int column);
    void saveShortcut(const QString& kind, const QString& action, int slot, const QString& key, const QString& hold);
    void showToast(const QString& message);
    ShortcutManager* shortcuts_;
    SortingController* sorting_;
    QTableWidget* table_;
    QTabBar* categories_;
    QLabel* toast_;
    QTimer* toastTimer_;
    QPointer<QWidget> capture_;
    int captureRow_ = -1, captureColumn_ = -1;
};
}
