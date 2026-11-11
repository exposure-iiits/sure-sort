#include "sorting_controller.h"
#include "main_window.h"
#include "photo_viewer.h"
#include "edit_panel.h"
#include "destination_dialog.h"
#include "shortcut_manager.h"
#include "settings_dialog.h"
#include "watermark_dialog.h"
#include "output_dialog.h"
#include "destination_counter.h"
#include "raw_fixture.h"
#include <QAction>
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSlider>
#include <QSqlQuery>
#include <QSignalSpy>
#include <QKeySequenceEdit>
#include <QHeaderView>
#include <QTemporaryDir>
#include <QTest>
#include <QTableWidget>
#include <QTabBar>
#include <QLineEdit>
#include <QJsonDocument>
#include <atomic>
using namespace photocull;
namespace {
bool write(const QString& path,const QByteArray& bytes){QFile file(path);return file.open(QIODevice::WriteOnly)&&file.write(bytes)==bytes.size();}
QByteArray read(const QString& path){QFile file(path);return file.open(QIODevice::ReadOnly)?file.readAll():QByteArray();}
QByteArray jpeg(){QImage image(600,400,QImage::Format_RGB32);image.fill(QColor(60,120,150));QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);image.save(&buffer,"JPEG",95);return bytes;}
DestinationConfig config(const QString& root){return {root,{{"a","A","1"},{"b","B","2"},{"m","Third","3"},{"n","Fourth","4"}}};}
bool sql(const QString& path,const std::function<bool(QSqlDatabase&)>& action){const auto name=newId();bool result;{auto db=QSqlDatabase::addDatabase("QSQLITE",name);db.setDatabaseName(path);result=db.open()&&action(db);db.close();}QSqlDatabase::removeDatabase(name);return result;}
}
class WorkflowTest:public QObject {
    Q_OBJECT
private slots:
    void headerMenusAreModalWindows(){
        QTemporaryDir dir;
        MainWindow window(12*1024*1024,nullptr,dir.filePath("session.sqlite"));window.show();
        auto* viewer=window.findChild<PhotoViewer*>();
        auto* settings=window.findChild<QPushButton*>("settingsButton");
        auto* shortcuts=window.findChild<QPushButton*>("allShortcuts");
        QVERIFY(viewer->isVisible());
        for(int i=0;i<2;++i){
            settings->click();QPointer<SettingsDialog> dialog=window.findChild<SettingsDialog*>();QVERIFY(dialog&&dialog->isVisible());QVERIFY(viewer->isVisible());
            QVERIFY(dialog->isWindow());QCOMPARE(dialog->windowModality(),Qt::WindowModal);QTRY_COMPARE(QApplication::activeModalWidget(),dialog.data());
            dialog->close();QTRY_VERIFY(dialog.isNull());
            shortcuts->click();QVERIFY(viewer->isVisible());
            QPointer<QDialog> reference=window.findChild<QDialog*>("shortcutReference");QVERIFY(reference&&reference->isVisible());
            QVERIFY(reference->isWindow());QCOMPARE(reference->windowModality(),Qt::WindowModal);QTRY_COMPARE(QApplication::activeModalWidget(),reference.data());
            QTest::keyClick(reference,Qt::Key_Escape);QVERIFY(viewer->isVisible());
            QTRY_VERIFY(reference.isNull());
        }
        window.shutdown();
    }
    void nextFolderShortcutSkipsUsedAndConflictingKeys(){
        QWidget window;ShortcutManager manager(&window);DestinationConfig folders{"C:/output",{{"one","One","1","2"}}};
        QCOMPARE(manager.nextDestinationKey(folders),QString("3"));
        for(int n=3;n<=10;++n)folders.folders.append({newId(),QString::number(n),QString::number(n%10)});
        QCOMPARE(manager.nextDestinationKey(folders),QString("Ctrl+1"));
        QString error;auto app=manager.appBindings();auto row=app[0].toObject();row["trigger"]="Ctrl+1";app[0]=row;QVERIFY(manager.setAppBindings(app,error));
        QCOMPARE(manager.nextDestinationKey(folders),QString("Ctrl+2"));
        DestinationDialog dialog(folders,&manager);dialog.findChild<QPushButton*>("addDestination")->click();
        QCOMPARE(dialog.configuration().folders.last().shortcut,QString("Ctrl+2"));
    }
    void duplicateNamesSurviveImportExportAndUndo_data(){
        QTest::addColumn<bool>("edited");
        QTest::newRow("copy")<<false;QTest::newRow("edited-copy")<<true;

    }
    void duplicateNamesSurviveImportExportAndUndo(){
        QFETCH(bool,edited);QTemporaryDir dir;
        const auto input=dir.filePath("input"),database=dir.filePath("session.sqlite");
        QVERIFY(QDir().mkpath(input+"/camera1"));QVERIFY(QDir().mkpath(input+"/camera2/deep"));
        QImage image(64,48,QImage::Format_RGB32);image.fill(Qt::red);QVERIFY(image.save(input+"/camera1/IMG_1.jpg"));
        image.fill(Qt::blue);QVERIFY(image.save(input+"/camera2/deep/IMG_1.jpg"));
        const auto photos=scanFolder(input).photos;QCOMPARE(photos.size(),2);QVERIFY(photos[0].id!=photos[1].id);
        const auto first=read(photos[0].sources.first()),second=read(photos[1].sources.first());QStringList outputs;
        {SortingController c;QVERIFY(c.initialize(database));QVERIFY(c.configure(config(dir.filePath("output"))));
            for(const auto& photo:photos){if(edited){PhotoEdits e;e.brightness=5;QVERIFY(c.setEdits(photo,e));}QVERIFY(!c.assign(photo,"a").isEmpty());}
            QTRY_COMPARE(c.remaining(),0);QVERIFY(c.issues().isEmpty());
            for(const auto& photo:photos){const auto task=c.assignment(photo.id);QCOMPARE(task.status,QString("completed"));outputs.append(task.outputPath);}
            QVERIFY(outputs[0]!=outputs[1]);QCOMPARE(QDir(QFileInfo(outputs[0]).absolutePath()).entryList({"*.jpg"},QDir::Files).size(),2);
            const auto a=QImage(outputs[0]).pixelColor(10,10),b=QImage(outputs[1]).pixelColor(10,10);
            QVERIFY(a.red()>a.blue()+150);QVERIFY(b.blue()>b.red()+150);
        }
        {SortingController c;QVERIFY(c.initialize(database));for(const auto& photo:photos)QCOMPARE(c.assignments(photo.id).size(),1);
            c.undo();QTRY_VERIFY(!QFileInfo::exists(outputs[1]));QVERIFY(QFileInfo::exists(outputs[0]));
            c.undo();QTRY_VERIFY(!QFileInfo::exists(outputs[0]));QCOMPARE(read(photos[0].sources.first()),first);QCOMPARE(read(photos[1].sources.first()),second);
        }
    }
    void quickFolderResetAndFullscreen(){
        QTemporaryDir dir;const auto input=dir.filePath("input"),database=dir.filePath("session.sqlite");QVERIFY(QDir().mkpath(input));QVERIFY(write(input+"/photo.jpg",jpeg()));
        {MainWindow window(12*1024*1024,nullptr,database);window.show();window.openFolder(input);auto* viewer=window.findChild<PhotoViewer*>();QTRY_VERIFY(viewer->hasPhoto());
            window.findChild<QPushButton*>("quickAddDestination")->click();
            QPointer<SettingsDialog> settings=window.findChild<SettingsDialog*>();QVERIFY(settings&&settings->isVisible());
            QCOMPARE(settings->findChild<QListWidget*>("settingsSidebar")->currentRow(),int(SettingsDialog::Sorting));
            QVERIFY(!window.findChild<QWidget*>("quickDestinationEditor"));
            settings->findChild<QLineEdit*>("destinationOutputRoot")->setText(dir.filePath("output"));
            settings->findChild<QPushButton*>("saveSettingsPage")->click();settings->close();QTRY_VERIFY(settings.isNull());
            window.findChild<QPushButton*>("quickAddDestination")->click();QPointer<QWidget> dialog=window.findChild<QWidget*>("quickDestinationEditor");QVERIFY(dialog);QVERIFY(!dialog->isWindow());QCOMPARE(dialog->window(),&window);
            QVERIFY(dialog->findChildren<QPushButton*>().isEmpty());
            auto* name=dialog->findChild<QLineEdit*>("quickFolderName");QVERIFY(name->hasFocus());
            name->setText("../bad");QTest::keyClick(name,Qt::Key_Return);QVERIFY(!name->toolTip().isEmpty());
            name->clear();QTest::keyClicks(name,"Keep");QCOMPARE(name->text(),QString("Keep"));QTest::keyClick(name,Qt::Key_Return);QTRY_VERIFY(dialog.isNull());QVERIFY(QFileInfo(dir.filePath("output/Keep")).isDir());
            window.findChild<QPushButton*>("quickAddDestination")->click();dialog=window.findChild<QWidget*>("quickDestinationEditor");name=dialog->findChild<QLineEdit*>("quickFolderName");
            if(!qEnvironmentVariable("SURESORT_UI_CAPTURE").isEmpty()){QTest::qWait(100);QVERIFY(window.grab().save(QDir(qEnvironmentVariable("SURESORT_UI_CAPTURE")).filePath("inline-folder.png")));}
            QTest::keyClicks(name,"Second");viewer->setFocus();QTRY_VERIFY(dialog.isNull());
            QVERIFY(QFileInfo(dir.filePath("output/Second")).isDir());
            QPushButton* second=nullptr;for(auto* button:window.findChildren<QPushButton*>())if(button->text()=="2  Second")second=button;
            QVERIFY(second);QVERIFY(!second->property("destinationAction").isValid());
            window.findChild<QPushButton*>("quickAddDestination")->click();dialog=window.findChild<QWidget*>("quickDestinationEditor");
            name=dialog->findChild<QLineEdit*>("quickFolderName");QTest::keyClicks(name,"Canceled");
            QTest::keyClick(name,Qt::Key_Escape);QTRY_VERIFY(dialog.isNull());QVERIFY(!QFileInfo::exists(dir.filePath("output/Canceled")));
            QPushButton* chip=nullptr;for(auto* button:window.findChildren<QPushButton*>())if(button->property("destinationChip").toBool())chip=button;QVERIFY(chip);
            QTimer::singleShot(0,&window,[&]{auto* menu=window.findChild<QMenu*>("destinationContextMenu");QVERIFY(menu);QCOMPARE(menu->actions().size(),1);for(auto* action:menu->actions())QVERIFY(!action->text().contains("delete",Qt::CaseInsensitive));QVERIFY(!menu->findChild<QAction*>("destinationMode_move"));menu->close();});
            QMetaObject::invokeMethod(chip,"customContextMenuRequested",Q_ARG(QPoint,QPoint(5,5)));
            QTRY_VERIFY(window.findChildren<QPushButton*>().size()>0);QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
            auto* reset=window.findChild<QPushButton*>("resetPhotoEdits");QVERIFY(!reset->isVisible());window.activateWindow();viewer->setFocus();
            QTest::keyPress(viewer,Qt::Key_E);QTest::keyClick(viewer,Qt::Key_Right);QTest::keyRelease(viewer,Qt::Key_E);QVERIFY(reset->isVisible());reset->click();QVERIFY(viewer->edits().isIdentity());QVERIFY(!reset->isVisible());
            QTest::mouseDClick(viewer,Qt::LeftButton);QVERIFY(window.isFullScreen());QTest::qWait(60);QTest::mouseMove(viewer,viewer->rect().center());QTest::mouseMove(viewer,QPoint(viewer->width()/2,2));QTRY_VERIFY(window.findChild<QWidget*>("toolbar")->isVisible());
            window.findChild<QPushButton*>("fullscreenButton")->click();QVERIFY(!window.isFullScreen());QTest::qWait(100);
            QTest::mouseDClick(viewer,Qt::LeftButton);QTRY_VERIFY(window.isFullScreen());QTest::qWait(100);
            QTest::mouseDClick(viewer,Qt::LeftButton);QTRY_VERIFY(!window.isFullScreen());
            window.shutdown();
        }
        {SortingController c;QVERIFY(c.initialize(database));QCOMPARE(c.config().folders.size(),2);QCOMPARE(c.config().folders[0].shortcut,QString("1"));QCOMPARE(c.config().folders[1].shortcut,QString("2"));}
    }
    void copyMultipleFoldersAndUndoAfterRestart(){
        QTemporaryDir dir;const auto source=dir.filePath("photo.jpg"),database=dir.filePath("session.sqlite");const auto bytes=jpeg();QVERIFY(write(source,bytes));Photo photo{"photo",{source}};
        {SortingController c;QVERIFY(c.initialize(database));QVERIFY(c.configure(config(dir.filePath("out"))));c.assign(photo,"a");c.assign(photo,"b");QTRY_COMPARE(c.remaining(),0);QCOMPARE(c.assignments(photo.id).size(),2);QCOMPARE(read(c.assignment(photo.id,"a").outputPath),bytes);QCOMPARE(read(c.assignment(photo.id,"b").outputPath),bytes);QCOMPARE(read(source),bytes);}
        {SortingController c;QVERIFY(c.initialize(database));QCOMPARE(c.assignments(photo.id).size(),2);c.undo();QTRY_VERIFY(!QFileInfo::exists(dir.filePath("out/B/photo.jpg")));QVERIFY(QFileInfo::exists(dir.filePath("out/A/photo.jpg")));QCOMPARE(c.assignments(photo.id).size(),1);c.undo();QTRY_VERIFY(!QFileInfo::exists(dir.filePath("out/A/photo.jpg")));QCOMPARE(read(source),bytes);}
    }
    void folderRowControls(){
        QTemporaryDir dir;MainWindow window(12*1024*1024,nullptr,dir.filePath("session.sqlite"));ShortcutManager manager(&window);DestinationDialog dialog(config(dir.path()),&manager,&window);dialog.show();QTest::qWait(100);
        auto* table=dialog.findChild<QTableWidget*>("destinationFolders");QVERIFY(table);QCOMPARE(table->columnCount(),3);
        for(auto* button:dialog.findChildren<QPushButton*>())QVERIFY(button->text()!="Up"&&button->text()!="Down"&&button->text()!="Remove"&&button->text()!="Add");
        auto* second=qobject_cast<QPushButton*>(table->cellWidget(1,2));QVERIFY(second&&!second->icon().isNull());table->setCurrentCell(0,0);second->click();QCOMPARE(dialog.configuration().folders[1].id,QString("m"));
        qobject_cast<QPushButton*>(table->cellWidget(1,2))->click();QCOMPARE(dialog.configuration().folders[1].id,QString("n"));
        auto* add=dialog.findChild<QPushButton*>("addDestination");QVERIFY(add&&!add->icon().isNull());add->click();QCOMPARE(dialog.configuration().folders.last().shortcut,QString("2"));
        const auto key=table->cellWidget(0,1);QVERIFY(key->height()>=key->minimumSizeHint().height());
        if(qEnvironmentVariableIsSet("SURESORT_SCREENSHOT")){dialog.setFocus();QTest::qWait(100);QVERIFY(dialog.grab().save(qEnvironmentVariable("SURESORT_SCREENSHOT")+".destinations.png"));}
        window.shutdown();
    }
    void automaticRetryPreservesSnapshot(){
        QTemporaryDir dir;const auto source=dir.filePath("photo.jpg");QVERIFY(write(source,jpeg()));Photo photo{"photo",{source}};std::atomic_int attempts{0};
        SortingController c(nullptr,[&](ProcessingTask task,const CancelToken& cancel){++attempts;if(attempts==1){task.status="failed";task.error="Temporary read failure";return task;}return ProcessingPipeline::prepare(task,cancel);},200);
        QVERIFY(c.initialize(dir.filePath("session.sqlite")));QVERIFY(c.configure(config(dir.filePath("out"))));OutputSettings original;original.jpegQuality=91;QVERIFY(c.setOutput(original));QSignalSpy errors(&c,&SortingController::problem);const auto id=c.assign(photo,"a");
        QTRY_VERIFY(c.assignment(photo.id).retryOperation!=0);QVERIFY(c.issues().isEmpty());QCOMPARE(c.failed(),0);QCOMPARE(c.remaining(),1);OutputSettings changed;changed.jpegQuality=83;QVERIFY(c.setOutput(changed));
        QTRY_COMPARE(c.assignment(photo.id).status,QString("completed"));QCOMPARE(attempts.load(),2);QCOMPARE(c.assignment(photo.id).id,id);QCOMPARE(c.assignment(photo.id).output.jpegQuality,91);QCOMPARE(errors.size(),0);QCOMPARE(QDir(dir.filePath("out/A")).entryList({"*.jpg"},QDir::Files).size(),1);c.undo();QTRY_VERIFY(!QFileInfo::exists(dir.filePath("out/A/photo.jpg")));QVERIFY(!c.canUndo());
    }
    void automaticRetriesStopAndSurviveRestart(){
        QTemporaryDir dir;const auto source=dir.filePath("photo.jpg"),database=dir.filePath("session.sqlite");QVERIFY(write(source,jpeg()));Photo photo{"photo",{source}};std::atomic_int attempts{0};
        auto fail=[&](ProcessingTask task,const CancelToken&){++attempts;task.status="failed";task.error="Destination unavailable";return task;};
        {SortingController c(nullptr,fail,1000);QVERIFY(c.initialize(database));QVERIFY(c.configure(config(dir.filePath("out"))));c.assign(photo,"a");QTRY_VERIFY(c.assignment(photo.id).retryOperation);QCOMPARE(c.assignment(photo.id).automaticRetries,1);QVERIFY(c.issues().isEmpty());}
        {SortingController c(nullptr,fail,10);QSignalSpy errors(&c,&SortingController::problem);QVERIFY(c.initialize(database));QTRY_COMPARE(c.failed(),1);QCOMPARE(attempts.load(),3);QCOMPARE(c.assignment(photo.id).automaticRetries,2);QCOMPARE(c.assignment(photo.id).retryOperation,0);QCOMPARE(errors.size(),1);QVERIFY(c.issues().first().contains("Automatic retries exhausted"));QTest::qWait(100);QCOMPARE(attempts.load(),3);QCOMPARE(read(source),jpeg());}
        {SortingController c(nullptr,fail,10);QVERIFY(c.initialize(database));QTest::qWait(100);QCOMPARE(attempts.load(),3);QCOMPARE(c.failed(),1);}
    }
    void publicationCollisionRetriesWithoutDecoding(){
        QTemporaryDir dir;const auto source=dir.filePath("photo.jpg");const auto bytes=jpeg();QVERIFY(write(source,bytes));Photo photo{"photo",{source}};std::atomic_int decodes{0};
        SortingController c(nullptr,[&](ProcessingTask task,const CancelToken& cancel){++decodes;auto result=ProcessingPipeline::prepare(task,cancel);if(result.status=="prepared")write(result.outputPath,"external output");return result;},20);
        QVERIFY(c.initialize(dir.filePath("session.sqlite")));QVERIFY(c.configure(config(dir.filePath("out"))));QSignalSpy errors(&c,&SortingController::problem);const auto id=c.assign(photo,"a");QTRY_COMPARE(c.assignment(photo.id).status,QString("completed"));QCOMPARE(decodes.load(),1);QCOMPARE(c.assignment(photo.id).id,id);QCOMPARE(read(dir.filePath("out/A/photo.jpg")),QByteArray("external output"));QCOMPARE(read(c.assignment(photo.id).outputPath),bytes);QCOMPARE(errors.size(),0);c.undo();QTRY_VERIFY(!QFileInfo::exists(dir.filePath("out/A/photo_2.jpg")));QCOMPARE(read(dir.filePath("out/A/photo.jpg")),QByteArray("external output"));
    }
    void interruptedAutomaticCleanupRecovers(){
        QTemporaryDir dir;const auto source=dir.filePath("photo.jpg"),database=dir.filePath("session.sqlite");const auto bytes=jpeg();QVERIFY(write(source,bytes));Photo photo{"photo",{source}};
        {SortingController c;QVERIFY(c.initialize(database));QVERIFY(c.configure(config(dir.filePath("out"))));c.assign(photo,"a");QTRY_COMPARE(c.assignment(photo.id).status,QString("completed"));}
        QVERIFY(sql(database,[](QSqlDatabase& db){QSqlQuery q(db);return q.exec("DELETE FROM assignments")&&q.exec("DELETE FROM undo_history")&&q.exec("UPDATE tasks SET status='preserved',retry_operation=4,automatic_retries=1");}));
        {SortingController c(nullptr,ProcessingPipeline::prepare,10);QVERIFY(c.initialize(database));QTRY_VERIFY(!QFileInfo::exists(dir.filePath("out/A/photo.jpg")));QCOMPARE(read(source),bytes);QVERIFY(c.issues().isEmpty());QVERIFY(!c.canUndo());}
    }
    void exhaustedFailureAppearsInFooter(){
        QTemporaryDir dir;const auto database=dir.filePath("session.sqlite");Photo photo{"missing",{dir.filePath("missing.jpg")}};
        {SortingController c(nullptr,ProcessingPipeline::prepare,10);QVERIFY(c.initialize(database));QVERIFY(c.configure(config(dir.filePath("out"))));c.assign(photo,"a");QTRY_COMPARE(c.failed(),1);}
        MainWindow window(12*1024*1024,nullptr,database);window.resize(900,650);window.show();QTest::qWait(100);auto* error=window.findChild<QLabel*>("processingError");QVERIFY(error&&error->isVisible());QVERIFY(error->text().contains("Automatic retries exhausted"));QVERIFY(error->toolTip().contains("missing.jpg"));auto* status=window.findChild<QPushButton*>("processingStatus");QVERIFY(status->text().contains("Failed: 1"));QVERIFY(status->geometry().right()<window.width());
        if(qEnvironmentVariableIsSet("SURESORT_SCREENSHOT")){QVERIFY(window.grab().save(qEnvironmentVariable("SURESORT_SCREENSHOT")+".error.png"));}
        window.shutdown();
    }
    void undoAndCancelStopAutomaticRetries(){
        for(bool cancel:{false,true}){QTemporaryDir dir;Photo photo{"photo",{dir.filePath("missing.jpg")}};std::atomic_int attempts{0};SortingController c(nullptr,[&](ProcessingTask task,const CancelToken&){++attempts;task.status="failed";task.error="Unavailable original";return task;},150);QVERIFY(c.initialize(dir.filePath("session.sqlite")));QVERIFY(c.configure(config(dir.filePath("out"))));c.assign(photo,"a");QTRY_VERIFY(c.assignment(photo.id).retryOperation);if(cancel)c.cancelPending();else c.undo();QTest::qWait(250);QCOMPARE(attempts.load(),1);QCOMPARE(c.remaining(),0);if(cancel)QCOMPARE(c.assignment(photo.id).status,QString("cancelled"));}
    }
    void originalCropRatio_data(){QTest::addColumn<QSize>("size");QTest::addColumn<int>("rotation");for(const auto size:{QSize(600,400),QSize(400,600),QSize(400,400)})for(int rotation:{0,1})QTest::newRow(qPrintable(QString::number(size.width())+"x"+QString::number(size.height())+"-"+QString::number(rotation)))<<size<<rotation;}
    void originalCropRatio(){QFETCH(QSize,size);QFETCH(int,rotation);PhotoViewer viewer;viewer.resize(620,620);viewer.show();QImage image(size,QImage::Format_RGB32);image.fill(Qt::gray);viewer.setPreview({image,size,{},{}});PhotoEdits e;e.rotation=rotation;viewer.setEdits(e);viewer.setCropAspect(-1);viewer.beginCrop();const auto rotated=rotation?size.transposed():size;const double scale=qMin(620.0/rotated.width(),620.0/rotated.height());const QPointF top((620-rotated.width()*scale)/2,(620-rotated.height()*scale)/2);auto point=[&](double x,double y){return (top+QPointF(x*rotated.width()*scale,y*rotated.height()*scale)).toPoint();};QTest::mousePress(&viewer,Qt::LeftButton,Qt::NoModifier,point(.1,.1));QTest::mouseMove(&viewer,point(.7,.5));QTest::mouseRelease(&viewer,Qt::LeftButton,Qt::NoModifier,point(.7,.5));const auto crop=viewer.cropSelection();QVERIFY(crop.width()>.1);QVERIFY(qAbs(crop.width()-crop.height())<.01);viewer.confirmCrop();QVERIFY(qAbs(double(viewer.editedSourceSize().width())/viewer.editedSourceSize().height()-double(rotated.width())/rotated.height())<.02);}
    void uiShortcutsStepsZoomAndBrand(){
        QTemporaryDir dir;const auto input=dir.filePath("input"),database=dir.filePath("session.sqlite");QVERIFY(QDir().mkpath(input));QVERIFY(write(input+"/photo.jpg",jpeg()));QString error;{SessionStore store;QVERIFY(store.open(database,error));QVERIFY(store.saveConfig(config(dir.filePath("out")),error));}
        MainWindow window(12*1024*1024,nullptr,database);window.show();window.openFolder(input);auto* viewer=window.findChild<PhotoViewer*>();QVERIFY(viewer);QTest::qWait(300);QCOMPARE(window.windowTitle(),QString("Sure Sort"));
        QTest::keyClick(viewer,Qt::Key_Z);QVERIFY(!viewer->isFit());QCOMPARE(viewer->zoomPercent(),100.0);QTest::keyClick(viewer,Qt::Key_Z);QVERIFY(viewer->isFit());
        QTest::mouseClick(window.findChild<QPushButton*>("fullscreenButton"),Qt::LeftButton);QVERIFY(window.isFullScreen());QTest::keyClick(viewer,Qt::Key_F11);QVERIFY(!window.isFullScreen());
        const int holds[]={Qt::Key_E,Qt::Key_B,Qt::Key_C,Qt::Key_H,Qt::Key_S,Qt::Key_T};for(int hold:holds){QTest::keyPress(viewer,Qt::Key(hold));QTest::keyClick(viewer,Qt::Key_Right);QTest::keyRelease(viewer,Qt::Key(hold));}
        const auto e=viewer->edits();QCOMPARE(e.exposure,5);QCOMPARE(e.brightness,5);QCOMPARE(e.contrast,5);QCOMPARE(e.highlights,5);QCOMPARE(e.shadows,5);QCOMPARE(e.saturation,5);
        auto* fullscreen=window.findChild<QPushButton*>("fullscreenButton");QVERIFY(fullscreen&&!fullscreen->icon().isNull());QVERIFY(fullscreen->text().isEmpty());auto* settings=window.findChild<QPushButton*>("settingsButton");QVERIFY(settings&&!settings->menu());QVERIFY(!settings->icon().isNull());QVERIFY(window.findChild<QAction*>("outputSettings"));QVERIFY(window.findChild<QAction*>("destinationSettings"));QVERIFY(window.findChild<QPushButton*>("undoAction"));for(auto* button:window.findChildren<QPushButton*>())QVERIFY(button->text()!="Retry"&&button->text()!="Cancel queue"&&button->text()!="Destinations…");QVERIFY(!window.findChild<QLabel*>("processingError")->isVisible());QVERIFY(window.findChild<QWidget*>("photoFooter")->height()<=115);
        QTest::mouseClick(window.findChild<QPushButton*>("allShortcuts"),Qt::LeftButton);auto* shortcuts=window.findChild<QDialog*>("shortcutReference");QVERIFY(shortcuts);auto* table=shortcuts->findChild<QTableWidget*>();QVERIFY(table);shortcuts->findChild<QTabBar*>("shortcutCategories")->setCurrentIndex(2);bool undo=false,folder=false;for(int row=0;row<table->rowCount();++row){if(table->item(row,0)->text()=="Undo last copy or removal"&&table->item(row,1)->text()=="Ctrl+Z")undo=true;if(table->item(row,0)->text()=="Copy / remove: Third")folder=true;}QVERIFY(undo);QVERIFY(folder);shortcuts->close();
        QTest::mouseClick(window.findChild<QPushButton*>("toggleEdit"),Qt::LeftButton);auto* panel=window.findChild<EditPanel*>();QVERIFY(panel->findChild<QComboBox*>("cropAspect")->findText("Original ratio")>=0);
        if(qEnvironmentVariableIsSet("SURESORT_SCREENSHOT")){QTest::qWait(150);QVERIFY(window.grab().save(qEnvironmentVariable("SURESORT_SCREENSHOT")));}
        QTest::keyClick(viewer,Qt::Key_1);QTRY_VERIFY(QFileInfo::exists(dir.filePath("out/A/photo.jpg")));QTest::keyClick(viewer,Qt::Key_Z,Qt::ControlModifier);QTRY_VERIFY(!QFileInfo::exists(dir.filePath("out/A/photo.jpg")));window.shutdown();
    }
    void compactLayoutSettingsCropAndUndo(){
        QTemporaryDir dir;
        const auto input=dir.filePath("input"),database=dir.filePath("session.sqlite");
        QVERIFY(QDir().mkpath(input));
        const auto fixture=qEnvironmentVariable("SURESORT_UI_FIXTURE");
        const auto original=fixture.isEmpty()?jpeg():read(fixture);
        QVERIFY(write(input+"/photo.jpg",original));
        QString error;
        {SessionStore store;QVERIFY(store.open(database,error));QVERIFY(store.saveConfig(config(dir.filePath("out")),error));}
        MainWindow window(16*1024*1024,nullptr,database);window.show();window.openFolder(input);
        auto* viewer=window.findChild<PhotoViewer*>();QVERIFY(viewer);QTRY_VERIFY(viewer->hasPhoto());
        QVERIFY(window.findChild<QWidget*>("toolbar")->height()<=46);
        QVERIFY(window.findChild<QWidget*>("photoFooter")->height()<=90);
        QVERIFY(!window.findChild<QWidget*>("cropBar")->isVisible());
        auto* undo=window.findChild<QPushButton*>("undoAction");QVERIFY(undo&&!undo->isEnabled());
        auto capture=[&](const QString& name,QWidget* widget){const auto path=qEnvironmentVariable("SURESORT_UI_CAPTURE");if(!path.isEmpty()){QTest::qWait(600);QVERIFY(widget->grab().save(QDir(path).filePath(name+".png")));}};
        capture("main",&window);

        QTest::keyClick(viewer,Qt::Key_Comma,Qt::ControlModifier);
        QPointer<SettingsDialog> settings=window.findChild<SettingsDialog*>();QVERIFY(settings&&settings->isVisible());
        auto* sidebar=settings->findChild<QListWidget*>("settingsSidebar");QVERIFY(sidebar);QCOMPARE(sidebar->count(),6);
        QCOMPARE(sidebar->currentRow(),int(SettingsDialog::Sorting));
        capture("settings-sorting",settings);
        auto* folders=settings->findChild<QTableWidget*>("destinationFolders");QVERIFY(folders);folders->item(0,0)->setText("Selected");
        settings->findChild<QPushButton*>("saveSettingsPage")->click();
        QCOMPARE(settings->findChild<QLabel*>("settingsFeedback")->text(),QString("Saved."));
        QPushButton* destination=nullptr;for(auto* button:window.findChildren<QPushButton*>())if(button->property("destinationId").toString()=="a")destination=button;
        QVERIFY(destination&&destination->text().contains("Selected"));
        sidebar->setCurrentRow(SettingsDialog::Watermark);
        QVERIFY(settings->findChild<WatermarkDialog*>()->isVisible());
        QVERIFY(!settings->findChild<QPushButton*>("saveSettingsPage")->isVisible());
        capture("settings-watermark",settings);
        sidebar->setCurrentRow(SettingsDialog::Saving);
        auto* output=settings->findChild<OutputDialog*>();QVERIFY(output&&output->isVisible());
        output->findChild<QSlider*>("jpegQuality")->setValue(90);
        capture("settings-saving",settings);
        sidebar->setCurrentRow(SettingsDialog::Editing);
        QVERIFY(settings->findChild<QSlider*>("editingStep")->isVisible());
        capture("settings-editing",settings);
        sidebar->setCurrentRow(SettingsDialog::About);
        QVERIFY(settings->findChild<QLabel*>("aboutVersion")->text().contains("Version 0.1.4"));
        capture("settings-about",settings);
        settings->findChild<QPushButton*>("closeSettings")->click();QTRY_VERIFY(settings.isNull()||!settings->isVisible());

        viewer->setFocus();QTest::keyClick(viewer,Qt::Key_K);
        QTRY_VERIFY(viewer->isCropping());QVERIFY(window.findChild<QWidget*>("cropBar")->isVisible());
        auto* ratio=window.findChild<QComboBox*>("cropRatio");QVERIFY(ratio);ratio->setCurrentText("1:1");
        auto dragCrop=[&]{const QPoint from(viewer->width()/3,viewer->height()/3),to(viewer->width()*2/3,viewer->height()*2/3);QTest::mousePress(viewer,Qt::LeftButton,Qt::NoModifier,from);QTest::mouseMove(viewer,to);QTest::mouseRelease(viewer,Qt::LeftButton,Qt::NoModifier,to);};
        dragCrop();capture("crop",&window);
        window.findChild<QPushButton*>("cancelCrop")->click();QVERIFY(!viewer->isCropping());QVERIFY(!window.findChild<QWidget*>("cropBar")->isVisible());QVERIFY(viewer->edits().crop==QRectF(0,0,1,1));
        QTest::keyClick(viewer,Qt::Key_K);dragCrop();window.findChild<QPushButton*>("applyCrop")->click();
        QVERIFY(!viewer->isCropping());QVERIFY(qAbs(viewer->editedSourceSize().width()-viewer->editedSourceSize().height())<=2);
        QTest::mouseClick(destination,Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(dir.filePath("out/Selected/photo.jpg")),10000);
        QTRY_VERIFY(undo->isEnabled());QTest::mouseClick(undo,Qt::LeftButton);
        QTRY_VERIFY(!QFileInfo::exists(dir.filePath("out/Selected/photo.jpg")));
        QCOMPARE(read(input+"/photo.jpg"),original);
        window.shutdown();
    }
    void shortcutEditorAndStepPersistAcrossRestart(){
        QTemporaryDir dir;
        const auto input=dir.filePath("input"),database=dir.filePath("session.sqlite");
        QVERIFY(QDir().mkpath(input));QVERIFY(write(input+"/photo.jpg",jpeg()));
        QString error;{SessionStore store;QVERIFY(store.open(database,error));QVERIFY(store.saveConfig(config(dir.filePath("out")),error));}
        auto capture=[&](const QString& name,QWidget* widget){const auto path=qEnvironmentVariable("SURESORT_UI_CAPTURE");if(!path.isEmpty()){QTest::qWait(150);QVERIFY(widget->grab().save(QDir(path).filePath(name+".png")));}};
        {
            MainWindow window(12*1024*1024,nullptr,database);window.show();window.openFolder(input);
            auto* viewer=window.findChild<PhotoViewer*>();QTRY_VERIFY(viewer->hasPhoto());
            for(auto* chip:window.findChildren<QPushButton*>())if(chip->property("destinationChip").toBool()){
                QVERIFY(!chip->text().contains(" · Copy"));QVERIFY(!chip->text().contains(" · Move"));
                QVERIFY(!chip->property("destinationAction").isValid());
            }
            window.findChild<QPushButton*>("settingsButton")->click();
            QPointer<SettingsDialog> settings=window.findChild<SettingsDialog*>();QVERIFY(settings);
            QVERIFY(settings->isWindow());QCOMPARE(settings->windowModality(),Qt::WindowModal);
            auto* sidebar=settings->findChild<QListWidget*>("settingsSidebar");
            QCOMPARE(sidebar->count(),6);for(int i=0;i<sidebar->count();++i)QVERIFY(sidebar->item(i)->text()!="Shortcuts");
            sidebar->setCurrentRow(SettingsDialog::Editing);
            auto* step=settings->findChild<QSlider*>("editingStep");QCOMPARE(step->value(),5);step->setValue(7);
            QVERIFY(settings->findChild<QLabel*>("editingStepValue")->text().contains("0.7 EV"));
            settings->close();QTRY_VERIFY(settings.isNull());
            window.findChild<QPushButton*>("allShortcuts")->click();
            QPointer<QDialog> reference=window.findChild<QDialog*>("shortcutReference");QVERIFY(reference);
            QVERIFY(reference->isWindow());QCOMPARE(reference->windowModality(),Qt::WindowModal);QVERIFY(!window.findChild<QLabel*>("appLogo"));
            auto* table=reference->findChild<QTableWidget*>();QCOMPARE(table->columnCount(),3);
            QCOMPARE(table->rowCount(),5);QVERIFY(!reference->findChild<QWidget*>("shortcutEditor"));capture("shortcuts",reference);
            auto edit=[&](const QString& action,int slot=0){auto* tabs=reference->findChild<QTabBar*>("shortcutCategories");tabs->setCurrentIndex(action.startsWith("assign:")?2:action.startsWith("edit:")?3:action=="fit"?1:0);
                const auto id=action.startsWith("assign:")?action.mid(7):action;
                for(int i=0;i<table->rowCount();++i)if(table->item(i,0)->data(Qt::UserRole+1).toString()==id){
                    table->scrollToItem(table->item(i,slot+1));
                    QTest::mouseClick(table->viewport(),Qt::LeftButton,Qt::NoModifier,table->visualItemRect(table->item(i,slot+1)).center());
                    QTest::mouseDClick(table->viewport(),Qt::LeftButton,Qt::NoModifier,table->visualItemRect(table->item(i,slot+1)).center());return;
                }
                QFAIL("Shortcut cell not found");
            };
            auto key=[&](Qt::Key trigger){auto* input=reference->findChild<QLineEdit*>("shortcutCapture");QVERIFY(input);QTest::keyClick(input,trigger);QCoreApplication::processEvents();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);};
            edit("next");QVERIFY(reference->findChild<QLineEdit*>("shortcutCapture"));
            key(Qt::Key_Left);QVERIFY(reference->findChild<QLabel*>("shortcutToast")->isVisible());
            auto* manager=window.findChild<ShortcutManager*>();QVERIFY(manager);
            QCOMPARE(manager->appBindings()[0].toObject()["trigger"].toString(),QString("Right"));capture("shortcut-conflict",&window);
            edit("next");key(Qt::Key_PageDown);
            QCOMPARE(manager->appBindings()[0].toObject()["trigger"].toString(),QString("PgDown"));
            edit("next",1);key(Qt::Key_PageUp);
            edit("next");key(Qt::Key_Escape);
            QCOMPARE(manager->appBindings()[0].toObject()["trigger"].toString(),QString("PgDown"));
            auto chord=[&](Qt::Key trigger){auto* input=reference->findChild<QLineEdit*>("shortcutCapture");QVERIFY(input);QTest::keyPress(input,Qt::Key_U);QTest::keyClick(input,trigger);QTest::keyRelease(input,Qt::Key_U);QCoreApplication::processEvents();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);};
            edit("edit:exposureUp");chord(Qt::Key_Up);
            edit("edit:exposureUp",1);chord(Qt::Key_Down);
            QCOMPARE(manager->editBindings()[0].toObject()["hold"].toString(),QString("U"));
            edit("assign:a");key(Qt::Key_9);edit("assign:a",1);key(Qt::Key_8);
            edit("fit");key(Qt::Key_Delete);
            reference->close();QTRY_VERIFY(reference.isNull());
            window.findChild<QPushButton*>("toggleEdit")->setChecked(true);
            window.findChild<QPushButton*>("editingShortcutsButton")->click();reference=window.findChild<QDialog*>("shortcutReference");QVERIFY(reference);
            QCOMPARE(reference->findChild<QTabBar*>("shortcutCategories")->currentIndex(),3);QCOMPARE(reference->findChild<QTableWidget*>()->accessibleName(),QString("Editing shortcuts"));
            capture("editing-category",&window);
            reference->close();QTRY_VERIFY(reference.isNull());
            window.activateWindow();viewer->setFocus();
            QTest::keyPress(viewer,Qt::Key_U);QTest::keyClick(viewer,Qt::Key_Up);QTest::keyRelease(viewer,Qt::Key_U);QCOMPARE(viewer->edits().exposure,7);
            QTest::keyPress(viewer,Qt::Key_B);QTest::keyClick(viewer,Qt::Key_Right);QTest::keyRelease(viewer,Qt::Key_B);QCOMPARE(viewer->edits().brightness,7);
            QSignalSpy actions(manager,&ShortcutManager::triggered);QTest::keyClick(viewer,Qt::Key_PageDown);QCOMPARE(actions.last().first().toString(),QString("next"));
            QTest::keyClick(viewer,Qt::Key_9);QTRY_VERIFY(QFileInfo::exists(dir.filePath("out/A/photo.jpg")));
            capture("main-updated",&window);window.shutdown();
        }
        {
            MainWindow window(12*1024*1024,nullptr,database);window.show();window.openFolder(input);
            auto* viewer=window.findChild<PhotoViewer*>();QTRY_VERIFY(viewer->hasPhoto());
            auto* manager=window.findChild<ShortcutManager*>();
            QCOMPARE(manager->appBindings()[0].toObject()["trigger"].toString(),QString("PgDown"));
            QCOMPARE(manager->editBindings()[0].toObject()["hold"].toString(),QString("U"));
            QCOMPARE(manager->editBindings()[0].toObject()["alternate"].toString(),QString("Down"));
            QSignalSpy alternateActions(manager,&ShortcutManager::triggered);window.activateWindow();viewer->setFocus();QTest::keyClick(viewer,Qt::Key_PageUp);QCOMPARE(alternateActions.last().first().toString(),QString("next"));
            QTest::keyClick(viewer,Qt::Key_8);QCOMPARE(alternateActions.last().first().toString(),QString("assign:a"));
            bool disabledFit=false;for(const auto& binding:manager->appBindings())if(binding.toObject()["action"]=="fit")disabledFit=binding.toObject()["trigger"].toString().isEmpty();QVERIFY(disabledFit);
            QPushButton* chip=nullptr;for(auto* button:window.findChildren<QPushButton*>())if(button->property("destinationId").toString()=="a")chip=button;
            QVERIFY(chip&&chip->text().startsWith("9  "));
            window.activateWindow();viewer->setFocus();
            QTest::keyPress(viewer,Qt::Key_U);QTest::keyClick(viewer,Qt::Key_Up);QTest::keyRelease(viewer,Qt::Key_U);QCOMPARE(viewer->edits().exposure,14);
            window.findChild<QPushButton*>("settingsButton")->click();auto* settings=window.findChild<SettingsDialog*>();
            settings->showPage(SettingsDialog::Editing);QCOMPARE(settings->findChild<QSlider*>("editingStep")->value(),7);
            window.shutdown();
        }
    }
    void destinationBadgesTrackDiskAndUndo(){
        QTemporaryDir dir;
        const auto database=dir.filePath("session.sqlite"),input=dir.filePath("input"),output=dir.filePath("out");
        QVERIFY(QDir().mkpath(input));QVERIFY(write(input+"/photo.jpg",jpeg()));
        QVERIFY(QDir().mkpath(output+"/A/Archive"));
        QVERIFY(write(output+"/A/existing.JPG",jpeg()));QVERIFY(write(output+"/A/existing.dng","raw companion"));
        QVERIFY(write(output+"/A/other.PNG","supported photo"));QVERIFY(write(output+"/A/readme.txt","not a photo"));
        QVERIFY(write(output+"/A/Archive/archived.jpg",jpeg()));
        QString error;{SessionStore store;QVERIFY(store.open(database,error));QVERIFY(store.saveConfig(config(output),error));}
        MainWindow window(12*1024*1024,nullptr,database);window.show();window.openFolder(input);
        auto* viewer=window.findChild<PhotoViewer*>();QTRY_VERIFY(viewer->hasPhoto());
        auto badge=[&](const QString& id)->QLabel*{for(auto* label:window.findChildren<QLabel*>("destinationPhotoCount"))if(label->property("destinationId").toString()==id)return label;return nullptr;};
        QVERIFY(badge("a"));QTRY_COMPARE(badge("a")->text(),QString("2"));QTRY_COMPARE(badge("b")->text(),QString("0"));
        window.activateWindow();viewer->setFocus();QTest::keyClick(viewer,Qt::Key_1);
        QTRY_VERIFY(QFileInfo::exists(output+"/A/photo.jpg"));QTRY_COMPARE(badge("a")->text(),QString("3"));
        QTRY_VERIFY(window.findChild<QPushButton*>("undoAction")->isEnabled());window.findChild<QPushButton*>("undoAction")->click();
        QTRY_VERIFY(!QFileInfo::exists(output+"/A/photo.jpg"));QTRY_COMPARE(badge("a")->text(),QString("2"));
        QVERIFY(write(output+"/A/external.jpg",jpeg()));QTRY_COMPARE(badge("a")->text(),QString("3"));
        QVERIFY(QFile::remove(output+"/A/external.jpg"));QTRY_COMPARE(badge("a")->text(),QString("2"));
        // A destination created externally after opening the app is picked up too.
        QVERIFY(QDir().mkpath(output+"/B"));QVERIFY(write(output+"/B/new.jpeg",jpeg()));QTRY_COMPARE(badge("b")->text(),QString("1"));
        // A second destination copy counts once; subfolders are excluded.
        window.activateWindow();viewer->setFocus();QTest::keyClick(viewer,Qt::Key_3);
        QTRY_VERIFY(QFileInfo::exists(output+"/Third/photo.jpg"));QTRY_VERIFY(QFileInfo::exists(input+"/photo.jpg"));
        QTRY_COMPARE(badge("m")->text(),QString("1"));
        const auto capture=qEnvironmentVariable("SURESORT_UI_CAPTURE");if(!capture.isEmpty())QVERIFY(window.grab().save(QDir(capture).filePath("folder-counts.png")));
        window.findChild<QPushButton*>("undoAction")->click();QTRY_VERIFY(QFileInfo::exists(input+"/photo.jpg"));QTRY_COMPARE(badge("m")->text(),QString("0"));
        window.shutdown();
    }
    void destinationCounterIgnoresStaleConfiguration(){
        QTemporaryDir dir;const auto before=dir.filePath("before"),after=dir.filePath("after");
        QVERIFY(QDir().mkpath(before));QVERIFY(QDir().mkpath(after));
        for(int i=0;i<200;++i)QVERIFY(write(before+QString("/%1.jpg").arg(i),"photo"));
        QVERIFY(write(after+"/one.png","photo"));
        DestinationCounter counter;QHash<QString,int> counts;
        connect(&counter,&DestinationCounter::countsChanged,this,[&](const QHash<QString,int>& next){counts=next;});
        counter.setFolders({{"old",before}},dir.path());counter.setFolders({{"new",after}},dir.path());
        QTRY_COMPARE(counts.value("new",-1),1);QVERIFY(!counts.contains("old"));counter.shutdown();
    }
};
QTEST_MAIN(WorkflowTest)
#include "test_workflow.moc"
