#include "sorting_controller.h"
#include "shortcut_manager.h"
#include "main_window.h"
#include "raw_fixture.h"
#include <QApplication>
#include <QBuffer>
#include <QColorSpace>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QImageReader>
#include <QKeyEvent>
#include <QSemaphore>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <QSqlDatabase>
#include <QLabel>
#include "photo_viewer.h"
#include "destination_dialog.h"
#include <QTableWidget>
#include <QDialogButtonBox>
#include <QPushButton>
#include <atomic>

using namespace photocull;
namespace {
bool write(const QString& path,const QByteArray& data) { QFile f(path);return f.open(QIODevice::WriteOnly)&&f.write(data)==data.size(); }
QByteArray read(const QString& path) { QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray(); }
QByteArray jpeg(QSize size={80,40}) {
    QImage image(size,QImage::Format_RGB32);image.fill(Qt::red);
    QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);image.save(&buffer,"JPEG");return bytes;
}
DestinationConfig config(const QString& root) {return {root,{{"day","Day","1"},{"night","Night","2"}}};}
ProcessingTask task(const Photo& photo,const QString& directory) {
    ProcessingTask t;t.id=newId();t.photo=photo;t.categoryId="day";t.categoryName="Day";t.directory=directory;
    t.temporaryPath=QDir(directory).filePath(".photocull-"+t.id+".part");return t;
}
}
class SortingTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase(){qRegisterMetaType<Photo>();QVERIFY(QSqlDatabase::isDriverAvailable("QSQLITE"));}
    void configurationValidation() {
        QTemporaryDir dir;auto c=config(dir.path());QVERIFY(validateDestinationConfig(c).isEmpty());
        for(const QString name:{"..","../Day","CON","LPT1.txt","Day.","Day ","a/b","a\\b","a:b"})QVERIFY(!validateFolderName(name).isEmpty());
        c.folders[1].name="day";QVERIFY(!validateDestinationConfig(c).isEmpty());
        c=config(dir.path());c.folders[1].shortcut="1";QVERIFY(!validateDestinationConfig(c).isEmpty());
        c=config(dir.path());for(int i=0;i<100;++i)c.folders.append({newId(),"Category "+QString::number(i),{}});
        QVERIFY(validateDestinationConfig(c).isEmpty());
        QWidget window;ShortcutManager manager(&window);QString error;
        QVERIFY(!manager.setDestinations({{"day",QKeySequence(Qt::Key_Right)}},error));
        QVERIFY(manager.setDestinations({{"day",QKeySequence(Qt::CTRL|Qt::Key_Z)}},error));
        QVERIFY(!manager.setDestinations({{"day",QKeySequence(Qt::Key_1)},{"night",QKeySequence(Qt::Key_1)}},error));
        QVERIFY(manager.setDestinations({{"day",QKeySequence(Qt::Key_1)}},error));
        QSignalSpy spy(&manager,&ShortcutManager::triggered);window.show();
        QTest::keyClick(&window,Qt::Key_1);QCOMPARE(spy.size(),1);
        QKeyEvent repeat(QEvent::KeyPress,Qt::Key_1,Qt::NoModifier,"1",true,5);QApplication::sendEvent(&window,&repeat);
        QCOMPARE(spy.size(),1);
        QKeyEvent navigation(QEvent::KeyPress,Qt::Key_Right,Qt::NoModifier,QString(),true,3);QApplication::sendEvent(&window,&navigation);
        QCOMPARE(spy.size(),2);QCOMPARE(spy.last()[0].toString(),"next");
        QVERIFY(manager.setDestinations({{"day",QKeySequence(Qt::Key_3)}},error));
        QTest::keyClick(&window,Qt::Key_1);QCOMPARE(spy.size(),2);
        QTest::keyClick(&window,Qt::Key_3);QCOMPARE(spy.size(),3);
    }
    void copyCollisionAndIdempotence() {
        QTemporaryDir dir;const auto source=dir.filePath("frame.JPG");const auto original=jpeg();QVERIFY(write(source,original));
        const auto output=dir.filePath("out");QVERIFY(QDir().mkpath(output+"/Day"));QVERIFY(write(output+"/Day/frame.JPG","existing photograph"));
        SortingController controller;QSignalSpy errors(&controller,&SortingController::problem);
        QVERIFY(controller.initialize(dir.filePath("session.sqlite")));QVERIFY(controller.configure(config(output)));
        QSignalSpy advance(&controller,&SortingController::advanceRequested);Photo photo{"photo",{source}};
        const auto id=controller.assign(photo,"day");QVERIFY2(!id.isEmpty(),errors.isEmpty()?"":qPrintable(errors.last()[0].toString()));
        QCOMPARE(advance.size(),1);QTRY_COMPARE_WITH_TIMEOUT(controller.assignment(photo.id).status,QString("completed"),5000);
        const auto path=controller.assignment(photo.id).outputPath;QVERIFY(path.endsWith("frame_2.JPG"));QCOMPARE(read(path),original);
        QCOMPARE(read(output+"/Day/frame.JPG"),QByteArray("existing photograph"));QCOMPARE(read(source),original);
        QCOMPARE(controller.assign(photo,"day"),id);QCOMPARE(advance.size(),2);
        QCOMPARE(QDir(output+"/Day").entryList({"*.JPG"},QDir::Files).size(),2);
    }
    void pngCopiedExactly() {
        QTemporaryDir dir;const auto path=dir.filePath("image.png");QImage image(35,20,QImage::Format_ARGB32);image.fill(Qt::transparent);QVERIFY(image.save(path));
        auto t=task({"png",{path}},dir.filePath("out"));auto prepared=ProcessingPipeline::prepare(t,{});QCOMPARE(prepared.status,"prepared");
        auto completed=ProcessingPipeline::publish(prepared,{});QCOMPARE(completed.status,"completed");QCOMPARE(read(completed.outputPath),read(path));
    }
    void rawSensorDevelopmentAndPair() {
        QTemporaryDir dir;const auto rawPath=dir.filePath(QString::fromUtf8("sensor_परीक्षण.dng"));const auto jpgPath=dir.filePath("sensor.jpg");
        const auto rawBytes=fixture::rawDng({jpeg({80,40})});QVERIFY(write(rawPath,rawBytes));QVERIFY(write(jpgPath,jpeg({80,40})));
        auto t=task({"pair",{jpgPath,rawPath}},dir.filePath("out"));auto prepared=ProcessingPipeline::prepare(t,{});
        QVERIFY2(prepared.status=="prepared",qPrintable(prepared.error));auto completed=ProcessingPipeline::publish(prepared,{});
        QCOMPARE(completed.status,"completed");QImageReader reader(completed.outputPath);auto image=reader.read();
        QVERIFY2(!image.isNull(),qPrintable(reader.errorString()));QCOMPARE(image.size(),QSize(384,512)); // SENSOR dimensions/orientation, not 80x40 preview.
        QCOMPARE(image.colorSpace(),QColorSpace(QColorSpace::SRgb));
        QCOMPARE(QDir(t.directory).entryList({"*.jpg"},QDir::Files).size(),1);QCOMPARE(read(rawPath),rawBytes);QCOMPARE(read(jpgPath),jpeg({80,40}));
    }
    void instantAdvanceHundredsAndCancellation() {
        QTemporaryDir dir;const auto source=dir.filePath("frame.jpg");QVERIFY(write(source,jpeg()));
        std::atomic_int entered{0},active{0},maximum{0};
        SortingController controller(nullptr,[&](ProcessingTask t,const CancelToken& cancel){
            ++entered;const int count=++active;maximum.store(qMax(maximum.load(),count));
            while(!cancel->load())QThread::msleep(2);
            --active;t.status="cancelled";return t;
        });
        QVERIFY(controller.initialize(dir.filePath("session.sqlite")));QVERIFY(controller.configure(config(dir.filePath("out"))));
        QSignalSpy advance(&controller,&SortingController::advanceRequested);
        for(int i=0;i<200;++i)QVERIFY(!controller.assign({QString::number(i),{source}},"day").isEmpty());
        QCOMPARE(advance.size(),200);QCOMPARE(controller.remaining(),200);QTRY_COMPARE(entered.load(),1);QCOMPARE(maximum.load(),1);
        controller.cancelPending();QCOMPARE(controller.remaining(),0);QTRY_COMPARE(active.load(),0);
        for(int i=0;i<200;++i)QCOMPARE(controller.assignment(QString::number(i)).status,QString("cancelled"));
    }
    void replacedIdenticalOutputPreserved() {
        QTemporaryDir dir;const auto source=dir.filePath("frame.jpg");QVERIFY(write(source,jpeg()));
        auto prepared=ProcessingPipeline::prepare(task({"one",{source}},dir.filePath("out")),{});auto output=ProcessingPipeline::publish(prepared,{});
        QCOMPARE(output.status,"completed");const auto contents=read(output.outputPath);
        QVERIFY(QFile::rename(output.outputPath,output.outputPath+".old"));QVERIFY(write(output.outputPath,contents));
        QString error;QVERIFY(!ProcessingPipeline::removeOwned(output.outputPath,output.digest,output.identity,error));
        QCOMPARE(read(output.outputPath),contents);
    }
    void failureRetryAndPublishCollision() {
        QTemporaryDir dir;const auto missing=dir.filePath("missing.jpg");Photo photo{"one",{missing}};
        SortingController controller;QVERIFY(controller.initialize(dir.filePath("session.sqlite")));QVERIFY(controller.configure(config(dir.filePath("out"))));
        controller.assign(photo,"day");QTRY_COMPARE(controller.assignment("one").status,QString("failed"));QVERIFY(!controller.assignment("one").error.isEmpty());
        QVERIFY(write(missing,jpeg()));controller.retryFailed();QTRY_COMPARE(controller.assignment("one").status,QString("completed"));
        auto prepared=ProcessingPipeline::prepare(task(photo,dir.filePath("collision")),{});QCOMPARE(prepared.status,"prepared");
        QVERIFY(write(prepared.outputPath,"external file created after prepare"));auto failed=ProcessingPipeline::publish(prepared,{});
        QCOMPARE(failed.status,"failed");QCOMPARE(read(prepared.outputPath),QByteArray("external file created after prepare"));
        auto cleaned=ProcessingPipeline::cleanup(failed);QCOMPARE(cleaned.status,"preserved");QCOMPARE(read(prepared.outputPath),QByteArray("external file created after prepare"));
    }
    void recovery_data(){QTest::addColumn<bool>("alreadyPublished");QTest::newRow("prepared")<<false;QTest::newRow("published-before-db-commit")<<true;}
    void recovery() {
        QFETCH(bool,alreadyPublished);QTemporaryDir dir;const auto source=dir.filePath("frame.jpg");QVERIFY(write(source,jpeg()));
        const auto db=dir.filePath("session.sqlite");Photo photo{"one",{source}};QString output;
        {
            SessionStore store;QString error;QVERIFY2(store.open(db,error),qPrintable(error));auto t=task(photo,dir.filePath("out/Day"));
            QVERIFY(store.saveConfig(config(dir.filePath("out")),error));QVERIFY(store.changeAssignment(photo,t.id,&t,{},error));
            t=ProcessingPipeline::prepare(t,{});QCOMPARE(t.status,"prepared");QVERIFY2(store.saveTask(t,error),qPrintable(error));output=t.outputPath;
            if(alreadyPublished)QCOMPARE(ProcessingPipeline::publish(t,{}).status,QString("completed"));
        }
        SortingController controller;QVERIFY(controller.initialize(db));QTRY_COMPARE(controller.assignment("one").status,QString("completed"));
        QCOMPARE(controller.assignment("one").outputPath,output);QCOMPARE(QDir(dir.filePath("out/Day")).entryList({"*.jpg"},QDir::Files).size(),1);
    }
    void interruptedPreparationAndSessionLock() {
        QTemporaryDir dir;const auto source=dir.filePath("frame.jpg");QVERIFY(write(source,jpeg()));const auto db=dir.filePath("session.sqlite");QString partial;
        {
            SessionStore store,second;QString error;QVERIFY(store.open(db,error));QVERIFY(!second.open(db,error));
            auto t=task({"one",{source}},dir.filePath("out/Day"));t.status="processing";QVERIFY(QDir().mkpath(t.directory));partial=t.temporaryPath;QVERIFY(write(partial,"unverified interrupted bytes"));
            QVERIFY(store.changeAssignment(t.photo,t.id,&t,{},error));
        }
        SortingController controller;QVERIFY(controller.initialize(db));QTRY_COMPARE(controller.assignment("one").status,QString("completed"));
        QCOMPARE(read(partial),QByteArray("unverified interrupted bytes"));QCOMPARE(read(controller.assignment("one").outputPath),jpeg());
    }
    void shutdownDuringWorkRecovers() {
        QTemporaryDir dir;const auto source=dir.filePath("frame.jpg");QVERIFY(write(source,jpeg()));const auto db=dir.filePath("session.sqlite");
        std::atomic_bool entered{false};
        {
            SortingController controller(nullptr,[&](ProcessingTask t,const CancelToken& token){entered=true;while(!token->load())QThread::msleep(2);t.status="cancelled";return t;});
            QVERIFY(controller.initialize(db));QVERIFY(controller.configure(config(dir.filePath("out"))));controller.assign({"one",{source}},"day");QTRY_VERIFY(entered.load());controller.shutdown();
        }
        SortingController recovered;QVERIFY(recovered.initialize(db));QTRY_COMPARE(recovered.assignment("one").status,QString("completed"));
    }
    void blockedDestinationAndCorruptRawPair() {
        QTemporaryDir dir;const auto source=dir.filePath("frame.jpg");QVERIFY(write(source,jpeg()));
        const auto blocked=dir.filePath("blocked");QVERIFY(write(blocked,"a file, not a directory"));
        auto result=ProcessingPipeline::prepare(task({"one",{source}},blocked),{});QCOMPARE(result.status,"failed");
        const auto raw=dir.filePath("frame.dng");QVERIFY(write(raw,"corrupted sensor file"));
        result=ProcessingPipeline::prepare(task({"pair",{source,raw}},dir.filePath("out")),{});
        QCOMPARE(result.status,"failed");QVERIFY(result.error.contains("RAW"));
        QVERIFY(QDir(dir.filePath("out")).entryList({"*.jpg"},QDir::Files).isEmpty());QCOMPARE(read(source),jpeg());
    }
    void metadataConfigRoundTrip() {
        QTemporaryDir dir;const auto db=dir.filePath("session.sqlite");
        {
            SessionStore store;QString error;QVERIFY(store.open(db,error));auto c=config(dir.filePath("out"));
            c.folders[0].name="Personal";c.folders[0].shortcut="Ctrl+1";c.folders.swapItemsAt(0,1);
            QVERIFY(store.saveConfig(c,error));SessionSnapshot snapshot;QVERIFY(store.load(snapshot,error));
            QCOMPARE(snapshot.config.folders[0].name,QString("Night"));QCOMPARE(snapshot.config.folders[1].name,QString("Personal"));
            QCOMPARE(snapshot.config.folders[1].shortcut,QString("Ctrl+1"));
            c.folders.removeAt(0);QVERIFY(store.saveConfig(c,error));
        }
        SortingController controller;QVERIFY(controller.initialize(db));QCOMPARE(controller.config().folders.size(),1);
        QCOMPARE(controller.config().folders[0].name,QString("Personal"));
    }
    void windowKeyboardWorkflow() {
        QTemporaryDir dir;const auto input=dir.filePath("input"),output=dir.filePath("output"),db=dir.filePath("session.sqlite");
        QVERIFY(QDir().mkpath(input));for(int i=1;i<=3;++i)QVERIFY(write(QDir(input).filePath(QString("IMG_%1.jpg").arg(i)),jpeg({900,600})));
        {SessionStore store;QString error;QVERIFY(store.open(db,error));QVERIFY(store.saveConfig(config(output),error));}
        auto hasLabel=[](MainWindow& window,const QString& text){for(auto* label:window.findChildren<QLabel*>())if(label->text()==text)return true;return false;};
        {
            MainWindow window(16*1024*1024,nullptr,db);window.show();window.openFolder(input);
            auto* viewer=window.findChild<PhotoViewer*>();QVERIFY(viewer);
            QTRY_VERIFY(hasLabel(window,"IMG_1.jpg"));QTest::keyClick(viewer,Qt::Key_1);
            QVERIFY(hasLabel(window,"IMG_2.jpg")); // Synchronous auto-advance, not waiting for worker completion.
            QKeyEvent repeat(QEvent::KeyPress,Qt::Key_1,Qt::NoModifier,"1",true,5);QApplication::sendEvent(viewer,&repeat);
            QVERIFY(hasLabel(window,"IMG_2.jpg"));QTest::keyClick(viewer,Qt::Key_Space);QVERIFY(hasLabel(window,"IMG_3.jpg"));
            QTRY_VERIFY(QFileInfo::exists(output+"/Day/IMG_1.jpg"));QVERIFY(!QFileInfo::exists(output+"/Day/IMG_2.jpg"));
            QTest::keyClick(viewer,Qt::Key_Z,Qt::ControlModifier);QVERIFY(hasLabel(window,"IMG_3.jpg"));
            QTest::keyClick(viewer,Qt::Key_Home);QVERIFY(hasLabel(window,"IMG_1.jpg"));QTest::keyClick(viewer,Qt::Key_1);
#ifdef Q_OS_WIN
            QTRY_VERIFY(!QFileInfo::exists(output+"/Day/IMG_1.jpg"));
#endif
            QTest::keyClick(viewer,Qt::Key_2);QVERIFY(hasLabel(window,"IMG_2.jpg"));
            QTRY_VERIFY(QFileInfo::exists(output+"/Night/IMG_1.jpg"));QTest::keyClick(viewer,Qt::Key_Left);
            QVERIFY(hasLabel(window,"IMG_1.jpg"));QTest::keyClick(viewer,Qt::Key_Backspace);
#ifdef Q_OS_WIN
            QTRY_VERIFY(!QFileInfo::exists(output+"/Night/IMG_1.jpg"));
#endif
            for(auto* chip:window.findChildren<QPushButton*>())if(chip->property("destinationChip").toBool())QTRY_VERIFY(!chip->property("assigned").toBool());
            if(qEnvironmentVariableIsSet("PHOTOCULL_PHASE2_SCREENSHOT"))QVERIFY(window.grab().save(qEnvironmentVariable("PHOTOCULL_PHASE2_SCREENSHOT")));
            window.shutdown();
        }
        MainWindow restored(16*1024*1024,nullptr,db);restored.show();QTRY_VERIFY(hasLabel(restored,"IMG_1.jpg"));restored.shutdown();
    }
    void destinationDialogEditing() {
        QTemporaryDir dir;QWidget owner;ShortcutManager manager(&owner);
        DestinationDialog dialog(config(dir.filePath("out")),&manager,&owner);dialog.show();
        auto* table=dialog.findChild<QTableWidget*>();QVERIFY(table);QCOMPARE(table->rowCount(),2);
        table->item(0,0)->setText("Morning");table->setCurrentCell(1,0);
        auto* remove=qobject_cast<QPushButton*>(table->cellWidget(1,2));QVERIFY(remove);remove->click();QCOMPARE(table->rowCount(),1);
        auto* add=dialog.findChild<QPushButton*>("addDestination");QVERIFY(add);add->click();QCOMPARE(table->rowCount(),2);
        // Finish the inline name edit before saving; default shortcut 2 remains conflict-free.
        table->setCurrentCell(0,0);QSignalSpy accepted(&dialog,&QDialog::accepted);
        auto* actions=dialog.findChild<QDialogButtonBox*>();QVERIFY(actions);actions->button(QDialogButtonBox::Save)->click();
        QCOMPARE(accepted.size(),1);QCOMPARE(dialog.configuration().folders[0].name,QString("Morning"));
    }
};
QTEST_MAIN(SortingTest)
#include "test_sorting.moc"
