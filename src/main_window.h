#pragma once
#include "folder_scanner.h"
#include "preview_controller.h"
#include "photo_viewer.h"
#include "sorting_controller.h"
#include <QMainWindow>
#include <QTimer>
#include <QPointer>
class QLabel;
class QPushButton;
class QHBoxLayout;
class QStackedWidget;
class QScrollArea;
namespace photocull {
class ShortcutManager;
class WatermarkDialog;
class EditPanel;
class OutputDialog;
class SettingsDialog;
class DestinationCounter;
class ShortcutsDialog;
class PhotoPopup;
class Filmstrip;
class CopyFlight;
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(qint64 cacheBudget, QWidget* parent = nullptr, QString databasePath = {});
    ~MainWindow() override;
    void openFolder(const QString& folder);
    void shutdown();
private:
    bool eventFilter(QObject* target, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void editDestination(const QString& id = {});
    void cancelDestinationEdit();
    void destinationMenu(const QString& id, const QPoint& position);
    void updateResetButton();
    void chooseFolder();
    void navigate(int index);
    void toggleDestination(const QString& id);
    void animateCopy(const QImage& snapshot,const QRect& source,const QString& photoId,const QString& destinationId);
    void stopCopyAnimation();
    void updateStatus(const DecodedImage* result = nullptr);
    void syncSavedDestinations();
    void toggleFullscreen();
    void configureDestinations();
    void configureWatermark();
    void configureOutput();
    void showShortcuts();
    void applyEdits(const PhotoEdits& edits);
    void editingAction(const QString& action);
    void configureEditingShortcuts();
    void rebuildDestinations();
    void updateQueue();
    void openSettings(int page = 0);
    void togglePhotoPopup(bool histogram);
    void layoutPhotoTools();
    void updatePhotoPopup();
    void applyViewerPreferences();
    FolderScanner scanner_;
    PreviewController controller_;
    SortingController sorting_;
    ShortcutManager* shortcuts_;
    quint64 folderEpoch_ = 0;
    PhotoViewer* viewer_;
    QLabel *folderLabel_, *outputLabel_, *filenameLabel_, *positionLabel_, *dimensionsLabel_, *typeLabel_, *zoomLabel_;
    QTimer resizeTimer_;
    QTimer positionTimer_;
    QPointer<CopyFlight> copyFlight_;
    QLabel* progressNotice_;
    QPushButton* queueButton_;
    QPushButton* undoButton_;
    QPushButton* resetButton_;
    bool wasMaximized_ = false;
    bool processingDetails_ = false;
    QWidget *topBar_, *cropBar_;
    QLabel* errorLabel_;
    QHBoxLayout* destinationsLayout_;
    DestinationCounter* destinationCounter_;
    QHash<QString, QLabel*> destinationCounts_;
    QString restorePhoto_,lastProblem_;
    QString folder_;
    QPointer<SettingsDialog> settingsDialog_;
    QPointer<ShortcutsDialog> shortcutsDialog_;
    QPointer<QWidget> destinationEditor_;
    QStackedWidget* workspace_;
    QWidget* photoPage_;
    EditPanel* editPanel_;
    PhotoPopup* photoPopup_;
    Filmstrip* filmstrip_;
    QWidget* footer_;
    QWidget* statusRow_;
    QScrollArea* editScroll_;
    QScrollArea* photoTools_;
    QScrollArea* informationTools_;
};
}
