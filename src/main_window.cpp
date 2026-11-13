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
#include "destination_chip.h"
#include "photo_popup.h"
#include "filmstrip.h"
#include "copy_flight.h"
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
#include <QScrollBar>
#include <QWheelEvent>
#include <QGridLayout>
#include <QPropertyAnimation>
#include <QDesktopServices>
#include <QUrl>
#include <QSet>

namespace photocull {
MainWindow::MainWindow(qint64 cacheBudget, QWidget* parent,QString databasePath) : QMainWindow(parent) {
    setWindowTitle("Sure Sort"); resize(1280, 800); setMinimumSize(640, 480);
    controller_.setCacheBudget(cacheBudget);
    setStyleSheet(photoUiStyle());
    auto* central = new QWidget(this); setCentralWidget(central);
    auto* layout = new QVBoxLayout(central); layout->setContentsMargins(0,0,0,0); layout->setSpacing(0);
    auto* top = new QWidget(central);topBar_=top;top->setObjectName("toolbar"); top->setFixedHeight(42);
    auto* topLayout = new QHBoxLayout(top); topLayout->setContentsMargins(14,4,10,4); topLayout->setSpacing(6);
    auto* brand = new QLabel("Sure Sort", top); brand->setObjectName("AppTitle");
    auto* open = new QPushButton(uiIcon("folder"),"Open Folder", top);open->setObjectName("openFolder"); open->setToolTip("Open a local folder · Ctrl+O");
    auto* settings=new QPushButton(uiIcon("settings"),QString(),top);settings->setObjectName("settingsButton");settings->setToolTip("Settings");settings->setAccessibleName("Settings");
    settings->setToolTip("Settings · Ctrl+,");
    auto* destinations=new QAction("Sorting destinations…",this);destinations->setObjectName("destinationSettings");addAction(destinations);
    auto* output=new QAction("Output settings…",this);output->setObjectName("outputSettings");addAction(output);
    auto* watermark=new QAction("Watermark…",this);watermark->setObjectName("watermarkSettings");addAction(watermark);
    auto* shortcutButton=new QPushButton(uiIcon("keyboard"),QString(),top);shortcutButton->setObjectName("allShortcuts");shortcutButton->setAccessibleName("All shortcuts");shortcutButton->setToolTip("All keyboard shortcuts");
    connect(shortcutButton,&QPushButton::clicked,this,&MainWindow::showShortcuts);
    auto* information=new QPushButton(uiIcon("info"),QString(),top);information->setObjectName("photoInfoButton");information->setAccessibleName("Photo information");information->setToolTip("Photo information");information->setCheckable(true);information->setEnabled(false);
    auto* histogram=new QPushButton(uiIcon("histogram"),QString(),top);histogram->setObjectName("histogramButton");histogram->setAccessibleName("Photo histogram");histogram->setToolTip("Photo histogram");histogram->setCheckable(true);histogram->setEnabled(false);
    auto* edit=new QPushButton("Edit",top);edit->setObjectName("toggleEdit");edit->setCheckable(true);
    folderLabel_ = new QLabel("No folder selected", top); folderLabel_->setObjectName("muted");
    folderLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    outputLabel_=new QPushButton("no output folder yet",top);outputLabel_->setObjectName("outputFolder");outputLabel_->setCursor(Qt::PointingHandCursor);outputLabel_->setFocusPolicy(Qt::NoFocus);outputLabel_->setAccessibleName("Output folder options");connect(outputLabel_,&QPushButton::clicked,this,&MainWindow::outputFolderMenu);outputLabel_->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    auto* fullscreen = new QPushButton(uiIcon("fullscreen"),QString(), top);fullscreen->setObjectName("fullscreenButton");fullscreen->setAccessibleName("Full screen"); fullscreen->setToolTip("Toggle fullscreen · F11");
    for(auto* button:{open,edit,information,histogram,shortcutButton,settings,fullscreen}){button->setProperty("toolbarButton",true);button->setFocusPolicy(Qt::NoFocus);button->setIconSize(QSize(18,18));}
    topLayout->addWidget(brand); topLayout->addWidget(open);topLayout->addWidget(folderLabel_,1);topLayout->addWidget(outputLabel_);topLayout->addWidget(information);topLayout->addWidget(histogram);topLayout->addWidget(edit);topLayout->addWidget(shortcutButton);topLayout->addWidget(settings);topLayout->addWidget(fullscreen);
    layout->addWidget(top);
    workspace_=new QStackedWidget(central);workspace_->setObjectName("workspacePages");
    photoPage_=new QWidget(workspace_);workspace_->addWidget(photoPage_);layout->addWidget(workspace_,1);
    auto* photoGrid=new QGridLayout(photoPage_);photoGrid->setContentsMargins(0,0,0,0);photoGrid->setSpacing(0);photoGrid->setRowStretch(1,1);photoGrid->setColumnStretch(1,1);
    auto* photoBody=new QWidget(photoPage_);photoGrid->addWidget(photoBody,1,1);
    auto* body=new QHBoxLayout(photoBody);body->setContentsMargins(0,0,0,0);body->setSpacing(0);
    viewer_ = new PhotoViewer(central);body->addWidget(viewer_,1);
    photoTools_=new QScrollArea(viewer_);photoTools_->setObjectName("photoTools");photoTools_->setAttribute(Qt::WA_NoMousePropagation);photoTools_->setWidgetResizable(true);photoTools_->setFrameShape(QFrame::NoFrame);photoTools_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    photoTools_->setStyleSheet("QScrollArea#photoTools, QScrollArea#photoTools > QWidget > QWidget {background:transparent;border:0;} QScrollArea#photoTools QScrollBar:vertical {width:6px;background:transparent;}");photoTools_->viewport()->setAutoFillBackground(false);
    auto* toolContent=new QWidget;auto* toolLayout=new QVBoxLayout(toolContent);toolLayout->setContentsMargins(0,0,0,0);toolLayout->setSpacing(12);toolLayout->setSizeConstraint(QLayout::SetMinimumSize);photoTools_->setWidget(toolContent);
    informationTools_=new QScrollArea(viewer_);informationTools_->setObjectName("informationTools");informationTools_->setAttribute(Qt::WA_NoMousePropagation);informationTools_->setWidgetResizable(true);informationTools_->setFrameShape(QFrame::NoFrame);informationTools_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    informationTools_->setStyleSheet("QScrollArea#informationTools, QScrollArea#informationTools > QWidget > QWidget {background:transparent;border:0;} QScrollArea#informationTools QScrollBar:vertical {width:6px;background:transparent;}");informationTools_->viewport()->setAutoFillBackground(false);
    auto* infoContent=new QWidget;auto* infoLayout=new QVBoxLayout(infoContent);infoLayout->setContentsMargins(0,0,0,0);infoLayout->setSpacing(0);infoLayout->setSizeConstraint(QLayout::SetMinimumSize);informationTools_->setWidget(infoContent);
    photoPopup_=new PhotoPopup(infoContent);photoPopup_->setProperty("layoutManaged",true);infoLayout->addWidget(photoPopup_);infoLayout->addStretch();informationTools_->hide();
    photoTools_->hide();
    connect(information,&QPushButton::clicked,this,[this]{togglePhotoPopup(false);});
    connect(histogram,&QPushButton::clicked,this,[this]{togglePhotoPopup(true);});
    connect(photoPopup_,&PhotoPopup::dismissed,this,[information,histogram,this]{information->setChecked(false);histogram->setChecked(false);layoutPhotoTools();viewer_->setFocus();});
    viewer_->setMouseTracking(true);
    qApp->installEventFilter(this);
    connect(viewer_,&PhotoViewer::fullscreenRequested,this,&MainWindow::toggleFullscreen);
    auto* editScroll=new QScrollArea(toolContent);editScroll_=editScroll;editScroll->setObjectName("editCard");editScroll->setFixedWidth(336);editScroll->setWidgetResizable(true);editScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);editScroll->setFrameShape(QFrame::NoFrame);
    editScroll->setStyleSheet("QScrollArea#editCard {background:rgba(30,30,35,235);border:1px solid rgba(255,255,255,42);border-radius:12px;padding:4px;} QScrollArea#editCard > QWidget > QWidget {background:transparent;} QScrollArea#editCard QSlider {background:transparent;} QScrollArea#editCard QScrollBar:vertical {background:transparent;width:6px;}");
    editScroll->viewport()->setAutoFillBackground(false);
    editPanel_=new EditPanel(editScroll);editPanel_->setEnabled(false);editScroll->setWidget(editPanel_);editScroll->hide();
    toolLayout->addWidget(editScroll);toolLayout->addStretch();
    connect(viewer_,&PhotoViewer::targetChanged,this,&MainWindow::layoutPhotoTools);
    connect(edit,&QPushButton::toggled,editScroll,&QWidget::setVisible);
    connect(edit,&QPushButton::toggled,this,&MainWindow::layoutPhotoTools);
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

    filmstrip_=new Filmstrip(photoPage_);photoGrid->addWidget(filmstrip_,2,1);filmstrip_->hide();
    filmstrip_->setCompletionResolver([this](const QString& id){return sorting_.hasCompletedCopy(id);});
    connect(filmstrip_,&Filmstrip::photoSelected,this,[this](int index){if(!viewer_->isCropping()){navigate(index);viewer_->setFocus();}});
    auto* footer=new QWidget(central);footer_=footer;footer->setObjectName("photoFooter");
    auto* footerLayout=new QVBoxLayout(footer);footerLayout->setContentsMargins(10,3,10,4);footerLayout->setSpacing(2);
    auto* destinationRow=new QHBoxLayout;destinationRow->setSpacing(10);
    auto* scroll=new QScrollArea(footer);scroll->setObjectName("destinationScroll");scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);scroll->setMinimumWidth(0);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);scroll->setFixedHeight(44);scroll->viewport()->installEventFilter(this);
    auto* destinationBar=new QWidget(scroll);destinationBar->setObjectName("destinationBar");destinationsLayout_=new QHBoxLayout(destinationBar);
    destinationsLayout_->setContentsMargins(0,1,0,3);destinationsLayout_->setSpacing(7);scroll->setWidget(destinationBar);destinationRow->addWidget(scroll,1);
    destinationsLayout_->setSizeConstraint(QLayout::SetMinimumSize);
    resetButton_=new QPushButton(uiIcon("reset"),"Reset edits",footer);resetButton_->setObjectName("resetPhotoEdits");resetButton_->setToolTip("Reset all adjustments, rotation and crop on this photo");resetButton_->setFocusPolicy(Qt::NoFocus);resetButton_->hide();destinationRow->addWidget(resetButton_);
    connect(resetButton_,&QPushButton::clicked,this,[this]{editingAction("edit:reset");viewer_->setFocus();});
    connect(viewer_,&PhotoViewer::cropModeChanged,this,[this](bool){updateStatus();});
    footerLayout->addLayout(destinationRow);
    statusRow_=new QWidget(footer);statusRow_->setObjectName("photoStatusRow");auto* status=new QHBoxLayout(statusRow_);status->setContentsMargins(0,0,0,0);status->setSpacing(10);
    filenameLabel_=new QLabel("← → Browse   ·   Z 100%",footer);filenameLabel_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);status->addWidget(filenameLabel_,2);
    queueButton_=new QPushButton("Ready",footer);queueButton_->setObjectName("processingStatus");queueButton_->setFlat(true);queueButton_->setFocusPolicy(Qt::NoFocus);status->addWidget(queueButton_);
    positionLabel_=new QLabel("0 / 0",footer);dimensionsLabel_=new QLabel(footer);typeLabel_=new QLabel(footer);zoomLabel_=new QLabel(footer);
    for(auto* label:{positionLabel_,dimensionsLabel_,typeLabel_,zoomLabel_}){label->setStyleSheet("color:#a1a1a9;font-size:12px;");status->addWidget(label);}
    footerLayout->addWidget(statusRow_);
    errorLabel_=new QLabel(footer);errorLabel_->setObjectName("processingError");errorLabel_->setWordWrap(true);errorLabel_->setStyleSheet("color:#e0b58e;background:#322a25;border-radius:4px;padding:5px;font-size:11px;");errorLabel_->hide();footerLayout->addWidget(errorLabel_);
    progressNotice_=new QLabel(footer);progressNotice_->setObjectName("folderProgressNotice");progressNotice_->setStyleSheet("color:#e0b58e;font-size:11px;");progressNotice_->hide();footerLayout->addWidget(progressNotice_);
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
    connect(settings,&QPushButton::clicked,this,[this]{openSettings();});
    connect(destinations,&QAction::triggered,this,&MainWindow::configureDestinations);
    connect(watermark,&QAction::triggered,this,&MainWindow::configureWatermark);
    connect(output,&QAction::triggered,this,&MainWindow::configureOutput);
    connect(&sorting_,&SortingController::watermarkChanged,viewer_,&PhotoViewer::setWatermark);
    connect(viewer_,&PhotoViewer::editsRendered,this,[this]{const auto size=viewer_->editedSourceSize();if(size.isValid())dimensionsLabel_->setText(QString("%1 × %2").arg(size.width()).arg(size.height()));if(photoPopup_->isVisible()&&photoPopup_->mode()==PhotoPopup::Histogram)photoPopup_->setHistogramImage(viewer_->histogramImage());});
    connect(&scanner_, &FolderScanner::finished, this, [this](quint64 epoch, Catalog photos, const QString& error) {
        if (epoch != folderEpoch_) return;
        sorting_.registerCatalog(photos);controller_.setCatalog(std::move(photos));
        filmstrip_->setCatalog(controller_.catalog());applyViewerPreferences();
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
    connect(viewer_, &PhotoViewer::targetChanged,photoPopup_,&PhotoPopup::reposition);
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
            if(action=="next"||action=="previous"||action=="first"||action=="last"||action=="skip"||action=="clearAssignment"||action.startsWith("assign:"))return;
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
        else if (action=="clearAssignment" && index>=0) sorting_.clearAssignment(controller_.catalog()[index]);
        else if (action.startsWith("assign:") && index>=0) toggleDestination(action.mid(7));
    });
    positionTimer_.setSingleShot(true); positionTimer_.setInterval(250);
    connect(&positionTimer_,&QTimer::timeout,this,[this]{sorting_.savePosition(folder_,controller_.index()>=0?controller_.catalog()[controller_.index()].id:QString());});
    connect(&sorting_,&SortingController::configChanged,this,&MainWindow::rebuildDestinations);
    connect(&sorting_,&SortingController::assignmentChanged,this,[this](const QString&){filmstrip_->refreshCompletion();updateStatus();});
    connect(&sorting_,&SortingController::progressChanged,this,[this]{if(sorting_.ready()&&sorting_.issues().isEmpty())lastProblem_.clear();updateQueue();});
    connect(&sorting_,&SortingController::advanceRequested,this,[this]{navigate(controller_.index()+1);});
    connect(&sorting_,&SortingController::copyCompleted,this,[this](const QString&,const QString& destination){if(sorting_.uiPreferences().value("showDestinationToast").toBool(false))viewer_->showToast(destination);});
    connect(&sorting_,&SortingController::problem,this,[this](const QString& error){lastProblem_=error;queueButton_->setToolTip(error);updateQueue();});
    connect(queueButton_,&QPushButton::clicked,this,[this]{
        processingDetails_=!processingDetails_;updateQueue();
    });
    if(databasePath.isEmpty()) databasePath=QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("profile.sqlite");
    if(sorting_.initialize(databasePath)) {
        applyViewerPreferences();
        restorePhoto_=sorting_.restoredPhoto();
        const auto restored=sorting_.restoredFolder();
        if(!restored.isEmpty())QTimer::singleShot(0,this,[this,restored]{if(folder_.isEmpty())openFolder(restored);});
    } else rebuildDestinations();
    updateQueue();
}
MainWindow::~MainWindow(){qApp->removeEventFilter(this);}
void MainWindow::shutdown() {
    stopCopyAnimation();
    photoPopup_->shutdown();filmstrip_->shutdown();
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
void MainWindow::updateOutputButton(){
    const auto root=sorting_.config().outputRoot;
    const auto full=root.isEmpty()?QString("Choose output folder"):"→ "+QDir::toNativeSeparators(root);
    const int limit=qBound(150,width()/3,360);
    const auto metrics=outputLabel_->fontMetrics();
    const int buttonWidth=qMin(limit,metrics.horizontalAdvance(full+QString::fromUtf8("  ▾"))+24);
    outputLabel_->setFixedSize(buttonWidth,28);
    outputLabel_->setText(metrics.elidedText(full,Qt::ElideMiddle,buttonWidth-38)+QString::fromUtf8("  ▾"));
    outputLabel_->setToolTip((root.isEmpty()?QString("Output folder"):QDir::toNativeSeparators(root))+"\nOpen folder or change output location");
}
void MainWindow::outputFolderMenu(){
    QMenu menu(this);menu.setObjectName("outputFolderMenu");menu.setMinimumWidth(220);
    const auto root=sorting_.config().outputRoot;
    auto* open=menu.addAction(uiIcon("folder"),"Open in Explorer");open->setObjectName("openOutputInExplorer");open->setEnabled(QFileInfo(root).isDir());
    connect(open,&QAction::triggered,this,[root]{QDesktopServices::openUrl(QUrl::fromLocalFile(root));});
    auto* change=menu.addAction(uiIcon("settings"),"Change output folder…");change->setObjectName("changeOutputFolder");change->setEnabled(sorting_.ready());
    connect(change,&QAction::triggered,this,[this,root]{const auto selected=QFileDialog::getExistingDirectory(this,"Change output folder",root);if(selected.isEmpty())return;auto config=sorting_.config();config.outputRoot=selected;sorting_.configure(config);});
    menu.ensurePolished();
    menu.exec(outputLabel_->mapToGlobal(QPoint(outputLabel_->width()-menu.sizeHint().width(),outputLabel_->height()+6)));
}
void MainWindow::toggleDestination(const QString& id){
    const int index=controller_.index();
    if(index<0||viewer_->isCropping()||!sorting_.ready())return;
    const auto photo=controller_.catalog()[index];
    // Capture before assign emits its synchronous auto-advance signal.
    const auto bounds=viewer_->sortingBounds();
    const QRect source(viewer_->mapTo(centralWidget(),bounds.topLeft()),bounds.size());
    const auto snapshot=viewer_->sortingSnapshot();
    const auto result=sorting_.toggleAssignment(photo,id);
    if(result==SortingController::DestinationToggle::CopyQueued)animateCopy(snapshot,source,photo.id,id);
    else if(result==SortingController::DestinationToggle::RemovalRequested&&copyFlight_&&copyFlight_->property("photoId")==photo.id&&copyFlight_->property("destinationId")==id)stopCopyAnimation();
}
void MainWindow::stopCopyAnimation(){
    if(copyFlight_){delete copyFlight_.data();copyFlight_=nullptr;}
}
void MainWindow::animateCopy(const QImage& snapshot,const QRect& source,const QString& photoId,const QString& destinationId){
    stopCopyAnimation();
    if(snapshot.isNull()||source.isEmpty())return;
    QWidget* destination=nullptr;
    for(int i=0;i<destinationsLayout_->count();++i){auto* widget=destinationsLayout_->itemAt(i)->widget();if(widget&&widget->property("destinationChip").toBool()&&widget->property("destinationId")==destinationId){destination=widget;break;}}
    if(!destination)return;
    if(auto* layout=centralWidget()->layout())layout->activate();
    if(auto* scroll=findChild<QScrollArea*>("destinationScroll"))scroll->ensureWidgetVisible(destination,8,0);
    auto* surface=centralWidget();const auto center=destination->mapTo(surface,destination->rect().center());
    auto* flight=new CopyFlight(snapshot,source,QRectF(QPointF(center)-QPointF(10,8),QSizeF(20,16)),surface);copyFlight_=flight;
    flight->setProperty("photoId",photoId);flight->setProperty("destinationId",destinationId);flight->show();flight->raise();
    auto* animation=new QPropertyAnimation(flight,"progress",flight);animation->setDuration(520);animation->setStartValue(0.0);animation->setEndValue(1.0);animation->setEasingCurve(QEasingCurve::Linear);
    connect(animation,&QPropertyAnimation::finished,flight,&QObject::deleteLater);animation->start();
}
void MainWindow::openFolder(const QString& folder) {
    stopCopyAnimation();
    if(!folder_.isEmpty())sorting_.savePosition(folder_,controller_.index()>=0?controller_.catalog()[controller_.index()].id:QString());
    positionTimer_.stop();
    ++folderEpoch_; controller_.clear(); resizeTimer_.stop(); folder_ = QFileInfo(folder).canonicalFilePath();if(folder_.isEmpty())folder_=QDir(folder).absolutePath();
    sorting_.openFolderSession(folder_,[this]{QMessageBox box(QMessageBox::Question,"Choose saved progress","The folder progress and local progress both changed. Both will be preserved. Choose the session to continue.",QMessageBox::NoButton,this);auto* local=box.addButton("Use local progress",QMessageBox::AcceptRole);box.addButton("Use folder progress",QMessageBox::RejectRole);box.exec();return box.clickedButton()==local;});
    restorePhoto_=sorting_.restoredPhoto();progressNotice_->setText(sorting_.progressNotice());progressNotice_->setVisible(!progressNotice_->text().isEmpty());
    filmstrip_->setCatalog({});filmstrip_->hide();photoPopup_->setPhoto({});photoPopup_->setHistogramImage({});layoutPhotoTools();
    viewer_->resetPhoto(); viewer_->setLoading(false); viewer_->setMessage("Scanning folder…");
    folderLabel_->setText(QDir(folder_).dirName()); folderLabel_->setToolTip(folder_);
    dimensionsLabel_->clear(); typeLabel_->clear(); zoomLabel_->clear(); updateStatus();
    scanner_.start(folder_, folderEpoch_,sorting_.excludedFolders());
    positionTimer_.start();
}
void MainWindow::navigate(int index) {
    if (controller_.catalog().isEmpty()) return;
    index = qBound(0,index,int(controller_.catalog().size())-1);
    if (index == controller_.index()) return;
    viewer_->resetPhoto(); dimensionsLabel_->clear(); zoomLabel_->clear();
    const auto e=sorting_.edits(controller_.catalog()[index].id);viewer_->setEdits(e);editPanel_->setEdits(e);
    controller_.select(index, viewer_->displayTarget()); updateStatus();
    filmstrip_->setCurrentIndex(index);updatePhotoPopup();
    positionTimer_.start();
}
void MainWindow::syncSavedDestinations(){
    // Saved copies remain removable even after their shortcut mapping is deleted.
    QSet<QString> configured;for(const auto& folder:sorting_.config().folders)configured.insert(folder.id);
    QHash<QString,ProcessingTask> saved;const int index=controller_.index();
    if(index>=0)for(const auto& task:sorting_.assignments(controller_.catalog()[index].id))if(!configured.contains(task.categoryId))saved.insert(task.categoryId,task);
    bool changed=false;QSet<QString> present;
    for(int i=destinationsLayout_->count()-1;i>=0;--i){auto* widget=destinationsLayout_->itemAt(i)->widget();if(!widget||!widget->property("savedDestination").toBool())continue;
        const auto id=widget->property("destinationId").toString();
        if(saved.contains(id)&&widget->property("destinationPath")==saved[id].directory){if(widget->property("destinationChip").toBool())present.insert(id);continue;}
        auto* item=destinationsLayout_->takeAt(i);widget->deleteLater();delete item;destinationCounts_.remove(id);changed=true;
    }
    for(auto it=saved.cbegin();it!=saved.cend();++it)if(!present.contains(it.key())){
        const auto& task=it.value();auto* chip=new DestinationChip({},task.categoryName,this);chip->setProperty("destinationChip",true);chip->setFocusPolicy(Qt::NoFocus);
        auto* count=new QLabel("…",this);count->setObjectName("destinationPhotoCount");count->setProperty("folderName",task.categoryName);count->setAlignment(Qt::AlignCenter);count->setFixedSize(22,22);count->setStyleSheet("background:#353840;color:#cbd0da;border:1px solid #4b505b;border-radius:11px;font-size:11px;");
        for(auto* widget:QList<QWidget*>{chip,count}){widget->setProperty("savedDestination",true);widget->setProperty("destinationId",task.categoryId);widget->setProperty("destinationPath",task.directory);}
        int position=destinationsLayout_->count()-1;for(int i=0;i<destinationsLayout_->count();++i)if(auto* widget=destinationsLayout_->itemAt(i)->widget();widget&&widget->objectName()=="quickAddDestination"){position=i;break;}
        destinationsLayout_->insertWidget(position,chip);destinationsLayout_->insertWidget(position+1,count);destinationCounts_.insert(task.categoryId,count);
        connect(chip,&QPushButton::clicked,this,[this,id=task.categoryId]{toggleDestination(id);viewer_->setFocus();});changed=true;
    }
    if(changed){QHash<QString,QString> paths;for(int i=0;i<destinationsLayout_->count();++i)if(auto* widget=destinationsLayout_->itemAt(i)->widget();widget&&widget->property("destinationChip").toBool())paths.insert(widget->property("destinationId").toString(),widget->property("destinationPath").toString());destinationCounter_->setFolders(paths,sorting_.config().outputRoot);}
}
void MainWindow::updateStatus(const DecodedImage* result) {
    const int index = controller_.index();
    editPanel_->setEnabled(index>=0&&sorting_.ready());
    findChild<QPushButton*>("photoInfoButton")->setEnabled(index>=0||!photoPopup_->isHidden());
    findChild<QPushButton*>("histogramButton")->setEnabled(index>=0||!photoPopup_->isHidden());
    updateResetButton();
    positionLabel_->setText(QString("%1 / %2").arg(index + 1).arg(controller_.catalog().size()));
    if (index >= 0) {
        const auto& photo = controller_.catalog()[index];
        filenameLabel_->setText(photo.name()); filenameLabel_->setToolTip(photo.sources.join('\n'));
        typeLabel_->setText(photo.typeLabel());
    } else filenameLabel_->setText("← → Browse   ·   Z 100%");
    syncSavedDestinations();
    for(int i=0;i<destinationsLayout_->count();++i)if(auto* button=qobject_cast<QPushButton*>(destinationsLayout_->itemAt(i)->widget());button&&button->property("destinationChip").toBool()){
        const auto id=button->property("destinationId").toString();
        const auto task=index>=0?sorting_.assignment(controller_.catalog()[index].id,id):ProcessingTask{};
        const bool assigned=index>=0&&sorting_.hasCompletedCopy(controller_.catalog()[index].id,id);
        const bool removing=task.removalRequested&&(task.status=="removing"||task.retryOperation);
        button->setEnabled(index>=0&&sorting_.ready()&&!viewer_->isCropping()&&!removing);
        if(button->property("assigned").toBool()!=assigned){button->setProperty("assigned",assigned);button->style()->unpolish(button);button->style()->polish(button);button->update();}
        const bool pending=!task.id.isEmpty()&&(task.status=="queued"||task.status=="processing"||task.status=="prepared"||task.retryOperation);
        const QString action=removing?"Removing copy…":assigned?"Copied here · Click or press this folder's shortcut to remove":pending?"Saving… · Click or press this folder's shortcut to cancel":"Click or press this folder's shortcut to copy";
        const auto path=assigned?(!task.outputPath.isEmpty()?task.outputPath:task.predecessorPath):button->property("destinationPath").toString();
        button->setToolTip(action+"\n"+QDir::toNativeSeparators(path)+(task.error.isEmpty()?QString():"\n"+task.error));
        button->setAccessibleDescription(action);
    }
    if (result) {
        const auto dimensions=viewer_->editedSourceSize();
        if (dimensions.isValid()) dimensionsLabel_->setText(QString("%1 × %2").arg(dimensions.width()).arg(dimensions.height()));
        dimensionsLabel_->setToolTip(result->note);
    }
}
void MainWindow::toggleFullscreen() {
    auto* layout=qobject_cast<QVBoxLayout*>(centralWidget()->layout());
    if(isFullScreen()){
        wasMaximized_?showMaximized():showNormal();layout->insertWidget(0,topBar_);viewer_->showToast({});
    }else{
        wasMaximized_=isMaximized();layout->removeWidget(topBar_);showFullScreen();
        viewer_->showToast("Double-click to leave fullscreen · Move to top for controls");
    }
    if(auto* button=findChild<QPushButton*>("fullscreenButton")){button->setIcon(uiIcon(isFullScreen()?"exitFullscreen":"fullscreen"));button->setAccessibleName(isFullScreen()?"Exit full screen":"Full screen");button->setToolTip(isFullScreen()?"Leave fullscreen · Double-click / F11 / Esc":"Full screen · F11");}
    topBar_->setVisible(!isFullScreen());
    statusRow_->setVisible(!isFullScreen());applyViewerPreferences();
}
bool MainWindow::eventFilter(QObject* target,QEvent* event){
    auto* widget=qobject_cast<QWidget*>(target);
    if(event->type()==QEvent::Wheel)if(auto* scroll=findChild<QScrollArea*>("destinationScroll");scroll&&target==scroll->viewport()&&scroll->horizontalScrollBar()->maximum()>0){
        auto* wheel=static_cast<QWheelEvent*>(event);const auto pixel=wheel->pixelDelta(),angle=wheel->angleDelta();const int delta=!pixel.isNull()?(pixel.x()?pixel.x():pixel.y()):(angle.x()?angle.x():angle.y());scroll->horizontalScrollBar()->setValue(scroll->horizontalScrollBar()->value()-delta);return true;
    }
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
    stopCopyAnimation();
    QMainWindow::resizeEvent(event);
    if(isFullScreen())topBar_->setGeometry(0,0,centralWidget()->width(),42);
    layoutPhotoTools();
    updateOutputButton();
    typeLabel_->setVisible(width()>=780);dimensionsLabel_->setVisible(width()>=700);outputLabel_->setVisible(width()>=900);
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
        dialog->setWindowModality(Qt::WindowModal);dialog->setProperty("suspendPhotoShortcuts",true);
        connect(dialog,&QDialog::finished,this,[this]{applyViewerPreferences();activateWindow();viewer_->setFocus();});
        connect(dialog,&SettingsDialog::viewerPreferencesChanged,this,&MainWindow::applyViewerPreferences);
        connect(dialog,&SettingsDialog::editingRequested,this,[this]{if(auto* button=findChild<QPushButton*>("toggleEdit"))button->setChecked(true);viewer_->setFocus();});
    }
    settingsDialog_->showPage(SettingsDialog::Page(page));
}
void MainWindow::rebuildDestinations() {
    stopCopyAnimation();
    destinationEditor_=nullptr;
    if(auto* scroll=findChild<QScrollArea*>("destinationScroll"))scroll->setFixedHeight(44);
    const auto output=sorting_.config().outputRoot;
    updateOutputButton();
    while(auto* item=destinationsLayout_->takeAt(0)){if(item->widget())item->widget()->deleteLater();delete item;}
    destinationCounts_.clear();
    QHash<QString,QString> folderPaths;
    QVector<QPair<QString,QKeySequence>> bindings;
    for(const auto& folder:sorting_.config().folders){
        bindings.append({folder.id,QKeySequence(folder.shortcut,QKeySequence::PortableText)});
        bindings.append({folder.id,QKeySequence(folder.alternateShortcut,QKeySequence::PortableText)});
        auto* button=new DestinationChip(folder.shortcut,folder.name,this);
        button->setProperty("destinationChip",true);button->setProperty("destinationId",folder.id);button->setFocusPolicy(Qt::NoFocus);
        button->setProperty("destinationPath",QDir(sorting_.config().outputRoot).filePath(folder.name));
        button->setToolTip(QString("Copy to ")+QDir(sorting_.config().outputRoot).filePath(folder.name)+(folder.alternateShortcut.isEmpty()?QString():"\nAlternate: "+folder.alternateShortcut)); destinationsLayout_->addWidget(button);
        auto* count=new QLabel("…",this);count->setObjectName("destinationPhotoCount");count->setProperty("destinationId",folder.id);count->setProperty("folderName",folder.name);
        count->setAlignment(Qt::AlignCenter);count->setFixedSize(22,22);count->setStyleSheet("background:#353840;color:#cbd0da;border:1px solid #4b505b;border-radius:11px;font-size:11px;");
        count->setToolTip("Counting photos…");destinationsLayout_->addWidget(count);destinationsLayout_->addSpacing(5);
        destinationCounts_.insert(folder.id,count);folderPaths.insert(folder.id,QDir(sorting_.config().outputRoot).filePath(folder.name));
        connect(button,&QPushButton::clicked,this,[this,id=folder.id]{toggleDestination(id);viewer_->setFocus();});
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
    editPanel_->setShortcutBindings(shortcuts_->editBindings());
    if(bindings.isEmpty())destinationsLayout_->addWidget(new QLabel("Add a sorting folder",this));
    auto* add=new QPushButton(uiIcon("plus"),QString(),this);add->setObjectName("quickAddDestination");add->setFixedSize(28,28);add->setFocusPolicy(Qt::NoFocus);add->setToolTip("Create sorting folder");add->setAccessibleName("Create sorting folder");add->setEnabled(sorting_.ready());destinationsLayout_->addWidget(add);
    connect(add,&QPushButton::clicked,this,[this]{editDestination();});
    destinationsLayout_->addStretch();
    destinationCounter_->setFolders(folderPaths,sorting_.config().outputRoot);
    updateStatus();
}
void MainWindow::destinationMenu(const QString& id,const QPoint& position){
    QMenu menu(this);menu.setObjectName("destinationContextMenu");
    auto* rename=menu.addAction("Rename…");rename->setObjectName("renameDestination");
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
    editor->show();activateWindow();name->setFocus();name->selectAll();
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
        dialog->setWindowModality(Qt::WindowModal);dialog->setProperty("suspendPhotoShortcuts",true);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog,&QDialog::finished,this,[this]{editPanel_->setShortcutBindings(shortcuts_->editBindings());activateWindow();viewer_->setFocus();});
    }
    shortcutsDialog_->show();shortcutsDialog_->raise();shortcutsDialog_->activateWindow();
}
void MainWindow::applyViewerPreferences(){
    const auto preferences=sorting_.uiPreferences();const auto position=preferences.value("filmstripPosition").toString("left");
    auto* grid=qobject_cast<QGridLayout*>(photoPage_->layout());
    int row=2,column=1;if(position=="top")row=0;else if(position=="left"){row=1;column=0;}else if(position=="right"){row=1;column=2;}
    if(filmstrip_->property("stripPosition").toString()!=position){grid->removeWidget(filmstrip_);filmstrip_->setVertical(column!=1);grid->addWidget(filmstrip_,row,column);filmstrip_->setProperty("stripPosition",position);}
    filmstrip_->setVisible(preferences.value("showFilmstrip").toBool(true)&&!controller_.catalog().isEmpty());
}
void MainWindow::updatePhotoPopup(){
    const int index=controller_.index();if(index<0)return;
    photoPopup_->setPhoto(controller_.catalog()[index]);
    if(photoPopup_->isVisible()&&photoPopup_->mode()==PhotoPopup::Histogram)photoPopup_->setHistogramImage(viewer_->histogramImage());
}
void MainWindow::togglePhotoPopup(bool histogram){
    const auto mode=histogram?PhotoPopup::Histogram:PhotoPopup::Information;
    const bool hide=photoPopup_->isVisible()&&photoPopup_->mode()==mode;
    if(hide)photoPopup_->hide();
    else {updatePhotoPopup();photoPopup_->setMode(mode);if(mode==PhotoPopup::Histogram)photoPopup_->setHistogramImage(viewer_->histogramImage());photoPopup_->show();photoPopup_->reposition();photoPopup_->setFocus();}
    findChild<QPushButton*>("photoInfoButton")->setChecked(!hide&&!histogram);findChild<QPushButton*>("histogramButton")->setChecked(!hide&&histogram);
    layoutPhotoTools();if(!hide)informationTools_->verticalScrollBar()->setValue(0);
}
void MainWindow::layoutPhotoTools(){
    const bool info=!photoPopup_->isHidden(),editing=!editScroll_->isHidden();
    const int cardWidth=info&&editing?qMin(336,qMax(180,(viewer_->width()-52)/2)):336;
    photoPopup_->setProperty("cardWidth",cardWidth);editScroll_->setFixedWidth(cardWidth);
    photoPopup_->reposition();
    editScroll_->setFixedHeight(qMax(editPanel_->sizeHint().height(),editPanel_->minimumSizeHint().height())+12);
    const int railWidth=cardWidth+8,height=qMax(100,viewer_->height()-24);
    photoTools_->setGeometry(qMax(0,viewer_->width()-railWidth-12),12,railWidth,height);
    informationTools_->setGeometry(12,12,railWidth,height);
    photoTools_->setVisible(editing);informationTools_->setVisible(info);photoTools_->raise();informationTools_->raise();
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
    const bool error=!issues.isEmpty()||!lastProblem_.isEmpty();
    queueButton_->setStyleSheet(error?"padding:2px 0;border:0;background:transparent;font-size:11px;color:#e0b58e;":"padding:2px 0;border:0;background:transparent;font-size:11px;color:#99999f;");
    const auto details=issues.isEmpty()?lastProblem_:issues.join('\n');queueButton_->setToolTip(details.isEmpty()?fullStatus+"\nFailures retry automatically":details);
    errorLabel_->setVisible(error);
    if(error){const auto first=details.section('\n',0,0);errorLabel_->setText(processingDetails_?details:"Processing error: "+first.left(180)+(first.size()>180?"…":"")+" · Click processing status for details.");errorLabel_->setToolTip(details);}
}
}
