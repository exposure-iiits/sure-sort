#include "main_window.h"
#include "shortcut_manager.h"
#include "destination_dialog.h"
#include "watermark_dialog.h"
#include "output_dialog.h"
#include "ui_icons.h"
#include <QMenu>
#include <QAction>
#include "edit_panel.h"
#include "shortcuts_dialog.h"
#include "settings_dialog.h"
#include "ui_theme.h"
#include "destination_counter.h"
#include <QComboBox>
#include <QSignalBlocker>
#include <QStyle>
#include <QJsonObject>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QMessageBox>
#include <QStandardPaths>
#include <QFileInfo>
#include <QApplication>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QKeySequenceEdit>
#include <QStackedWidget>
#include <QKeyEvent>

namespace photocull {
MainWindow::MainWindow(qint64 cacheBudget, QWidget* parent,QString databasePath) : QMainWindow(parent) {
    setWindowTitle("Sure Sort"); resize(1280, 800); setMinimumSize(640, 480);
    controller_.setCacheBudget(cacheBudget);
    controller_.setSourceResolver([this](const Photo& photo){return sorting_.resolvedPhoto(photo);});
    setStyleSheet(photoUiStyle());
    auto* central = new QWidget(this); setCentralWidget(central);
    auto* layout = new QVBoxLayout(central); layout->setContentsMargins(0,0,0,0); layout->setSpacing(0);
    auto* top = new QWidget(central);topBar_=top;top->setObjectName("toolbar"); top->setFixedHeight(42);
    auto* topLayout = new QHBoxLayout(top); topLayout->setContentsMargins(14,4,10,4); topLayout->setSpacing(10);
    auto* brand = new QLabel("Sure Sort", top); brand->setObjectName("AppTitle");
    auto* open = new QPushButton(uiIcon("folder"),"Open Folder", top);open->setObjectName("openFolder"); open->setToolTip("Open a local folder · Ctrl+O");
    auto* settings=new QPushButton(uiIcon("settings"),QString(),top);settings->setObjectName("settingsButton");settings->setToolTip("Settings");settings->setAccessibleName("Settings");
    settings->setToolTip("Settings · Ctrl+,");
    auto* destinations=new QAction("Sorting destinations…",this);destinations->setObjectName("destinationSettings");addAction(destinations);
    auto* output=new QAction("Output settings…",this);output->setObjectName("outputSettings");addAction(output);
    auto* watermark=new QAction("Watermark…",this);watermark->setObjectName("watermarkSettings");addAction(watermark);
    auto* shortcutButton=new QPushButton(uiIcon("keyboard"),QString(),top);shortcutButton->setObjectName("allShortcuts");shortcutButton->setAccessibleName("All shortcuts");shortcutButton->setToolTip("All keyboard shortcuts");
    connect(shortcutButton,&QPushButton::clicked,this,[this]{
        if(shortcutsDialog_&&workspace_->currentWidget()==shortcutsDialog_){shortcutsDialog_->close();shortcutsDialog_=nullptr;}
        else showShortcuts();
    });
    auto* edit=new QPushButton("Edit",top);edit->setObjectName("toggleEdit");edit->setCheckable(true);
    folderLabel_ = new QLabel("No folder selected", top); folderLabel_->setObjectName("muted");
    folderLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    outputLabel_=new QLabel("no output folder yet",top);outputLabel_->setObjectName("outputFolder");outputLabel_->setAlignment(Qt::AlignRight|Qt::AlignVCenter);outputLabel_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    auto* fullscreen = new QPushButton(uiIcon("fullscreen"),QString(), top);fullscreen->setObjectName("fullscreenButton");fullscreen->setAccessibleName("Full screen"); fullscreen->setToolTip("Toggle fullscreen · F11");
    for(auto* button:{open,edit,shortcutButton,settings,fullscreen}){button->setProperty("toolbarButton",true);button->setFocusPolicy(Qt::NoFocus);button->setIconSize(QSize(18,18));}
    topLayout->addWidget(brand); topLayout->addWidget(open);topLayout->addWidget(folderLabel_,1);topLayout->addWidget(outputLabel_,1);topLayout->addWidget(edit);topLayout->addWidget(shortcutButton);topLayout->addWidget(settings);topLayout->addWidget(fullscreen);
    layout->addWidget(top);
    workspace_=new QStackedWidget(central);workspace_->setObjectName("workspacePages");
    photoPage_=new QWidget(workspace_);workspace_->addWidget(photoPage_);layout->addWidget(workspace_,1);
    auto* body=new QHBoxLayout(photoPage_);body->setContentsMargins(0,0,0,0);body->setSpacing(0);
    viewer_ = new PhotoViewer(central);body->addWidget(viewer_,1);
    viewer_->setMouseTracking(true);
    qApp->installEventFilter(this);
    connect(viewer_,&PhotoViewer::fullscreenRequested,this,&MainWindow::toggleFullscreen);
    auto* editScroll=new QScrollArea(central);editScroll->setFixedWidth(254);editScroll->setWidgetResizable(true);editScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);editScroll->setFrameShape(QFrame::NoFrame);
    editPanel_=new EditPanel(editScroll);editPanel_->setEnabled(false);editScroll->setWidget(editPanel_);editScroll->hide();body->addWidget(editScroll);
    connect(edit,&QPushButton::toggled,editScroll,&QWidget::setVisible);
    connect(editPanel_,&EditPanel::changed,this,&MainWindow::applyEdits);
    connect(editPanel_,&EditPanel::cropRequested,this,[this]{if(viewer_->isCropping())viewer_->cancelCrop();else viewer_->beginCrop();viewer_->setFocus();});
    connect(editPanel_,&EditPanel::cropAspectChanged,viewer_,&PhotoViewer::setCropAspect);
    connect(editPanel_,&EditPanel::shortcutsRequested,this,&MainWindow::configureEditingShortcuts);
    connect(viewer_,&PhotoViewer::cropConfirmed,this,[this](QRectF crop){auto e=viewer_->edits();e.crop=crop;applyEdits(e);});
    connect(&sorting_,&SortingController::editsChanged,this,[this](const QString& id,const PhotoEdits& e){
        const int index=controller_.index();if(index>=0&&controller_.catalog()[index].id==id){viewer_->setEdits(e);editPanel_->setEdits(e);updateResetButton();}
    });
    cropBar_=new QWidget(central);cropBar_->setObjectName("cropBar");auto* cropLayout=new QHBoxLayout(cropBar_);cropLayout->setContentsMargins(14,5,14,5);cropLayout->setSpacing(8);
    cropLayout->addWidget(new QLabel("Crop",cropBar_));auto* ratio=new QComboBox(cropBar_);ratio->setObjectName("cropRatio");
    const QList<QPair<QString,double>> ratios={{"Free",0},{"Original",-1},{"1:1",1},{"4:5",.8},{"5:4",1.25},{"3:2",1.5},{"2:3",2.0/3},{"4:3",4.0/3},{"3:4",.75},{"16:9",16.0/9},{"9:16",9.0/16}};
    for(const auto& item:ratios) ratio->addItem(item.first,item.second);
    cropLayout->addWidget(ratio);
    auto* cropHint=new QLabel("Drag to crop · Enter applies · Esc cancels",cropBar_);cropHint->setObjectName("muted");cropHint->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);cropLayout->addWidget(cropHint,1);
    auto* applyCrop=new QPushButton("Apply",cropBar_);applyCrop->setObjectName("applyCrop");auto* cancelCrop=new QPushButton("Cancel",cropBar_);cancelCrop->setObjectName("cancelCrop");cropLayout->addWidget(applyCrop);cropLayout->addWidget(cancelCrop);cropBar_->hide();layout->addWidget(cropBar_);
    connect(viewer_,&PhotoViewer::cropModeChanged,cropBar_,&QWidget::setVisible);
    connect(ratio,&QComboBox::currentIndexChanged,this,[this,ratio]{
        const auto value=ratio->currentData().toDouble();viewer_->setCropAspect(value);
        auto* panelRatio=editPanel_->findChild<QComboBox*>("cropAspect");
        for(int i=0;i<panelRatio->count();++i)if(qAbs(panelRatio->itemData(i).toDouble()-value)<.00001){QSignalBlocker block(panelRatio);panelRatio->setCurrentIndex(i);break;}
    });
    connect(editPanel_,&EditPanel::cropAspectChanged,this,[ratio](double value){for(int i=0;i<ratio->count();++i)if(qAbs(ratio->itemData(i).toDouble()-value)<.00001){QSignalBlocker block(ratio);ratio->setCurrentIndex(i);break;}});
    connect(applyCrop,&QPushButton::clicked,viewer_,&PhotoViewer::confirmCrop);connect(cancelCrop,&QPushButton::clicked,viewer_,&PhotoViewer::cancelCrop);

    auto* footer=new QWidget(central);footer->setObjectName("photoFooter");
    auto* footerLayout=new QVBoxLayout(footer);footerLayout->setContentsMargins(10,3,10,4);footerLayout->setSpacing(2);
    auto* destinationRow=new QHBoxLayout;destinationRow->setSpacing(10);
    auto* scroll=new QScrollArea(footer);scroll->setObjectName("destinationScroll");scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);scroll->setMinimumWidth(0);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);scroll->setFixedHeight(38);
    auto* destinationBar=new QWidget(scroll);destinationBar->setObjectName("destinationBar");destinationsLayout_=new QHBoxLayout(destinationBar);
    destinationsLayout_->setContentsMargins(0,1,0,3);destinationsLayout_->setSpacing(7);scroll->setWidget(destinationBar);destinationRow->addWidget(scroll,1);
    undoButton_=new QPushButton(uiIcon("undo"),"Undo",footer);undoButton_->setObjectName("undoAction");undoButton_->setToolTip("Undo last copy / move · Ctrl+Z");undoButton_->setFocusPolicy(Qt::NoFocus);destinationRow->addWidget(undoButton_);
    resetButton_=new QPushButton(uiIcon("reset"),"Reset edits",footer);resetButton_->setObjectName("resetPhotoEdits");resetButton_->setToolTip("Reset all adjustments, rotation and crop on this photo");resetButton_->setFocusPolicy(Qt::NoFocus);resetButton_->hide();destinationRow->addWidget(resetButton_);
    connect(resetButton_,&QPushButton::clicked,this,[this]{editingAction("edit:reset");viewer_->setFocus();});
    connect(undoButton_,&QPushButton::clicked,this,[this]{if(!viewer_->isCropping())sorting_.undo();viewer_->setFocus();});
    connect(viewer_,&PhotoViewer::cropModeChanged,this,[this](bool active){undoButton_->setEnabled(!active&&sorting_.canUndo());updateResetButton();});
    footerLayout->addLayout(destinationRow);
    auto* status=new QHBoxLayout;status->setSpacing(14);
    filenameLabel_=new QLabel("← → Browse   ·   F Fit   ·   Z 100%",footer);filenameLabel_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);status->addWidget(filenameLabel_,2);
    assignedLabel_=new QLabel(footer);assignedLabel_->setObjectName("assignmentStatus");assignedLabel_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);status->addWidget(assignedLabel_,1);
    queueButton_=new QPushButton("Ready",footer);queueButton_->setObjectName("processingStatus");queueButton_->setFlat(true);queueButton_->setFocusPolicy(Qt::NoFocus);status->addWidget(queueButton_);
    positionLabel_=new QLabel("0 / 0",footer);dimensionsLabel_=new QLabel(footer);typeLabel_=new QLabel(footer);zoomLabel_=new QLabel(footer);
    for(auto* label:{positionLabel_,dimensionsLabel_,typeLabel_,zoomLabel_}){label->setStyleSheet("color:#a1a1a9;font-size:12px;");status->addWidget(label);}
    footerLayout->addLayout(status);
    errorLabel_=new QLabel(footer);errorLabel_->setObjectName("processingError");errorLabel_->setWordWrap(true);errorLabel_->setStyleSheet("color:#e0b58e;background:#322a25;border-radius:4px;padding:5px;font-size:11px;");errorLabel_->hide();footerLayout->addWidget(errorLabel_);
    layout->addWidget(footer);
    destinationCounter_=new DestinationCounter(this);
    connect(destinationCounter_,&DestinationCounter::countsChanged,this,[this](const QHash<QString,int>& counts){
        for(auto it=counts.cbegin();it!=counts.cend();++it)if(auto* badge=destinationCounts_.value(it.key())){
            const auto text=it.value()<0?QString("?"):QString::number(it.value());
            badge->setText(text);
            QFont font=badge->font();font.setPixelSize(11);badge->setFont(font);
            while(badge->fontMetrics().horizontalAdvance(text)>24&&font.pixelSize()>7){font.setPixelSize(font.pixelSize()-1);badge->setFont(font);}
            const int diameter=qBound(22,badge->fontMetrics().horizontalAdvance(text)+10,32);
            badge->setFixedSize(diameter,diameter);
            badge->setStyleSheet(QString("background:#353840;color:#cbd0da;border:1px solid #4b505b;border-radius:%1px;font-weight:600;font-size:%2px;").arg(diameter/2).arg(font.pixelSize()));
            badge->setToolTip(it.value()<0?"Folder could not be read":QString("%1 photo%2 in this folder (excluding subfolders)").arg(it.value()).arg(it.value()==1?"":"s"));
            badge->setAccessibleName(badge->property("folderName").toString()+": "+(it.value()<0?"count unavailable":QString::number(it.value())+" photos"));
        }
    });
    connect(&sorting_,&SortingController::progressChanged,destinationCounter_,&DestinationCounter::refresh);
    connect(open, &QPushButton::clicked, this, &MainWindow::chooseFolder);
    connect(fullscreen, &QPushButton::clicked, this, &MainWindow::toggleFullscreen);
    connect(settings,&QPushButton::clicked,this,[this]{
        if(settingsDialog_&&workspace_->currentWidget()==settingsDialog_){settingsDialog_->close();settingsDialog_=nullptr;}
        else openSettings();
    });
    connect(destinations,&QAction::triggered,this,&MainWindow::configureDestinations);
    connect(watermark,&QAction::triggered,this,&MainWindow::configureWatermark);
    connect(output,&QAction::triggered,this,&MainWindow::configureOutput);
    connect(&sorting_,&SortingController::watermarkChanged,viewer_,&PhotoViewer::setWatermark);
    connect(viewer_,&PhotoViewer::editsRendered,this,[this]{const auto size=viewer_->editedSourceSize();if(size.isValid())dimensionsLabel_->setText(QString("%1 × %2").arg(size.width()).arg(size.height()));});
    connect(&scanner_, &FolderScanner::finished, this, [this](quint64 epoch, Catalog photos, const QString& error) {
        if (epoch != folderEpoch_) return;
        controller_.setCatalog(sorting_.mergeMovedPhotos(std::move(photos),folder_));
        viewer_->setLoading(false);
        if (!error.isEmpty()) viewer_->setMessage(error);
        else if (controller_.catalog().isEmpty()) viewer_->setMessage("No JPEG, PNG or recognized RAW files in this folder or its subfolders.");
        else {
            int selected=0;
            for(int i=0;i<controller_.catalog().size();++i)if(controller_.catalog()[i].id==restorePhoto_){selected=i;break;}
            restorePhoto_.clear(); navigate(selected);
        }
        updateStatus();
    });
    connect(&controller_, &PreviewController::previewReady, this,
            [this](quint64 gen, const QString& id, const DecodedImage& result) {
        if (gen != controller_.generation() || controller_.index() < 0 ||
            id != controller_.catalog()[controller_.index()].id) return;
        viewer_->setPreview(result); updateStatus(&result);
    });
    connect(&controller_, &PreviewController::detailReady, this,
            [this](quint64 gen, const QString&, const DecodedImage& result) {
        if (gen == controller_.generation()) { viewer_->setDetail(result); updateStatus(&result); }
    });
    connect(&controller_, &PreviewController::loadingChanged, viewer_, &PhotoViewer::setLoading);
    connect(viewer_, &PhotoViewer::detailRequired, &controller_, &PreviewController::requestDetail);
    connect(viewer_, &PhotoViewer::zoomChanged, this, [this](double percent) { zoomLabel_->setText(QString::number(percent,'f',0) + '%'); });
    resizeTimer_.setSingleShot(true); resizeTimer_.setInterval(120);
    connect(viewer_, &PhotoViewer::targetChanged, &resizeTimer_, qOverload<>(&QTimer::start));
    connect(&resizeTimer_, &QTimer::timeout, this, [this] {
        if (controller_.index() < 0) return;
        const bool zoomed = !viewer_->isFit();
        controller_.select(controller_.index(), viewer_->displayTarget());
        if (zoomed && !viewer_->hasDetail()) controller_.requestDetail(true);
    });
    shortcuts_ = new ShortcutManager(this);
    connect(shortcuts_, &ShortcutManager::triggered, this, [this](const QString& action) {
        const int index = controller_.index();
        if(action=="confirmCrop"){if(viewer_->isCropping())viewer_->confirmCrop();return;}
        if(action=="escape"){const bool cropping=viewer_->isCropping();viewer_->cancelCrop();if(cropping)return;}
        if(action.startsWith("edit:")){editingAction(action);return;}
        if(viewer_->isCropping()){
            if(action=="escape"){viewer_->cancelCrop();return;}
            if(action=="next"||action=="previous"||action=="first"||action=="last"||action=="skip"||action=="undoAssignment"||action=="clearAssignment"||action.startsWith("assign:"))return;
        }
        if (action == "open") chooseFolder();
        else if (action == "settings") openSettings();
        else if (action == "next") navigate(index + 1);
        else if (action == "previous") navigate(index - 1);
        else if (action == "first") navigate(0);
        else if (action == "last") navigate(int(controller_.catalog().size()) - 1);
        else if (action == "fit") viewer_->fit();
        else if (action == "actual"){if(!viewer_->isFit()&&qAbs(viewer_->zoomPercent()-100)<.01)viewer_->fit();else viewer_->actualSize();}
        else if (action == "zoomIn") viewer_->zoomBy(1.25);
        else if (action == "zoomOut") viewer_->zoomBy(0.8);
        else if (action == "fullscreen") toggleFullscreen();
        else if (action == "escape" && isFullScreen()) toggleFullscreen();
        else if (action=="skip") navigate(index+1);
        else if (action=="undoAssignment") sorting_.undo();
        else if (action=="clearAssignment" && index>=0) sorting_.clearAssignment(controller_.catalog()[index]);
        else if (action.startsWith("assign:") && index>=0) sorting_.assign(controller_.catalog()[index],action.mid(7));
    });
    positionTimer_.setSingleShot(true); positionTimer_.setInterval(250);
    connect(&positionTimer_,&QTimer::timeout,this,[this]{sorting_.savePosition(folder_,controller_.index()>=0?controller_.catalog()[controller_.index()].id:QString());});
    connect(&sorting_,&SortingController::configChanged,this,&MainWindow::rebuildDestinations);
    connect(&sorting_,&SortingController::assignmentChanged,this,[this](const QString&){updateStatus();});
    connect(&sorting_,&SortingController::progressChanged,this,[this]{if(sorting_.ready()&&sorting_.issues().isEmpty())lastProblem_.clear();updateQueue();});
    connect(&sorting_,&SortingController::sourcesChanged,this,[this](const QString& id){
        const int index=controller_.index();if(index>=0&&controller_.catalog()[index].id==id&&!viewer_->hasPhoto())controller_.select(index,viewer_->displayTarget());
    });
    connect(&sorting_,&SortingController::advanceRequested,this,[this]{navigate(controller_.index()+1);});
    connect(&sorting_,&SortingController::focusRequested,this,[this](const Photo& photo){
        for(int i=0;i<controller_.catalog().size();++i)if(controller_.catalog()[i].id==photo.id){navigate(i);return;}
        restorePhoto_=photo.id; openFolder(QFileInfo(photo.displayPath()).absolutePath());
    });
    connect(&sorting_,&SortingController::problem,this,[this](const QString& error){lastProblem_=error;queueButton_->setToolTip(error);updateQueue();});
    connect(queueButton_,&QPushButton::clicked,this,[this]{
        processingDetails_=!processingDetails_;updateQueue();
    });
    if(databasePath.isEmpty()) databasePath=QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("session.sqlite");
    if(sorting_.initialize(databasePath)) {
        restorePhoto_=sorting_.restoredPhoto();
        const auto restored=sorting_.restoredFolder();
        if(!restored.isEmpty())QTimer::singleShot(0,this,[this,restored]{if(folder_.isEmpty())openFolder(restored);});
    } else rebuildDestinations();
    updateQueue();
}
void MainWindow::shutdown() {
    destinationCounter_->shutdown();
    resizeTimer_.stop(); positionTimer_.stop();
    if(!folder_.isEmpty())sorting_.savePosition(folder_,controller_.index()>=0?controller_.catalog()[controller_.index()].id:QString());
    sorting_.shutdown(); scanner_.shutdown(); controller_.shutdown();
}
void MainWindow::chooseFolder() {
    const auto folder = QFileDialog::getExistingDirectory(this,"Open photo folder",folder_);
    if (!folder.isEmpty()) openFolder(folder);
    viewer_->setFocus();
}
void MainWindow::openFolder(const QString& folder) {
    ++folderEpoch_; controller_.clear(); resizeTimer_.stop(); folder_ = QDir(folder).absolutePath();
    viewer_->resetPhoto(); viewer_->setLoading(false); viewer_->setMessage("Scanning folder…");
    folderLabel_->setText(QDir(folder_).dirName()); folderLabel_->setToolTip(folder_);
    dimensionsLabel_->clear(); typeLabel_->clear(); zoomLabel_->clear(); updateStatus();
    scanner_.start(folder_, folderEpoch_);
    positionTimer_.start();
}
void MainWindow::navigate(int index) {
    if (controller_.catalog().isEmpty()) return;
    index = qBound(0,index,int(controller_.catalog().size())-1);
    if (index == controller_.index()) return;
    viewer_->resetPhoto(); dimensionsLabel_->clear(); zoomLabel_->clear();
    const auto e=sorting_.edits(controller_.catalog()[index].id);viewer_->setEdits(e);editPanel_->setEdits(e);
    controller_.select(index, viewer_->displayTarget()); updateStatus();
    positionTimer_.start();
}
void MainWindow::updateStatus(const DecodedImage* result) {
    const int index = controller_.index();
    editPanel_->setEnabled(index>=0&&sorting_.ready());
    updateResetButton();
    positionLabel_->setText(QString("%1 / %2").arg(index + 1).arg(controller_.catalog().size()));
    if (index >= 0) {
        const auto& photo = controller_.catalog()[index];
        filenameLabel_->setText(photo.name()); filenameLabel_->setToolTip(photo.sources.join('\n'));
        typeLabel_->setText(photo.typeLabel());
        QStringList placements,paths;for(const auto& assignment:sorting_.assignments(photo.id)){placements.append((assignment.action=="move"?"Move → ":"Copy → ")+assignment.categoryName+" · "+(assignment.retryOperation?QString("retrying %1/2").arg(assignment.automaticRetries):assignment.status));paths.append(assignment.outputPath.isEmpty()?assignment.directory:assignment.outputPath);}
        assignedLabel_->setText(placements.join("  |  "));assignedLabel_->setToolTip(paths.join('\n'));
        const auto assignments=sorting_.assignments(photo.id);
        for(int i=0;i<destinationsLayout_->count();++i)if(auto* button=qobject_cast<QPushButton*>(destinationsLayout_->itemAt(i)->widget())){
            bool assigned=false;for(const auto& task:assignments)if(task.categoryId==button->property("destinationId").toString()&&task.status!="cancelled"&&task.status!="retired-clean")assigned=true;
            if(button->property("assigned").toBool()!=assigned){button->setProperty("assigned",assigned);button->style()->unpolish(button);button->style()->polish(button);}
        }
    } else { filenameLabel_->setText("← → Browse   ·   F Fit   ·   Z 100%"); assignedLabel_->clear(); }
    if (result) {
        const auto dimensions=viewer_->editedSourceSize();
        if (dimensions.isValid()) dimensionsLabel_->setText(QString("%1 × %2").arg(dimensions.width()).arg(dimensions.height()));
        dimensionsLabel_->setToolTip(result->note);
    }
}
void MainWindow::toggleFullscreen() {
    auto* layout=qobject_cast<QVBoxLayout*>(centralWidget()->layout());
    if(isFullScreen()){
        wasMaximized_?showMaximized():showNormal();layout->insertWidget(0,topBar_);
    }else{
        wasMaximized_=isMaximized();layout->removeWidget(topBar_);showFullScreen();
        viewer_->showToast("Double-click to leave fullscreen · Move to top for controls");
    }
    if(auto* button=findChild<QPushButton*>("fullscreenButton")){button->setIcon(uiIcon(isFullScreen()?"exitFullscreen":"fullscreen"));button->setAccessibleName(isFullScreen()?"Exit full screen":"Full screen");button->setToolTip(isFullScreen()?"Leave fullscreen · Double-click / F11 / Esc":"Full screen · F11");}
    topBar_->setVisible(!isFullScreen());
}
bool MainWindow::eventFilter(QObject* target,QEvent* event){
    auto* widget=qobject_cast<QWidget*>(target);
    if(destinationEditor_&&widget&&(widget==destinationEditor_||destinationEditor_->isAncestorOf(widget))&&event->type()==QEvent::KeyPress&&static_cast<QKeyEvent*>(event)->key()==Qt::Key_Escape){
        cancelDestinationEdit();return true;
    }
    if(isFullScreen()&&widget&&widget->window()==this&&event->type()==QEvent::MouseMove){
        const auto point=centralWidget()->mapFromGlobal(static_cast<QMouseEvent*>(event)->globalPosition().toPoint());
        if(point.y()<=5){topBar_->setGeometry(0,0,centralWidget()->width(),42);topBar_->show();topBar_->raise();}
        else if(point.y()>58)topBar_->hide();
    }
    return QMainWindow::eventFilter(target,event);
}
void MainWindow::resizeEvent(QResizeEvent* event){
    QMainWindow::resizeEvent(event);
    if(isFullScreen())topBar_->setGeometry(0,0,centralWidget()->width(),42);
}
void MainWindow::updateResetButton(){
    const int index=controller_.index();
    resetButton_->setVisible(index>=0&&!sorting_.edits(controller_.catalog()[index].id).isIdentity());
    resetButton_->setEnabled(sorting_.ready());
}
void MainWindow::configureDestinations() {
    openSettings(SettingsDialog::Sorting);
}
void MainWindow::configureWatermark(){
    openSettings(SettingsDialog::Watermark);
}
void MainWindow::configureOutput(){
    openSettings(SettingsDialog::Saving);
}
void MainWindow::openSettings(int page){
    if(!sorting_.ready()){QMessageBox::warning(this,"Settings unavailable",lastProblem_);return;}
    if(!settingsDialog_){
        auto* dialog=new SettingsDialog(&sorting_,shortcuts_,this);settingsDialog_=dialog;dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowFlags(Qt::Widget);dialog->setMinimumSize(0,0);dialog->setProperty("suspendPhotoShortcuts",true);workspace_->addWidget(dialog);
        connect(dialog,&QDialog::finished,this,[this,dialog]{if(workspace_->currentWidget()==dialog){workspace_->setCurrentWidget(photoPage_);viewer_->setFocus();}});
        connect(dialog,&SettingsDialog::editingRequested,this,[this]{if(auto* button=findChild<QPushButton*>("toggleEdit"))button->setChecked(true);viewer_->setFocus();});
    }
    workspace_->setCurrentWidget(settingsDialog_);
    settingsDialog_->showPage(SettingsDialog::Page(page));
}
void MainWindow::rebuildDestinations() {
    destinationEditor_=nullptr;
    if(auto* scroll=findChild<QScrollArea*>("destinationScroll"))scroll->setFixedHeight(38);
    const auto output=sorting_.config().outputRoot;
    outputLabel_->setText(output.isEmpty()?"no output folder yet":"→ "+QDir::toNativeSeparators(output));outputLabel_->setToolTip(output);
    while(auto* item=destinationsLayout_->takeAt(0)){if(item->widget())item->widget()->deleteLater();delete item;}
    destinationCounts_.clear();
    QHash<QString,QString> folderPaths;
    QVector<QPair<QString,QKeySequence>> bindings;
    for(const auto& folder:sorting_.config().folders){
        bindings.append({folder.id,QKeySequence(folder.shortcut,QKeySequence::PortableText)});
        bindings.append({folder.id,QKeySequence(folder.alternateShortcut,QKeySequence::PortableText)});
        auto* button=new QPushButton((folder.shortcut.isEmpty()?QString():folder.shortcut+"  ")+folder.name,this);
        button->setProperty("destinationChip",true);button->setProperty("destinationId",folder.id);button->setFocusPolicy(Qt::NoFocus);
        button->setProperty("destinationAction",folder.action);
        button->setToolTip((folder.action=="move"?"Move to ":"Copy to ")+QDir(sorting_.config().outputRoot).filePath(folder.name)); destinationsLayout_->addWidget(button);
        auto* count=new QLabel("…",this);count->setObjectName("destinationPhotoCount");count->setProperty("destinationId",folder.id);count->setProperty("folderName",folder.name);
        count->setAlignment(Qt::AlignCenter);count->setFixedSize(22,22);count->setStyleSheet("background:#353840;color:#cbd0da;border:1px solid #4b505b;border-radius:11px;font-size:11px;");
        count->setToolTip("Counting photos…");destinationsLayout_->addWidget(count);destinationsLayout_->addSpacing(5);
        destinationCounts_.insert(folder.id,count);folderPaths.insert(folder.id,QDir(sorting_.config().outputRoot).filePath(folder.name));
        connect(button,&QPushButton::clicked,this,[this,id=folder.id]{if(!viewer_->isCropping()&&controller_.index()>=0)sorting_.assign(controller_.catalog()[controller_.index()],id);viewer_->setFocus();});
        button->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(button,&QPushButton::customContextMenuRequested,this,[this,button,id=folder.id](const QPoint& point){destinationMenu(id,button->mapToGlobal(point));});
    }
    QString error;
    shortcuts_->setDestinations({},error);
    // Load application keys before editing chords and folder keys, checking conflicts as each group is restored.
    auto disabled=shortcuts_->editBindings();
    for(int i=0;i<disabled.size();++i){auto row=disabled[i].toObject();row["trigger"]="";row["alternate"]="";disabled[i]=row;}
    shortcuts_->setEditBindings(disabled,error);
    const auto app=sorting_.uiPreferences().value("appBindings").toArray();
    if(!app.isEmpty()&&!shortcuts_->setAppBindings(app,error)){lastProblem_=error;updateQueue();}
    // Existing destination keys take priority over new default editing holds.
    auto editing=sorting_.editBindings();if(editing.isEmpty())editing=ShortcutManager::defaultEditBindings();
    for(int i=0;i<editing.size();++i){auto row=editing[i].toObject();
        for(const auto& binding:bindings)if(!binding.second.isEmpty()&&
            (binding.second==QKeySequence::fromString(row["hold"].toString(),QKeySequence::PortableText)||binding.second==QKeySequence::fromString(row["trigger"].toString(),QKeySequence::PortableText)||binding.second==QKeySequence(row["alternate"].toString()))){row["trigger"]="";row["alternate"]="";editing[i]=row;}
    }
    if(!shortcuts_->setEditBindings(editing,error)){
        lastProblem_=error;
        // Keep folder bindings usable even if a saved shortcut record is damaged.
        auto fallback=ShortcutManager::defaultEditBindings();
        for(int i=0;i<fallback.size();++i){auto row=fallback[i].toObject();for(const auto& binding:bindings)if(!binding.second.isEmpty()&&
            (binding.second==QKeySequence(row["hold"].toString())||binding.second==QKeySequence(row["trigger"].toString()))){row["trigger"]="";fallback[i]=row;}}
        shortcuts_->setEditBindings(fallback,error);updateQueue();
    }
    if(!shortcuts_->setDestinations(bindings,error)){lastProblem_=error;updateQueue();}
    if(bindings.isEmpty())destinationsLayout_->addWidget(new QLabel("Add a sorting folder",this));
    auto* add=new QPushButton(uiIcon("plus"),QString(),this);add->setObjectName("quickAddDestination");add->setFixedSize(28,28);add->setFocusPolicy(Qt::NoFocus);add->setToolTip("Create sorting folder");add->setAccessibleName("Create sorting folder");add->setEnabled(sorting_.ready());destinationsLayout_->addWidget(add);
    connect(add,&QPushButton::clicked,this,[this]{editDestination();});
    destinationsLayout_->addStretch();
    destinationCounter_->setFolders(folderPaths,sorting_.config().outputRoot);
}
void MainWindow::destinationMenu(const QString& id,const QPoint& position){
    QMenu menu(this);menu.setObjectName("destinationContextMenu");
    QString action;for(const auto& folder:sorting_.config().folders)if(folder.id==id)action=folder.action;
    for(const auto& value:QStringList{"copy","move"}){
        auto* item=menu.addAction(value=="copy"?"Copy":"Move");item->setObjectName("destinationMode_"+value);item->setCheckable(true);item->setChecked(action==value);
        connect(item,&QAction::triggered,this,[this,id,value]{auto config=sorting_.config();for(auto& folder:config.folders)if(folder.id==id)folder.action=value;sorting_.configure(config);});
    }
    menu.addSeparator();auto* rename=menu.addAction("Rename…");rename->setObjectName("renameDestination");
    connect(rename,&QAction::triggered,this,[this,id]{editDestination(id);});
    menu.exec(position);
}
void MainWindow::editDestination(const QString& id){
    if(!sorting_.ready())return;
    if(destinationEditor_){destinationEditor_->findChild<QLineEdit*>("quickFolderName")->setFocus();return;}
    const auto config=sorting_.config();
    if(config.outputRoot.isEmpty()){
        openSettings(SettingsDialog::Sorting);
        if(auto* root=settingsDialog_->findChild<QLineEdit*>("destinationOutputRoot"))root->setFocus();
        return;
    }
    workspace_->setCurrentWidget(photoPage_);
    Destination selected;bool found=id.isEmpty();
    for(const auto& folder:config.folders)if(folder.id==id){selected=folder;found=true;}
    if(!found)return;
    if(id.isEmpty())selected.shortcut=shortcuts_->nextDestinationKey(config);
    auto* editor=new QWidget(this);destinationEditor_=editor;editor->setObjectName("quickDestinationEditor");
    editor->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Fixed);
    editor->setProperty("suspendPhotoShortcuts",true);
    editor->setProperty("destinationAction",selected.action);
    auto* row=new QHBoxLayout(editor);row->setContentsMargins(8,0,3,0);row->setSpacing(6);
    if(!selected.shortcut.isEmpty())row->addWidget(new QLabel(selected.shortcut,editor));
    auto* name=new QLineEdit(selected.name,editor);name->setObjectName("quickFolderName");
    name->setPlaceholderText("Folder name");name->setFixedWidth(180);
    row->addWidget(name);
    auto commit=[this,id,name,selected,editor]{
        if(destinationEditor_!=editor)return;
        if(name->text().trimmed().isEmpty()){cancelDestinationEdit();return;}
        auto config=sorting_.config();auto folder=selected;
        folder.id=id.isEmpty()?newId():id;folder.name=name->text().trimmed();
        bool found=id.isEmpty();
        if(id.isEmpty())config.folders.append(folder);else for(auto& existing:config.folders)if(existing.id==id){existing=folder;found=true;}
        QString message=found?validateDestinationConfig(config):"This folder no longer exists.";
        QVector<QPair<QString,QKeySequence>> bindings;
        for(const auto& existing:config.folders){bindings.append({existing.id,QKeySequence(existing.shortcut)});bindings.append({existing.id,QKeySequence(existing.alternateShortcut)});}
        if(message.isEmpty())shortcuts_->validateDestinations(bindings,message);
        if(message.isEmpty()&&!id.isEmpty()&&!sorting_.renameDestination(id,folder.name,message)&&message.isEmpty())message="Could not rename the folder.";
        if(message.isEmpty()&&id.isEmpty()&&!QDir().mkpath(QDir(config.outputRoot).filePath(folder.name)))message="Could not create the folder. Check the output directory.";
        if(message.isEmpty()&&!sorting_.configure(config))message="Could not save the folder configuration.";
        if(!message.isEmpty()){
            viewer_->showToast(message);name->setToolTip(message);
            QTimer::singleShot(0,name,[name]{name->setFocus();});return;
        }
        viewer_->showToast({});viewer_->setFocus();
    };
    connect(name,&QLineEdit::editingFinished,this,commit);
    int position=destinationsLayout_->count()-2;
    if(!id.isEmpty())for(int i=0;i<destinationsLayout_->count();++i)if(auto* chip=destinationsLayout_->itemAt(i)->widget())
        if(chip->property("destinationChip").toBool()&&chip->property("destinationId").toString()==id){position=i;chip->hide();break;}
    destinationsLayout_->insertWidget(qMax(0,position),editor);
    editor->show();name->setFocus();name->selectAll();
    if(auto* scroll=findChild<QScrollArea*>("destinationScroll"))QTimer::singleShot(0,editor,[scroll,editor]{scroll->ensureWidgetVisible(editor);});
}
void MainWindow::cancelDestinationEdit(){
    if(destinationEditor_){auto* editor=destinationEditor_.data();destinationEditor_=nullptr;editor->hide();rebuildDestinations();viewer_->setFocus();}
}
void MainWindow::applyEdits(const PhotoEdits& edits){
    const int index=controller_.index();if(index<0)return;
    if(viewer_->isCropping()&&(edits.rotation!=viewer_->edits().rotation||edits.rotationDegrees!=viewer_->edits().rotationDegrees||edits.crop!=viewer_->edits().crop||edits.isIdentity()))viewer_->cancelCrop();
    sorting_.setEdits(controller_.catalog()[index],edits);
}
void MainWindow::editingAction(const QString& action){
    const int index=controller_.index();if(index<0)return;
    if(action=="edit:crop"){if(viewer_->isCropping())viewer_->cancelCrop();else viewer_->beginCrop();viewer_->setFocus();return;}
    auto e=sorting_.edits(controller_.catalog()[index].id);
    if(action=="edit:reset")e={};
    else if(action=="edit:rotateRight")e.rotation=(e.rotation+1)%4;
    else if(action=="edit:rotateLeft")e.rotation=(e.rotation+3)%4;
    else{
        const QStringList names={"exposure","brightness","contrast","highlights","shadows","saturation"};
        int* values[]={&e.exposure,&e.brightness,&e.contrast,&e.highlights,&e.shadows,&e.saturation};
        const int step=sorting_.editingStep();
        for(int i=0;i<names.size();++i){if(action=="edit:"+names[i]+"Up")*values[i]=qMin(i==0?50:100,*values[i]+step);if(action=="edit:"+names[i]+"Down")*values[i]=qMax(i==0?-50:-100,*values[i]-step);}
    }
    applyEdits(e);
    QString feedback;
    if(action=="edit:reset")feedback="Edits reset";
    else if(action=="edit:rotateRight"||action=="edit:rotateLeft")feedback=QString("Rotation %1°").arg(editRotationDegrees(e));
    else {
        const QStringList names={"exposure","brightness","contrast","highlights","shadows","saturation"};
        const int values[]={e.exposure,e.brightness,e.contrast,e.highlights,e.shadows,e.saturation};
        for(int i=0;i<names.size();++i)if(action.startsWith("edit:"+names[i])){
            auto label=names[i];label[0]=label[0].toUpper();
            feedback=label+" "+(values[i]>0?"+":"")+(i==0?QString::number(values[i]/10.0,'f',1)+" EV":QString::number(values[i]));break;
        }
    }
    if(!feedback.isEmpty())viewer_->showToast(feedback);
}
void MainWindow::showShortcuts(){
    if(!shortcutsDialog_){
        auto* dialog=new ShortcutsDialog(shortcuts_,&sorting_,this);shortcutsDialog_=dialog;
        dialog->setWindowFlags(Qt::Widget);dialog->setProperty("suspendPhotoShortcuts",true);workspace_->addWidget(dialog);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog,&QDialog::finished,this,[this,dialog]{if(workspace_->currentWidget()==dialog){workspace_->setCurrentWidget(photoPage_);viewer_->setFocus();}});
    }
    workspace_->setCurrentWidget(shortcutsDialog_);shortcutsDialog_->show();
}
void MainWindow::configureEditingShortcuts(){
    showShortcuts();
    shortcutsDialog_->showCategory(ShortcutsDialog::Editing);
}
void MainWindow::updateQueue() {
    const auto issues=sorting_.issues();
    const auto fullStatus=sorting_.processingStatus();
    const auto count=[&fullStatus](int part){return fullStatus.section('|',part,part).section(':',1).trimmed().toInt();};
    const int pending=count(1)+count(2),failed=count(3);
    QString status="Ready";
    if(!sorting_.ready())status="Sorting unavailable";
    else if(failed)status=QString("Failed: %1").arg(failed);
    else if(pending)status=QString("Saving %1…").arg(pending);
    else {const int completed=count(0);if(completed)status=QString("%1 sorted").arg(completed);}
    queueButton_->setText(status);
    undoButton_->setEnabled(sorting_.canUndo()&&!viewer_->isCropping());
    const bool error=!issues.isEmpty()||!lastProblem_.isEmpty();
    queueButton_->setStyleSheet(error?"padding:2px 0;border:0;background:transparent;font-size:11px;color:#e0b58e;":"padding:2px 0;border:0;background:transparent;font-size:11px;color:#99999f;");
    const auto details=issues.isEmpty()?lastProblem_:issues.join('\n');queueButton_->setToolTip(details.isEmpty()?fullStatus+"\nFailures retry automatically":details);
    errorLabel_->setVisible(error);
    if(error){const auto first=details.section('\n',0,0);errorLabel_->setText(processingDetails_?details:"Processing error: "+first.left(180)+(first.size()>180?"…":"")+" · Click processing status for details.");errorLabel_->setToolTip(details);}
}
}
