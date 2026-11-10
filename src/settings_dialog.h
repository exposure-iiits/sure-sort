#pragma once
#include <QDialog>

class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;

namespace photocull {
class DestinationDialog;
class ShortcutManager;
class SortingController;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    enum Page { Sorting, Watermark, Editing, Saving, Viewing, About };
    SettingsDialog(SortingController* sorting, ShortcutManager* shortcuts, QWidget* parent = nullptr);
    void showPage(Page page);
signals:
    void editingRequested();
    void viewerPreferencesChanged();
private:
    void selectPage(int page);
    void saveCurrentPage();
    SortingController* sorting_;
    ShortcutManager* shortcuts_;
    QListWidget* sidebar_;
    QStackedWidget* pages_;
    QLabel *title_, *description_, *feedback_;
    QPushButton* save_;
    DestinationDialog* destinations_;
};
}
