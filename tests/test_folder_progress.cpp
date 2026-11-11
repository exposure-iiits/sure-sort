#include "sorting_controller.h"
#include "main_window.h"
#include "photo_popup.h"
#include "photo_viewer.h"
#include "filmstrip.h"
#include "copy_flight.h"
#include "raw_fixture.h"
#include <QBuffer>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QSignalSpy>
#include <atomic>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
using namespace photocull;
namespace {
QByteArray photoBytes(const char* format="JPEG"){QImage image(120,80,QImage::Format_RGB32);image.fill(QColor(55,125,175));QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);image.save(&buffer,format);return bytes;}
bool writeFile(const QString& path,const QByteArray& bytes){QFile file(path);return file.open(QIODevice::WriteOnly)&&file.write(bytes)==bytes.size();}
QByteArray readFile(const QString& path){QFile file(path);return file.open(QIODevice::ReadOnly)?file.readAll():QByteArray();}
DestinationConfig destinations(const QString& root){return {root,{{"a","Keep","1"},{"b","Best","2"}}};}
QString progress(const QString& folder){return QDir(folder).filePath(".suresort-progress.sqlite");}
void capture(QWidget* widget,const QString& name){const auto root=qEnvironmentVariable("SURESORT_UI_CAPTURE");if(!root.isEmpty()){QDir().mkpath(root);QTest::qWait(80);widget->grab().save(QDir(root).filePath(name+".png"));}}
}
class FolderProgressTest:public QObject{
    Q_OBJECT
private slots:
    void fullSessionTravelsWithOriginalFolder(){
        QTemporaryDir dir;const auto input=dir.filePath("input"),renamed=dir.filePath("renamed"),output=dir.filePath("out");QVERIFY(QDir().mkpath(input+"/nested"));const auto source=input+"/nested/photo.jpg";const auto bytes=photoBytes();QVERIFY(writeFile(source,bytes));auto photo=scanFolder(input).photos.first();PhotoEdits edits;edits.brightness=12;
        {SortingController c;QVERIFY(c.initialize(dir.filePath("profile.sqlite")));QVERIFY(c.openFolderSession(input));QVERIFY(c.configure(destinations(output)));c.registerCatalog({photo});QVERIFY(c.setEdits(photo,edits));QVERIFY(c.setOutput({90,75,true}));c.assign(photo,"a");c.assign(photo,"b");QTRY_COMPARE(c.remaining(),0);QVERIFY(c.hasCompletedCopy(photo.id));c.savePosition(input,photo.id);QCOMPARE(readFile(source),bytes);c.shutdown();}
        QVERIFY(QFileInfo::exists(progress(input)));QVERIFY(QDir().rename(input,renamed));photo=scanFolder(renamed).photos.first();
        {SortingController c;QVERIFY(c.initialize(dir.filePath("fresh-computer.sqlite")));QVERIFY(c.openFolderSession(renamed));QCOMPARE(c.config().folders.size(),2);QCOMPARE(c.config().outputRoot,output);QCOMPARE(c.edits(photo.id),edits);QVERIFY(c.output()==OutputSettings({90,75,true}));QCOMPARE(c.restoredPhoto(),photo.id);QCOMPARE(c.assignments(photo.id).size(),2);QVERIFY(c.hasCompletedCopy(photo.id));c.removeAssignment(photo,"a");QTRY_COMPARE(c.assignments(photo.id).size(),1);QVERIFY(c.hasCompletedCopy(photo.id));c.removeAssignment(photo,"b");QTRY_COMPARE(c.assignments(photo.id).size(),0);QVERIFY(!c.hasCompletedCopy(photo.id));QCOMPARE(readFile(renamed+"/nested/photo.jpg"),bytes);c.undo();QTRY_VERIFY(c.hasCompletedCopy(photo.id));c.shutdown();}
        {SessionStore store;QString error;store.setSourceRoot(renamed);QVERIFY(store.open(progress(renamed),error));SessionSnapshot snapshot;QVERIFY(store.load(snapshot,error));QVERIFY(!snapshot.undo.isEmpty());QCOMPARE(snapshot.tasks.first().photo.sources.first(),renamed+"/nested/photo.jpg");}
    }
    void independentFoldersShareOneWorker(){
        QTemporaryDir dir;const auto first=dir.filePath("one"),second=dir.filePath("two");QVERIFY(QDir().mkpath(first));QVERIFY(QDir().mkpath(second));QVERIFY(writeFile(first+"/a.jpg",photoBytes()));QVERIFY(writeFile(second+"/b.jpg",photoBytes()));const auto a=scanFolder(first).photos.first(),b=scanFolder(second).photos.first();std::atomic_int running{0},peak{0};
        SortingController c(nullptr,[&](ProcessingTask task,const CancelToken& cancel){const int active=++running;peak.store(qMax(peak.load(),active));QThread::msleep(70);auto result=ProcessingPipeline::prepare(task,cancel);--running;return result;});QVERIFY(c.initialize(dir.filePath("profile.sqlite")));QVERIFY(c.openFolderSession(first));QVERIFY(c.configure(destinations(dir.filePath("out1"))));c.assign(a,"a");QVERIFY(c.openFolderSession(second));QVERIFY(c.configure(destinations(dir.filePath("out2"))));c.assign(b,"b");QTRY_COMPARE(c.remaining(),0);QCOMPARE(peak.load(),1);QCOMPARE(c.assignments(b.id).size(),1);QVERIFY(c.assignments(a.id).isEmpty());QVERIFY(c.openFolderSession(first));QCOMPARE(c.config().outputRoot,dir.filePath("out1"));QCOMPARE(c.assignments(a.id).size(),1);QVERIFY(QFileInfo::exists(dir.filePath("out1/Keep/a.jpg")));QVERIFY(QFileInfo::exists(dir.filePath("out2/Best/b.jpg")));c.shutdown();
    }
    void internalOutputsFollowRenamedSourceFolder(){
        QTemporaryDir dir;const auto input=dir.filePath("in"),renamed=dir.filePath("renamed");QVERIFY(QDir().mkpath(input));QVERIFY(writeFile(input+"/photo.jpg",photoBytes()));auto photo=scanFolder(input).photos.first();
        {SortingController c;QVERIFY(c.initialize(dir.filePath("profile.sqlite")));QVERIFY(c.openFolderSession(input));QVERIFY(c.configure(destinations(input+"/Sorted")));c.assign(photo,"a");QTRY_COMPARE(c.remaining(),0);c.savePosition(input,photo.id);c.shutdown();}
        QVERIFY(QDir().rename(input,renamed));photo=scanFolder(renamed,{}, {renamed+"/Sorted"}).photos.first();
        {SortingController c;QVERIFY(c.initialize(dir.filePath("fresh.sqlite")));QVERIFY(c.openFolderSession(renamed));QCOMPARE(c.config().outputRoot,renamed+"/Sorted");QCOMPARE(c.assignment(photo.id).outputPath,renamed+"/Sorted/Keep/photo.jpg");QVERIFY(c.hasCompletedCopy(photo.id));c.removeAssignment(photo,"a");QTRY_VERIFY(c.assignments(photo.id).isEmpty());QVERIFY(!QFileInfo::exists(renamed+"/Sorted/Keep/photo.jpg"));QCOMPARE(readFile(renamed+"/photo.jpg"),photoBytes());}
    }
    void removeDuringRefreshAndUndo(){
        QTemporaryDir dir;QVERIFY(writeFile(dir.filePath("photo.jpg"),photoBytes()));Photo photo{"photo",{dir.filePath("photo.jpg")}};std::atomic_bool entered{false},release{false};std::atomic_int calls{0};
        SortingController c(nullptr,[&](ProcessingTask task,const CancelToken& cancel){if(++calls==2){entered=true;while(!release&&!cancel->load())QThread::msleep(2);}return ProcessingPipeline::prepare(task,cancel);},10);QVERIFY(c.initialize(dir.filePath("db.sqlite")));QVERIFY(c.configure(destinations(dir.filePath("out"))));c.assign(photo,"a");QTRY_COMPARE(c.assignment(photo.id).status,QString("completed"));PhotoEdits edits;edits.brightness=15;c.setEdits(photo,edits);QTRY_VERIFY(entered.load());c.removeAssignment(photo,"a");release=true;QTRY_COMPARE(c.assignments(photo.id).size(),0);QTRY_COMPARE(c.remaining(),0);QVERIFY(!QFileInfo::exists(dir.filePath("out/Keep/photo.jpg")));QVERIFY(!c.hasCompletedCopy(photo.id));c.undo();QTRY_COMPARE(c.assignment(photo.id).status,QString("completed"));QVERIFY(c.hasCompletedCopy(photo.id));QCOMPARE(readFile(photo.sources.first()),photoBytes());
    }
    void modifiedOutputIsPreservedAndStillMarked(){
        QTemporaryDir dir;const auto source=dir.filePath("photo.jpg");QVERIFY(writeFile(source,photoBytes()));Photo photo{"photo",{source}};SortingController c(nullptr,ProcessingPipeline::prepare,10);QVERIFY(c.initialize(dir.filePath("db.sqlite")));QVERIFY(c.configure(destinations(dir.filePath("out"))));c.assign(photo,"a");QTRY_COMPARE(c.assignment(photo.id).status,QString("completed"));const auto output=c.assignment(photo.id).outputPath;QVERIFY(writeFile(output,"user changed this copy"));c.removeAssignment(photo,"a");QTRY_COMPARE(c.assignment(photo.id).status,QString("preserved"));QTRY_COMPARE(c.assignment(photo.id).retryOperation,0);QVERIFY(c.hasCompletedCopy(photo.id));QCOMPARE(c.assignments(photo.id).size(),1);QCOMPARE(readFile(output),QByteArray("user changed this copy"));QCOMPARE(readFile(source),photoBytes());QVERIFY(!c.issues().isEmpty());
    }
    void removalRecoversAfterRestart(){
        QTemporaryDir dir;const auto input=dir.filePath("in"),profile=dir.filePath("profile.sqlite");QVERIFY(QDir().mkpath(input));QVERIFY(writeFile(input+"/photo.jpg",photoBytes()));const auto photo=scanFolder(input).photos.first();ProcessingTask saved;
        {SortingController c;QVERIFY(c.initialize(profile));QVERIFY(c.openFolderSession(input));QVERIFY(c.configure(destinations(dir.filePath("out"))));c.assign(photo,"a");QTRY_COMPARE(c.assignment(photo.id).status,QString("completed"));saved=c.assignment(photo.id);c.shutdown();}
        {SessionStore store;QString error;store.setSourceRoot(input);QVERIFY(store.open(progress(input),error));saved.removalRequested=true;saved.status="removing";QVERIFY(store.saveTask(saved,error));}
        {SortingController c;QVERIFY(c.initialize(profile));QVERIFY(c.openFolderSession(input));QTRY_VERIFY(c.assignments(photo.id).isEmpty());QVERIFY(!QFileInfo::exists(saved.outputPath));c.undo();QTRY_COMPARE(c.assignment(photo.id).status,QString("completed"));QCOMPARE(readFile(input+"/photo.jpg"),photoBytes());}
    }
    void queuedCopyRecoversAndMissingOutputLosesGreen(){
        QTemporaryDir dir;const auto input=dir.filePath("in"),profile=dir.filePath("profile.sqlite");QVERIFY(QDir().mkpath(input));QVERIFY(writeFile(input+"/photo.jpg",photoBytes()));const auto photo=scanFolder(input).photos.first();
        {SortingController c;QVERIFY(c.initialize(profile));QVERIFY(c.openFolderSession(input));QVERIFY(c.configure(destinations(dir.filePath("out"))));c.assign(photo,"a");c.shutdown();}
        QString output;{SortingController c;QVERIFY(c.initialize(profile));QVERIFY(c.openFolderSession(input));QTRY_COMPARE(c.assignment(photo.id).status,QString("completed"));output=c.assignment(photo.id).outputPath;c.shutdown();}
        QVERIFY(QFile::remove(output));{SortingController c;QVERIFY(c.initialize(profile));QVERIFY(c.openFolderSession(input));c.registerCatalog({photo});QVERIFY(!c.hasCompletedCopy(photo.id));QCOMPARE(c.assignment(photo.id).status,QString("missing"));c.assign(photo,"a");QTRY_COMPARE(c.assignment(photo.id).status,QString("completed"));QVERIFY(c.hasCompletedCopy(photo.id));}
    }
    void destinationsBelowSourceAreExcluded(){
        QTemporaryDir dir;const auto input=dir.filePath("in");QVERIFY(QDir().mkpath(input));QVERIFY(writeFile(input+"/photo.jpg",photoBytes()));const auto photo=scanFolder(input).photos.first();SortingController c;QVERIFY(c.initialize(dir.filePath("profile.sqlite")));QVERIFY(c.openFolderSession(input));QVERIFY(c.configure(destinations(input+"/Sorted")));c.assign(photo,"a");QTRY_COMPARE(c.remaining(),0);QCOMPARE(scanFolder(input,{},c.excludedFolders()).photos.size(),1);QCOMPARE(scanFolder(input).photos.size(),2);QVERIFY(!c.configure({QFileInfo(input).absolutePath(),{{"bad",QFileInfo(input).fileName(),"3"}}}));
    }
    void originalsUnchangedForPngAndRawPair(){
        QTemporaryDir dir;const auto png=dir.filePath("image.png"),raw=dir.filePath("pair.dng"),jpeg=dir.filePath("pair.jpg");const auto rawBytes=fixture::rawDng({});QVERIFY(writeFile(png,photoBytes("PNG")));QVERIFY(writeFile(raw,rawBytes));QVERIFY(writeFile(jpeg,photoBytes()));SortingController c;QVERIFY(c.initialize(dir.filePath("db.sqlite")));QVERIFY(c.configure(destinations(dir.filePath("out"))));c.assign({"png",{png}},"a");c.assign({"pair",{raw,jpeg}},"a");QTRY_COMPARE(c.remaining(),0);QCOMPARE(readFile(png),photoBytes("PNG"));QCOMPARE(readFile(jpeg),photoBytes());QCOMPARE(readFile(raw),rawBytes);c.removeAssignment({"pair",{raw,jpeg}},"a");QTRY_VERIFY(c.assignment("pair").id.isEmpty());QCOMPARE(readFile(raw),rawBytes);QCOMPARE(readFile(jpeg),photoBytes());
    }
    void readOnlyProgressFallsBackAndSyncs(){
#ifdef Q_OS_WIN
        QTemporaryDir dir;const auto input=dir.filePath("in"),profile=dir.filePath("profile.sqlite");QVERIFY(QDir().mkpath(input));QVERIFY(writeFile(input+"/photo.jpg",photoBytes()));const auto photo=scanFolder(input).photos.first();
        {SortingController c;QVERIFY(c.initialize(profile));QVERIFY(c.openFolderSession(input));QVERIFY(c.configure(destinations(dir.filePath("out"))));c.shutdown();}
        const auto file=progress(input);QVERIFY(SetFileAttributesW(reinterpret_cast<LPCWSTR>(file.utf16()),FILE_ATTRIBUTE_READONLY));
        {SortingController c;QVERIFY(c.initialize(profile));QVERIFY(c.openFolderSession(input));QCOMPARE(c.progressNotice(),QString("Progress saved on this computer only."));c.assign(photo,"b");QTRY_COMPARE(c.assignment(photo.id).status,QString("completed"));c.shutdown();}
        QVERIFY(SetFileAttributesW(reinterpret_cast<LPCWSTR>(file.utf16()),FILE_ATTRIBUTE_NORMAL));
        {SortingController c;QVERIFY(c.initialize(profile));QVERIFY(c.openFolderSession(input));QVERIFY(c.progressNotice().isEmpty());QVERIFY(c.hasCompletedCopy(photo.id));c.shutdown();}
        {SortingController c;QVERIFY(c.initialize(dir.filePath("another.sqlite")));QVERIFY(c.openFolderSession(input));QVERIFY(c.hasCompletedCopy(photo.id));}
#else
        QSKIP("Windows read-only attribute fixture");
#endif
    }
    void lockedProgressCannotSortThroughProfile(){
        QTemporaryDir dir;QVERIFY(writeFile(dir.filePath("photo.jpg"),photoBytes()));const auto input=dir.path();SortingController first;QVERIFY(first.initialize(dir.filePath("one.sqlite")));QVERIFY(first.openFolderSession(input));SortingController second;QVERIFY(second.initialize(dir.filePath("two.sqlite")));QVERIFY(!second.openFolderSession(input));QVERIFY(!second.ready());QVERIFY(second.assign(scanFolder(input).photos.first(),"a").isEmpty());
    }
    void folderRenameUpdatesCopiesAndUndo(){
        QTemporaryDir dir;const auto input=dir.filePath("in"),profile=dir.filePath("profile.sqlite");QVERIFY(QDir().mkpath(input));QVERIFY(writeFile(input+"/photo.jpg",photoBytes()));const auto photo=scanFolder(input).photos.first();
        {SortingController c;QVERIFY(c.initialize(profile));QVERIFY(c.openFolderSession(input));QVERIFY(c.configure(destinations(input+"/Sorted")));c.assign(photo,"a");QTRY_COMPARE(c.assignment(photo.id).status,QString("completed"));QString error;QVERIFY(c.renameDestination("a","Renamed",error));QCOMPARE(c.assignment(photo.id).categoryName,QString("Renamed"));QCOMPARE(c.assignment(photo.id).outputPath,input+"/Sorted/Renamed/photo.jpg");QVERIFY(QFileInfo::exists(c.assignment(photo.id).outputPath));c.shutdown();}
        {SortingController c;QVERIFY(c.initialize(profile));QVERIFY(c.openFolderSession(input));QCOMPARE(c.config().folders.first().name,QString("Renamed"));c.undo();QTRY_VERIFY(!QFileInfo::exists(input+"/Sorted/Renamed/photo.jpg"));QCOMPARE(readFile(input+"/photo.jpg"),photoBytes());}
    }
    void divergentFallbackPreservesBothSessions(){
#ifdef Q_OS_WIN
        QTemporaryDir dir;const auto input=dir.filePath("in"),profile=dir.filePath("profile.sqlite");QVERIFY(QDir().mkpath(input));QVERIFY(writeFile(input+"/photo.jpg",photoBytes()));const auto photo=scanFolder(input).photos.first();
        {SortingController c;QVERIFY(c.initialize(profile));QVERIFY(c.openFolderSession(input));QVERIFY(c.configure(destinations(dir.filePath("out"))));c.shutdown();}
        const auto file=progress(input);QVERIFY(SetFileAttributesW(reinterpret_cast<LPCWSTR>(file.utf16()),FILE_ATTRIBUTE_READONLY));
        {SortingController c;QVERIFY(c.initialize(profile));QVERIFY(c.openFolderSession(input));c.assign(photo,"b");QTRY_COMPARE(c.remaining(),0);c.shutdown();}
        QVERIFY(SetFileAttributesW(reinterpret_cast<LPCWSTR>(file.utf16()),FILE_ATTRIBUTE_NORMAL));
        {SessionStore store;QString error;store.setSourceRoot(input);QVERIFY(store.open(file,error));QVERIFY(store.setMetadata("changed_elsewhere","1",error));}
        {SortingController c;QVERIFY(c.initialize(profile));bool chosen=false;QVERIFY(c.openFolderSession(input,[&]{chosen=true;return true;}));QVERIFY(chosen);QVERIFY(c.hasCompletedCopy(photo.id));QVERIFY(!QDir(input).entryList({"*.conflict-*.sqlite"},QDir::Files|QDir::Hidden).isEmpty());c.shutdown();}
#else
        QSKIP("Windows read-only attribute fixture");
#endif
    }
    void lockedOutputCanRetryRemoval(){
#ifdef Q_OS_WIN
        QTemporaryDir dir;const auto source=dir.filePath("photo.jpg");QVERIFY(writeFile(source,photoBytes()));Photo photo{"photo",{source}};SortingController c(nullptr,ProcessingPipeline::prepare,10);QVERIFY(c.initialize(dir.filePath("db.sqlite")));QVERIFY(c.configure(destinations(dir.filePath("out"))));c.assign(photo,"a");QTRY_COMPARE(c.assignment(photo.id).status,QString("completed"));const auto path=c.assignment(photo.id).outputPath;
        struct Lock{HANDLE value;~Lock(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}}lock{CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr)};QVERIFY(lock.value!=INVALID_HANDLE_VALUE);
        c.removeAssignment(photo,"a");QTRY_COMPARE(c.assignment(photo.id).status,QString("preserved"));QTRY_COMPARE(c.assignment(photo.id).retryOperation,0);QVERIFY(c.hasCompletedCopy(photo.id));CloseHandle(lock.value);lock.value=INVALID_HANDLE_VALUE;c.removeAssignment(photo,"a");QTRY_VERIFY(c.assignments(photo.id).isEmpty());QVERIFY(!QFileInfo::exists(path));QCOMPARE(readFile(source),photoBytes());
#else
        QSKIP("Windows handle locking fixture");
#endif
    }
    void footerFolderTogglesAndPersistentPanels(){
        QTemporaryDir dir;const auto input=dir.filePath("in"),other=dir.filePath("other"),output=dir.filePath("out"),profile=dir.filePath("profile.sqlite");QVERIFY(QDir().mkpath(input));QVERIFY(QDir().mkpath(other));QVERIFY(writeFile(input+"/photo.jpg",photoBytes()));QVERIFY(writeFile(other+"/other.jpg",photoBytes()));
        {SortingController c;QVERIFY(c.initialize(profile));QVERIFY(c.openFolderSession(input));QVERIFY(c.configure(destinations(output)));const auto photo=scanFolder(input).photos.first();c.assign(photo,"a");c.assign(photo,"b");QTRY_COMPARE(c.remaining(),0);c.shutdown();}
        MainWindow window(12*1024*1024,nullptr,profile);window.show();window.openFolder(input);auto* viewer=window.findChild<PhotoViewer*>();QTRY_VERIFY(viewer->hasPhoto());
        auto chip=[&](const QString& id)->QPushButton*{for(auto* button:window.findChildren<QPushButton*>())if(button->property("destinationChip").toBool()&&button->property("destinationId")==id)return button;return nullptr;};
        QVERIFY(chip("a"));QTRY_VERIFY(chip("a")->property("assigned").toBool());QVERIFY(chip("b")->property("assigned").toBool());QVERIFY(!window.findChild<QPushButton*>("manageCopies"));capture(&window,"footer-toggles-1280");
        window.findChild<QPushButton*>("photoInfoButton")->click();auto* popup=window.findChild<PhotoPopup*>();QVERIFY(popup->isVisible());QTest::mouseClick(viewer,Qt::LeftButton,Qt::NoModifier,viewer->rect().center());QVERIFY(popup->isVisible());QTest::keyClick(viewer,Qt::Key_Escape);QVERIFY(popup->isVisible());window.openFolder(other);QTRY_VERIFY(viewer->hasPhoto());QVERIFY(popup->isVisible());window.openFolder(input);QTRY_VERIFY(viewer->hasPhoto());window.findChild<QPushButton*>("histogramButton")->click();QCOMPARE(popup->mode(),PhotoPopup::Histogram);QVERIFY(popup->isVisible());window.findChild<QPushButton*>("histogramButton")->click();QVERIFY(!popup->isVisible());
        window.resize(640,480);capture(&window,"footer-toggles-640");window.activateWindow();viewer->setFocus();QTest::keyClick(viewer,Qt::Key_1);
        QTRY_VERIFY(!chip("a")->property("assigned").toBool());QVERIFY(chip("b")->property("assigned").toBool());QVERIFY(QFileInfo::exists(output+"/Best/photo.jpg"));QVERIFY(!QFileInfo::exists(output+"/Keep/photo.jpg"));QVERIFY(!window.findChild<CopyFlight*>("copyFlight"));
        chip("b")->click();QTRY_VERIFY(!chip("b")->property("assigned").toBool());QVERIFY(!QFileInfo::exists(output+"/Best/photo.jpg"));QCOMPARE(readFile(input+"/photo.jpg"),photoBytes());
        window.findChild<QPushButton*>("undoAction")->click();QTRY_VERIFY(chip("b")->property("assigned").toBool());QVERIFY(QFileInfo::exists(output+"/Best/photo.jpg"));
        chip("a")->click();QVERIFY(window.findChild<CopyFlight*>("copyFlight"));chip("a")->click();QVERIFY(!window.findChild<CopyFlight*>("copyFlight"));QTRY_VERIFY(!QFileInfo::exists(output+"/Keep/photo.jpg"));QVERIFY(chip("b")->property("assigned").toBool());window.shutdown();
    }
    void copyFlightUsesPreviousPhotoAndAllowsNavigation(){
        QTemporaryDir dir;const auto input=dir.filePath("in"),output=dir.filePath("out"),profile=dir.filePath("profile.sqlite");QVERIFY(QDir().mkpath(input));for(int i=0;i<3;++i)QVERIFY(writeFile(input+QString("/photo_%1.jpg").arg(i),photoBytes()));const auto photos=scanFolder(input).photos;
        {SessionStore store;QString error;QVERIFY(store.open(profile,error));QVERIFY(store.saveConfig(destinations(output),error));}
        MainWindow window(12*1024*1024,nullptr,profile);window.show();window.openFolder(input);auto* viewer=window.findChild<PhotoViewer*>();auto* strip=window.findChild<Filmstrip*>();QTRY_VERIFY(viewer->hasPhoto());window.activateWindow();viewer->setFocus();const auto sourceBounds=viewer->sortingBounds();QTest::keyClick(viewer,Qt::Key_1);
        QCOMPARE(strip->currentIndex(),1);auto* flight=window.findChild<CopyFlight*>("copyFlight");QVERIFY(flight);QCOMPARE(flight->property("photoId").toString(),photos[0].id);QCOMPARE(flight->property("destinationId").toString(),QString("a"));QVERIFY(!flight->snapshot().isNull());QVERIFY(flight->testAttribute(Qt::WA_TransparentForMouseEvents));QCOMPARE(flight->sourceRect().size().toSize(),sourceBounds.size());
        capture(&window,"copy-flight");QTest::keyClick(viewer,Qt::Key_Right);QCOMPARE(strip->currentIndex(),2);QTest::keyClick(viewer,Qt::Key_Left);QCOMPARE(strip->currentIndex(),1);QTRY_VERIFY(viewer->hasPhoto());QTest::keyClick(viewer,Qt::Key_2);QCOMPARE(strip->currentIndex(),2);QCOMPARE(window.findChildren<CopyFlight*>("copyFlight").size(),1);QCOMPARE(window.findChild<CopyFlight*>("copyFlight")->property("photoId").toString(),photos[1].id);
        QTRY_VERIFY(!window.findChild<CopyFlight*>("copyFlight"));QTRY_VERIFY(QFileInfo::exists(output+"/Keep/photo_0.jpg"));QTRY_VERIFY(QFileInfo::exists(output+"/Best/photo_1.jpg"));QCOMPARE(readFile(input+"/photo_0.jpg"),photoBytes());
        QTest::mouseClick(strip->viewport(),Qt::LeftButton,Qt::NoModifier,QPoint(40,40));QCOMPARE(strip->currentIndex(),0);QTRY_VERIFY(viewer->hasPhoto());viewer->setFocus();QTest::keyClick(viewer,Qt::Key_1);QCOMPARE(strip->currentIndex(),0);QTRY_VERIFY(!QFileInfo::exists(output+"/Keep/photo_0.jpg"));QVERIFY(!window.findChild<CopyFlight*>("copyFlight"));window.shutdown();
    }
    void toggleCancelsPendingCopyWithoutAdvancing(){
        QTemporaryDir dir;const auto source=dir.filePath("photo.jpg");QVERIFY(writeFile(source,photoBytes()));Photo photo{"photo",{source}};std::atomic_bool entered{false};
        SortingController c(nullptr,[&](ProcessingTask task,const CancelToken& cancel){entered=true;while(!cancel->load())QThread::msleep(2);return ProcessingPipeline::prepare(task,cancel);});QVERIFY(c.initialize(dir.filePath("profile.sqlite")));QVERIFY(c.configure(destinations(dir.filePath("out"))));QSignalSpy advance(&c,&SortingController::advanceRequested);
        QCOMPARE(c.toggleAssignment(photo,"a"),SortingController::DestinationToggle::CopyQueued);QTRY_VERIFY(entered.load());QVERIFY(!c.hasCompletedCopy(photo.id,"a"));QCOMPARE(c.toggleAssignment(photo,"a"),SortingController::DestinationToggle::RemovalRequested);QTRY_VERIFY(c.assignments(photo.id).isEmpty());QCOMPARE(advance.size(),1);QVERIFY(!QFileInfo::exists(dir.filePath("out/Keep/photo.jpg")));QCOMPARE(readFile(source),photoBytes());
    }
    void genieStartsAtFullPhotoAndBecomesTransparent(){
        QImage image(720,480,QImage::Format_RGB32);
        for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)image.setPixelColor(x,y,x%80<2||y%80<2?QColor(235,235,235):QColor(x*255/image.width(),y*255/image.height(),170));
        QWidget surface;surface.resize(900,650);surface.setStyleSheet("background:#111111;");
        CopyFlight flight(image,{90,30,720,480},{70,600,20,16},&surface);surface.show();flight.show();
        const auto at=[](const QImage& frame,int x,int y){return frame.pixelColor(qRound(x*frame.devicePixelRatio()),qRound(y*frame.devicePixelRatio()));};
        const auto initial=surface.grab().toImage();QVERIFY(at(initial,100,40).blue()>150);QCOMPARE(at(initial,880,10),QColor("#111111"));capture(&surface,"genie-start");
        flight.setProgress(.35);const auto bending=surface.grab().toImage();QVERIFY(bending!=initial);QCOMPARE(at(bending,95,35),QColor("#111111"));capture(&surface,"genie-bend");
        flight.setProgress(.65);capture(&surface,"genie-funnel");flight.setProgress(.9);capture(&surface,"genie-landing");
        flight.setProgress(1);const auto complete=surface.grab().toImage();QCOMPARE(at(complete,450,250),QColor("#111111"));QCOMPARE(at(complete,80,608),QColor("#111111"));
    }
    void savedCopyRemainsRemovableAfterMappingDeletion(){
        QTemporaryDir dir;const auto input=dir.filePath("in"),output=dir.filePath("out"),profile=dir.filePath("profile.sqlite");QVERIFY(QDir().mkpath(input));QVERIFY(writeFile(input+"/photo.jpg",photoBytes()));
        {SortingController c;QVERIFY(c.initialize(profile));QVERIFY(c.openFolderSession(input));auto config=destinations(output);QVERIFY(c.configure(config));c.assign(scanFolder(input).photos.first(),"a");QTRY_COMPARE(c.remaining(),0);config.folders.removeFirst();QVERIFY(c.configure(config));c.shutdown();}
        MainWindow window(12*1024*1024,nullptr,profile);window.show();window.openFolder(input);QTRY_VERIFY(window.findChild<PhotoViewer*>()->hasPhoto());QPushButton* saved=nullptr;for(auto* button:window.findChildren<QPushButton*>())if(button->property("destinationChip").toBool()&&button->property("destinationId")=="a")saved=button;
        QVERIFY(saved);QVERIFY(saved->property("assigned").toBool());QVERIFY(saved->property("savedDestination").toBool());saved->click();QTRY_VERIFY(!QFileInfo::exists(output+"/Keep/photo.jpg"));QCOMPARE(readFile(input+"/photo.jpg"),photoBytes());window.shutdown();
    }

};
QTEST_MAIN(FolderProgressTest)
#include "test_folder_progress.moc"
