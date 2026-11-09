#include "watermark.h"
#include "watermark_dialog.h"
#include "photo_viewer.h"
#include "sorting_controller.h"
#include "main_window.h"
#include "raw_fixture.h"
#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QColorSpace>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QImageReader>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QAction>
#include <QSignalSpy>
#include <QSlider>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <atomic>

using namespace photocull;
namespace {
QByteArray encoded(const QImage& image,const char* format="PNG") {
    QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);image.save(&buffer,format,100);return bytes;
}
QImage solid(QSize size,QColor color=QColor(100,100,100)) {
    QImage image(size,QImage::Format_ARGB32);image.fill(color);image.setColorSpace(QColorSpace::SRgb);return image;
}
bool write(const QString& path,const QByteArray& bytes){QFile f(path);return f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size();}
QByteArray read(const QString& path){QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();}
WatermarkSnapshot logo(QColor color=Qt::red,QSize size={40,20}) {
    QString error;auto asset=decodeWatermarkPng(encoded(solid(size,color)),"club.png",error);
    WatermarkSnapshot snapshot;snapshot.settings.enabled=true;snapshot.settings.sizePercent=40;snapshot.settings.paddingPercent=10;
    snapshot.asset=asset;snapshot.assetId=asset?asset->id:QString();return snapshot;
}
DestinationConfig config(const QString& root){return {root,{{"club","Club","1","copy"}}};}
ProcessingTask task(const QString& path,const QString& directory,const WatermarkSnapshot& watermark) {
    ProcessingTask t;t.id=newId();t.photo={newId(),{path}};t.categoryId="club";t.categoryName="Club";t.directory=directory;
    t.temporaryPath=QDir(directory).filePath(".photocull-"+t.id+".part");t.watermark=watermark;return t;
}
bool near(QColor a,QColor b,int tolerance=5){return qAbs(a.red()-b.red())<=tolerance&&qAbs(a.green()-b.green())<=tolerance&&qAbs(a.blue()-b.blue())<=tolerance;}
bool samePixels(const QImage& a,const QImage& b) {
    if(a.size()!=b.size())return false;
    for(int y=0;y<a.height();++y)for(int x=0;x<a.width();++x)if(!near(a.pixelColor(x,y),b.pixelColor(x,y),2))return false;
    return true;
}
QImage frame(PhotoViewer& viewer) {return viewer.grab().toImage().scaled(viewer.size(),Qt::IgnoreAspectRatio,Qt::SmoothTransformation);}
QRect redBounds(const QImage& image) {
    QRect bounds;
    for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x){const auto c=image.pixelColor(x,y);if(c.red()>180&&c.green()<50&&c.blue()<50)bounds=bounds.united(QRect(x,y,1,1));}
    return bounds;
}
bool execute(const QString& path,const std::function<bool(QSqlDatabase&)>& fn) {
    const auto name=newId();bool ok;
    {auto db=QSqlDatabase::addDatabase("QSQLITE",name);db.setDatabaseName(path);ok=db.open()&&fn(db);db.close();}
    QSqlDatabase::removeDatabase(name);return ok;
}
}
class WatermarkTest:public QObject {
    Q_OBJECT
private slots:
    void initTestCase(){qRegisterMetaType<WatermarkSnapshot>();qRegisterMetaType<Photo>();}
    void geometry_data(){
        QTest::addColumn<int>("corner");QTest::addColumn<QSize>("photo");
        for(const QSize size:{QSize(600,400),QSize(400,600),QSize(400,400)})for(int corner=0;corner<4;++corner)
            QTest::newRow(qPrintable(QString("%1x%2-corner%3").arg(size.width()).arg(size.height()).arg(corner)))<<corner<<size;
    }
    void geometry(){
        QFETCH(int,corner);QFETCH(QSize,photo);auto wm=logo();wm.settings.position=WatermarkPosition(corner);
        const auto rect=watermarkRect(photo,{40,20},wm.settings);
        QCOMPARE(rect.size(),QSizeF(160,80));
        QCOMPARE(rect.left(),corner%2==1?photo.width()-200.0:40.0);
        QCOMPARE(rect.top(),corner>=2?photo.height()-120.0:40.0);
        auto image=solid(photo);QString error;QVERIFY(compositeWatermark(image,wm,error));
        QCOMPARE(image.pixelColor(rect.center().toPoint()),QColor(Qt::red));
        QCOMPARE(image.pixelColor(0,0),QColor(100,100,100));
        const auto doubled=watermarkRect(photo*2,{40,20},wm.settings);
        QCOMPARE(doubled.topLeft(),rect.topLeft()*2);QCOMPARE(doubled.size(),rect.size()*2);
    }
    void constrainedAspectAndValidation(){
        auto wm=logo();wm.settings.sizePercent=100;wm.settings.paddingPercent=25;
        for(const QSize logoSize:{QSize(1,1000),QSize(1000,1),QSize(100,100)})for(const QSize photo:{QSize(400,600),QSize(600,400)}) {
            const auto rect=watermarkRect(photo,logoSize,wm.settings);
            QVERIFY(QRectF(100,100,photo.width()-200,photo.height()-200).contains(rect));
            QVERIFY(qAbs(rect.width()/rect.height()-double(logoSize.width())/logoSize.height())<0.001);
        }
        QString error;QVERIFY(!decodeWatermarkPng("broken","bad.png",error));QVERIFY(!error.isEmpty());
        QVERIFY(!decodeWatermarkPng(encoded(solid({20,20}),"JPEG"),"fake.png",error));
        WatermarkSettings restored;QVERIFY(readWatermarkSettings(watermarkSettingsJson(wm.settings),restored));QVERIFY(restored==wm.settings);
        QVERIFY(readWatermarkSettings({},restored));QVERIFY(!restored.enabled);
        QVERIFY(!readWatermarkSettings({{"enabled","true"}},restored));
        auto json=watermarkSettingsJson(wm.settings);json["size"]=0;QVERIFY(!readWatermarkSettings(json,restored));
        json["size"]=20.5;QVERIFY(!readWatermarkSettings(json,restored));
        wm.asset.reset();QVERIFY(!validateWatermark(wm).isEmpty());
    }
    void alphaOpacityColorAndImmutability(){
        auto wm=logo(QColor(255,0,0,128));wm.settings.opacityPercent=50;
        const auto png=wm.asset->png;const auto logoBefore=wm.asset->image.copy();auto input=solid({400,300},Qt::white);const auto original=input.copy();
        QString error;auto output=input;QVERIFY(compositeWatermark(output,wm,error));
        const auto point=watermarkRect(output.size(),wm.asset->image.size(),wm.settings).center().toPoint();
        QVERIFY(near(output.pixelColor(point),QColor(255,191,191),2));QCOMPARE(input,original);QCOMPARE(wm.asset->png,png);QCOMPARE(wm.asset->image,logoBefore);
        wm.settings.opacityPercent=0;output=input;QVERIFY(compositeWatermark(output,wm,error));QVERIFY(samePixels(output,input));
        wm.settings.enabled=false;output=input;QVERIFY(compositeWatermark(output,wm,error));QCOMPARE(output,input);
        auto p3=solid({400,300},QColor(80,120,160));p3.setColorSpace(QColorSpace::DisplayP3);wm.settings.enabled=true;wm.settings.opacityPercent=100;
        QVERIFY(compositeWatermark(p3,wm,error));QCOMPARE(p3.colorSpace(),QColorSpace(QColorSpace::SRgb));
        auto gray=QImage(400,300,QImage::Format_Grayscale8);gray.fill(100);QVERIFY(compositeWatermark(gray,wm,error));QVERIFY(gray.pixelColor(point).red()>gray.pixelColor(point).green());
    }
    void viewerMatchesOutputAndTransforms(){
        auto wm=logo();PhotoViewer viewer;viewer.resize(600,400);viewer.show();auto image=solid({600,400});const auto original=image.copy();
        viewer.setPreview({image,image.size(),{},{}});viewer.setWatermark(wm);QCoreApplication::processEvents();
        auto output=image;QString error;QVERIFY(compositeWatermark(output,wm,error));QVERIFY(samePixels(frame(viewer),output));
        QCOMPARE(redBounds(frame(viewer)),QRect(400,280,160,80));
        viewer.resize(300,300);QCoreApplication::processEvents();QCOMPARE(redBounds(frame(viewer)),QRect(200,190,80,40));
        viewer.zoomBy(2);QCOMPARE(redBounds(frame(viewer)),QRect(250,230,50,70)); // Right edge is outside viewport.
        // Pan the enlarged photo left/up; logo follows the photo, not the window.
        QMouseEvent press(QEvent::MouseButtonPress,{150,150},{150,150},Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(&viewer,&press);
        QMouseEvent move(QEvent::MouseMove,{0,100},{0,100},Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(&viewer,&move);
        QMouseEvent release(QEvent::MouseButtonRelease,{0,100},{0,100},Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(&viewer,&release);
        QCOMPARE(redBounds(frame(viewer)),QRect(100,180,160,80));
        viewer.fit();wm.settings.enabled=false;viewer.setWatermark(wm);QVERIFY(redBounds(frame(viewer)).isEmpty());
        wm.settings.enabled=true;viewer.setWatermark(wm);viewer.resetPhoto();viewer.setPreview({solid({400,600}),{400,600},{},{}});
        QCOMPARE(redBounds(frame(viewer)),QRect(150,240,80,40));QCOMPARE(image,original);
    }
    void dialogLiveControlsAndImport(){
        QTemporaryDir dir;QVERIFY(dir.isValid());auto wm=logo();const auto path=dir.filePath("club.png");QVERIFY(write(path,wm.asset->png));
        WatermarkDialog dialog({});PhotoViewer viewer;viewer.resize(600,400);viewer.show();viewer.setPreview({solid({600,400}),{600,400},{},{}});
        connect(&dialog,&WatermarkDialog::changed,&viewer,&PhotoViewer::setWatermark);QSignalSpy changed(&dialog,&WatermarkDialog::changed);dialog.show();QVERIFY(!dialog.isModal());
        auto* options=dialog.findChild<QWidget*>("watermarkOptions");QVERIFY(options&&!options->isVisible());
        auto* enabled=dialog.findChild<QCheckBox*>("watermarkEnabled");enabled->setChecked(true);
        QVERIFY(options->isVisible());QVERIFY(dialog.findChild<QPushButton*>("selectWatermark")->isVisible());QVERIFY(!dialog.snapshot().settings.enabled);
        dialog.selectPng(path);QTRY_COMPARE(changed.size(),1);QVERIFY(dialog.snapshot().settings.enabled);QVERIFY(!redBounds(frame(viewer)).isEmpty());
        const auto first=redBounds(frame(viewer));auto* size=dialog.findChild<QSlider*>("watermarkSize");QVERIFY(size);size->setValue(40);QVERIFY(redBounds(frame(viewer)).width()>first.width());
        auto* pos=dialog.findChild<QComboBox*>("watermarkPosition");QCOMPARE(pos->count(),4);pos->setCurrentIndex(0);QVERIFY(redBounds(frame(viewer)).left()<100);
        auto* padding=dialog.findChild<QSlider*>("watermarkPadding");padding->setValue(10);QCOMPARE(redBounds(frame(viewer)).left(),40);
        auto* opacity=dialog.findChild<QSlider*>("watermarkOpacity");opacity->setValue(0);QVERIFY(redBounds(frame(viewer)).isEmpty());opacity->setValue(100);
        enabled->setChecked(false);QVERIFY(!options->isVisible());QVERIFY(!dialog.findChild<QPushButton*>("selectWatermark")->isVisible());QVERIFY(redBounds(frame(viewer)).isEmpty());enabled->setChecked(true);QVERIFY(options->isVisible());QVERIFY(!redBounds(frame(viewer)).isEmpty());
        const auto id=dialog.snapshot().assetId;dialog.selectPng(dir.filePath("missing.png"));QTRY_VERIFY(dialog.findChild<QPushButton*>("selectWatermark")->isEnabled());QCOMPARE(dialog.snapshot().assetId,id);
        for(const auto* button:dialog.findChildren<QPushButton*>())QVERIFY(!button->text().contains("Apply")&&!button->text().contains("Export"));
    }
    void jpegPngAndRawAtOriginalResolution(){
        QTemporaryDir dir;auto wm=logo();
        for(const QString ext:{"jpg","png","dng"}) {
            const auto source=dir.filePath("photo."+ext);const auto bytes=ext=="dng"?fixture::rawDng({encoded(solid({80,40}),"JPEG")}):encoded(solid({1200,800}),ext=="jpg"?"JPEG":"PNG");QVERIFY(write(source,bytes));
            auto prepared=ProcessingPipeline::prepare(task(source,dir.filePath("out-"+ext),wm),{});QVERIFY2(prepared.status=="prepared",qPrintable(prepared.error));
            const auto completed=ProcessingPipeline::publish(prepared,{});QCOMPARE(completed.status,"completed");QImageReader reader(completed.outputPath);const auto output=reader.read();
            QCOMPARE(output.size(),ext=="dng"?QSize(384,512):QSize(1200,800));QCOMPARE(output.colorSpace(),QColorSpace(QColorSpace::SRgb));
            const auto point=watermarkRect(output.size(),wm.asset->image.size(),wm.settings).center().toPoint();QVERIFY(near(output.pixelColor(point),Qt::red,8));
            QCOMPARE(read(source),bytes);if(ext=="png")QCOMPARE(QImageReader::imageFormat(completed.outputPath),QByteArray("png"));else QCOMPARE(QImageReader::imageFormat(completed.outputPath),QByteArray("jpeg"));
            if(ext!="dng") {auto expected=solid({1200,800});QString error;QVERIFY(compositeWatermark(expected,wm,error));QVERIFY(near(output.pixelColor(point),expected.pixelColor(point),8));}
        }
        const auto source=dir.filePath("photo.jpg");wm.settings.enabled=false;const auto before=read(source);
        auto prepared=ProcessingPipeline::prepare(task(source,dir.filePath("copy"),wm),{});QCOMPARE(prepared.status,"prepared");const auto completed=ProcessingPipeline::publish(prepared,{});QCOMPARE(read(completed.outputPath),before);
    }
    void orientationBakedBeforeWatermark(){
        QTemporaryDir dir;auto bytes=encoded(solid({600,400}),"JPEG");QByteArray exif=QByteArray::fromHex("45786966000049492a0008000000010012010300010000000600000000000000");
        QByteArray segment=QByteArray::fromHex("ffe1");segment.append(char((exif.size()+2)>>8));segment.append(char((exif.size()+2)&255));bytes.insert(2,segment+exif);
        const auto path=dir.filePath("rotated.jpg");QVERIFY(write(path,bytes));auto wm=logo();wm.settings.position=WatermarkPosition::TopRight;
        const auto prepared=ProcessingPipeline::prepare(task(path,dir.filePath("out"),wm),{});QVERIFY2(prepared.status=="prepared",qPrintable(prepared.error));const auto completed=ProcessingPipeline::publish(prepared,{});
        QImageReader reader(completed.outputPath);reader.setAutoTransform(true);const auto image=reader.read();QCOMPARE(image.size(),QSize(400,600));
        QVERIFY(near(image.pixelColor(280,80),Qt::red,8));QVERIFY(near(image.pixelColor(80,80),QColor(100,100,100),8));QCOMPARE(read(path),bytes);
    }
    void transparentPngOutput(){
        QTemporaryDir dir;auto wm=logo(QColor(255,0,0,128));wm.settings.opacityPercent=50;
        const auto source=dir.filePath("transparent.png");QVERIFY(write(source,encoded(solid({600,400},Qt::transparent))));
        const auto prepared=ProcessingPipeline::prepare(task(source,dir.filePath("out"),wm),{});QVERIFY2(prepared.status=="prepared",qPrintable(prepared.error));
        const auto completed=ProcessingPipeline::publish(prepared,{});const QImage output(completed.outputPath);
        QCOMPARE(output.pixelColor(0,0).alpha(),0);QVERIFY(qAbs(output.pixelColor(480,320).alpha()-64)<=1);QCOMPARE(output.pixelColor(480,320).red(),255);
    }
    void retryUsesCapturedLogo(){
        QTemporaryDir dir;const auto source=dir.filePath("missing.png");SortingController controller;
        QVERIFY(controller.initialize(dir.filePath("session.sqlite")));QVERIFY(controller.configure(config(dir.filePath("out"))));
        const auto red=logo();QVERIFY(controller.setWatermark(red));QVERIFY(!controller.assign({"photo",{source}},"club").isEmpty());
        QTRY_COMPARE(controller.assignment("photo").status,QString("failed"));QVERIFY(controller.setWatermark(logo(Qt::blue)));
        QVERIFY(write(source,encoded(solid({600,400}))));controller.retryFailed();QTRY_COMPARE(controller.assignment("photo").status,QString("completed"));
        QCOMPARE(controller.assignment("photo").watermark.assetId,red.assetId);QCOMPARE(QImage(controller.assignment("photo").outputPath).pixelColor(480,320),QColor(Qt::red));
    }
    void processingBatchKeepsEventLoopRunning(){
        QTemporaryDir dir;const auto source=dir.filePath("photo.jpg");QVERIFY(write(source,encoded(solid({1600,1200}),"JPEG")));
        SortingController controller;QVERIFY(controller.initialize(dir.filePath("session.sqlite")));QVERIFY(controller.configure(config(dir.filePath("out"))));QVERIFY(controller.setWatermark(logo()));
        int heartbeats=0;QTimer heartbeat;heartbeat.setInterval(5);connect(&heartbeat,&QTimer::timeout,this,[&]{++heartbeats;});heartbeat.start();
        for(int i=0;i<40;++i)QVERIFY(!controller.assign({QString::number(i),{source}},"club").isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(controller.remaining(),0,15000);QVERIFY(heartbeats>10);QCOMPARE(controller.failed(),0);
        QCOMPARE(QDir(dir.filePath("out/Club")).entryList({"*.jpg"},QDir::Files).size(),40);
    }
    void capturedVersionsAndImmediateAdvance(){
        QTemporaryDir dir;const auto source=dir.filePath("photo.png");QVERIFY(write(source,encoded(solid({600,400}))));
        std::atomic_bool release{false};std::atomic_int entered{0};
        SortingController controller(nullptr,[&](ProcessingTask t,const CancelToken& cancel){++entered;while(!release.load()&&!cancel->load())QThread::msleep(2);return ProcessingPipeline::prepare(t,cancel);});
        QVERIFY(controller.initialize(dir.filePath("session.sqlite")));QVERIFY(controller.configure(config(dir.filePath("out"))));
        auto red=logo();QVERIFY(controller.setWatermark(red));QSignalSpy advance(&controller,&SortingController::advanceRequested);
        QVERIFY(!controller.assign({"red",{source}},"club").isEmpty());QCOMPARE(advance.size(),1);
        auto blue=logo(Qt::blue);blue.settings.position=WatermarkPosition::TopLeft;QVERIFY(controller.setWatermark(blue));QVERIFY(!controller.assign({"blue",{source}},"club").isEmpty());
        blue.settings.enabled=false;QVERIFY(controller.setWatermark(blue));QVERIFY(!controller.assign({"copy",{source}},"club").isEmpty());QCOMPARE(advance.size(),3);
        QTRY_COMPARE(entered.load(),1);QCOMPARE(controller.assignment("red").watermark.assetId,red.assetId);QCOMPARE(controller.assignment("blue").watermark.assetId,blue.assetId);
        release=true;QTRY_COMPARE_WITH_TIMEOUT(controller.remaining(),0,8000);
        QCOMPARE(controller.assignment("red").status,"completed");QCOMPARE(controller.assignment("blue").status,"completed");
        QImage redImage(controller.assignment("red").outputPath),blueImage(controller.assignment("blue").outputPath);
        QCOMPARE(redImage.pixelColor(480,320),QColor(Qt::red));QCOMPARE(blueImage.pixelColor(80,80),QColor(Qt::blue));QCOMPARE(read(controller.assignment("copy").outputPath),read(source));
        // Same destination but new settings creates a new output job; undo restores the previous snapshot.
        const auto old=controller.assignment("red").id;QVERIFY(controller.assign({"red",{source}},"club")!=old);QTRY_COMPARE(controller.assignment("red").status,QString("completed"));
        controller.undo();QTRY_COMPARE(controller.assignment("red").status,QString("completed"));QCOMPARE(controller.assignment("red").watermark.assetId,red.assetId);
        QCOMPARE(QImage(controller.assignment("red").outputPath).pixelColor(480,320),QColor(Qt::red));
    }
    void batchSharesLogoAndSavesOnce(){
        QTemporaryDir dir;const auto source=dir.filePath("photo.jpg");QVERIFY(write(source,encoded(solid({30,20}),"JPEG")));std::atomic_int entered{0};
        SortingController controller(nullptr,[&](ProcessingTask t,const CancelToken& cancel){++entered;while(!cancel->load())QThread::msleep(2);t.status="cancelled";return t;});
        const auto database=dir.filePath("session.sqlite");QVERIFY(controller.initialize(database));QVERIFY(controller.configure(config(dir.filePath("out"))));auto wm=logo();QVERIFY(controller.setWatermark(wm));
        QSignalSpy advance(&controller,&SortingController::advanceRequested);QElapsedTimer elapsed;elapsed.start();
        for(int i=0;i<200;++i){const auto photo=QString::number(i);QVERIFY(!controller.assign({photo,{source}},"club").isEmpty());QCOMPARE(controller.assignment(photo).watermark.asset.get(),wm.asset.get());}
        qInfo("200 watermarked assignments journaled and advanced in %lld ms",elapsed.elapsed());QCOMPARE(advance.size(),200);QTRY_COMPARE(entered.load(),1);QCOMPARE(controller.remaining(),200);
        bool timerFired=false;QTimer::singleShot(0,[&]{timerFired=true;});QTRY_VERIFY(timerFired);
        QVERIFY(execute(database,[](QSqlDatabase& db){QSqlQuery q(db);return q.exec("SELECT COUNT(*) FROM watermark_assets")&&q.next()&&q.value(0).toInt()==1;}));controller.cancelPending();QTRY_COMPARE(controller.remaining(),0);
    }
    void restartPreservesPngBytesAndGlobalSettings(){
        QTemporaryDir dir;const auto source=dir.filePath("photo.png");QVERIFY(write(source,encoded(solid({600,400}))));const auto database=dir.filePath("session.sqlite");
        auto old=logo(),current=logo(Qt::blue);current.settings.opacityPercent=70;current.settings.position=WatermarkPosition::TopLeft;
        auto queued=task(source,dir.filePath("out/Club"),old);QString error;
        {SessionStore store;QVERIFY(store.open(database,error));QVERIFY(store.saveConfig(config(dir.filePath("out")),error));QVERIFY(store.saveWatermark(current,error));QVERIFY(store.changeAssignment(queued.photo,queued.id,&queued,{},true,{},false,error));}
        old.asset.reset(); // Recovery must use the PNG stored in SQLite, with no source PNG file.
        {SortingController controller;QVERIFY(controller.initialize(database));QVERIFY(controller.watermark()==current);QVERIFY(controller.watermark().asset);QTRY_COMPARE(controller.assignment(queued.photo.id).status,QString("completed"));
         QCOMPARE(QImage(controller.assignment(queued.photo.id).outputPath).pixelColor(480,320),QColor(Qt::red));
         auto changed=controller.watermark();changed.settings.enabled=false;changed.settings.paddingPercent=8;QVERIFY(controller.setWatermark(changed));controller.shutdown();}
        {SortingController controller;QVERIFY(controller.initialize(database));QVERIFY(!controller.watermark().settings.enabled);QCOMPARE(controller.watermark().settings.paddingPercent,8);QCOMPARE(controller.watermark().asset->png,current.asset->png);}
    }
    void damagedAssetFailsWithoutPublishing(){
        QTemporaryDir dir;const auto database=dir.filePath("session.sqlite"),source=dir.filePath("photo.jpg");QVERIFY(write(source,encoded(solid({600,400}),"JPEG")));auto wm=logo();auto queued=task(source,dir.filePath("out/Club"),wm);QString error;
        {SessionStore store;QVERIFY(store.open(database,error));QVERIFY(store.saveConfig(config(dir.filePath("out")),error));QVERIFY(store.saveWatermark(wm,error));QVERIFY(store.changeAssignment(queued.photo,queued.id,&queued,{},true,{},false,error));}
        QVERIFY(execute(database,[](QSqlDatabase& db){QSqlQuery q(db);return q.exec("UPDATE watermark_assets SET png=X'00'");}));
        SortingController controller;QVERIFY(controller.initialize(database));QVERIFY(controller.watermark().settings.enabled);QVERIFY(!controller.watermark().asset);QVERIFY(controller.assign({"new",{source}},"club").isEmpty());
        QTRY_COMPARE(controller.assignment(queued.photo.id).status,QString("failed"));QVERIFY(!QFileInfo::exists(dir.filePath("out/Club/photo.jpg")));
        wm.settings.enabled=false;QVERIFY(controller.setWatermark(wm));QVERIFY(!controller.assign({"new",{source}},"club").isEmpty());QTRY_COMPARE(controller.assignment("new").status,QString("completed"));
    }
    void reimportRepairsDamagedAsset(){
        QTemporaryDir dir;const auto database=dir.filePath("session.sqlite");const auto wm=logo();QString error;
        {SessionStore store;QVERIFY(store.open(database,error));QVERIFY(store.saveWatermark(wm,error));}
        QVERIFY(execute(database,[](QSqlDatabase& db){QSqlQuery q(db);return q.exec("UPDATE watermark_assets SET png=X'00'");}));
        {SessionStore store;QVERIFY(store.open(database,error));SessionSnapshot snapshot;QVERIFY(store.load(snapshot,error));QVERIFY(!snapshot.watermark.asset);QVERIFY(store.saveWatermark(wm,error));}
        {SessionStore store;QVERIFY(store.open(database,error));SessionSnapshot snapshot;QVERIFY(store.load(snapshot,error));QVERIFY(snapshot.watermark.asset);QCOMPARE(snapshot.watermark.asset->png,wm.asset->png);}
    }
    void phase2Migration(){
        QTemporaryDir dir;const auto database=dir.filePath("legacy.sqlite");
        QVERIFY(execute(database,[](QSqlDatabase& db){QSqlQuery q(db);return q.exec("CREATE TABLE tasks(id TEXT PRIMARY KEY,photo_id TEXT NOT NULL,sources TEXT NOT NULL,category_id TEXT NOT NULL,category_name TEXT NOT NULL,directory TEXT NOT NULL,status TEXT NOT NULL,temp_path TEXT NOT NULL,output_path TEXT NOT NULL,digest BLOB NOT NULL,identity BLOB NOT NULL,error TEXT NOT NULL)")&&
            q.exec("INSERT INTO tasks VALUES('legacy','photo','[\"original.jpg\"]','club','Club','out','completed','','saved.jpg',X'01',X'02','')")&&q.exec("PRAGMA user_version=1");}));
        {SessionStore store;QString error;QVERIFY2(store.open(database,error),qPrintable(error));SessionSnapshot snapshot;QVERIFY(store.load(snapshot,error));QCOMPARE(snapshot.tasks.size(),1);QCOMPARE(snapshot.tasks[0].id,"legacy");QCOMPARE(snapshot.tasks[0].outputPath,"saved.jpg");QVERIFY(!snapshot.tasks[0].watermark.settings.enabled);QCOMPARE(snapshot.tasks[0].digest,QByteArray::fromHex("01"));}
        QVERIFY(execute(database,[](QSqlDatabase& db){QSqlQuery q(db);return q.exec("PRAGMA user_version")&&q.next()&&q.value(0).toInt()==7;}));
        QVERIFY(execute(database,[](QSqlDatabase& db){QSqlQuery q(db);return q.exec("UPDATE tasks SET watermark_settings='broken json'");}));
        SessionStore store;QString error;QVERIFY(store.open(database,error));SessionSnapshot snapshot;QVERIFY(!store.load(snapshot,error));QVERIFY(error.contains("watermark"));
    }
    void windowKeyboardWorkflow(){
        QTemporaryDir dir;QVERIFY(QDir().mkpath(dir.filePath("input")));QVERIFY(solid({1200,800}).save(dir.filePath("input/IMG_1.jpg")));QVERIFY(solid({800,1200}).save(dir.filePath("input/IMG_2.jpg")));
        const auto database=dir.filePath("session.sqlite");QString error;auto wm=logo();
        {SessionStore store;QVERIFY(store.open(database,error));QVERIFY(store.saveConfig(config(dir.filePath("out")),error));QVERIFY(store.saveWatermark(wm,error));}
        MainWindow window(8*1024*1024,nullptr,database);window.resize(1200,820);window.show();window.openFolder(dir.filePath("input"));QTest::qWait(400);
        auto* viewer=window.findChild<PhotoViewer*>();QVERIFY(viewer);QVERIFY(!redBounds(frame(*viewer)).isEmpty());
        window.findChild<QAction*>("watermarkSettings")->trigger();auto* dialog=window.findChild<WatermarkDialog*>();QVERIFY(dialog);QVERIFY(!dialog->isModal());
        dialog->findChild<QSlider*>("watermarkSize")->setValue(30);dialog->move(window.x()+780,window.y()+90);
        if(qEnvironmentVariableIsSet("PHOTOCULL_PHASE3_SCREENSHOT")){QCoreApplication::processEvents();QVERIFY(window.grab().save(qEnvironmentVariable("PHOTOCULL_PHASE3_SCREENSHOT")));QVERIFY(dialog->grab().save(qEnvironmentVariable("PHOTOCULL_PHASE3_SCREENSHOT")+".dialog.png"));}
        dialog->close();window.activateWindow();viewer->setFocus();QTest::keyClick(viewer,Qt::Key_1);
        bool advanced=false;for(const auto* label:window.findChildren<QLabel*>())if(label->text()=="2 / 2")advanced=true;QVERIFY(advanced);
        QTRY_VERIFY(QFileInfo::exists(dir.filePath("out/Club/IMG_1.jpg")));QImage output(dir.filePath("out/Club/IMG_1.jpg"));QCOMPARE(output.size(),QSize(1200,800));QVERIFY(near(output.pixelColor(1000,680),Qt::red,8));window.shutdown();
    }
};
QTEST_MAIN(WatermarkTest)
#include "test_watermark.moc"
