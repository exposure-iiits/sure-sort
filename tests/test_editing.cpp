#include "photo_edits.h"
#include "photo_viewer.h"
#include "edit_panel.h"
#include "edit_shortcuts_dialog.h"
#include "shortcut_manager.h"
#include "sorting_controller.h"
#include "main_window.h"
#include "raw_fixture.h"
#include <QApplication>
#include <QBuffer>
#include <QColorSpace>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <QTransform>
#include <atomic>

using namespace photocull;
namespace {
QImage solid(QSize size,QColor color=QColor(100,100,100)) {QImage image(size,QImage::Format_RGB32);image.fill(color);image.setColorSpace(QColorSpace::SRgb);return image;}
QImage pattern(QSize size={600,400}) {auto image=solid(size);for(int y=0;y<size.height();++y)for(int x=0;x<size.width();++x)image.setPixelColor(x,y,QColor(40+x*160/size.width(),40+y*160/size.height(),80));return image;}
QByteArray encode(const QImage& image,const char* format="PNG"){QByteArray bytes;QBuffer b(&bytes);b.open(QIODevice::WriteOnly);image.save(&b,format,100);return bytes;}
bool write(const QString& path,const QByteArray& bytes){QFile f(path);return f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size();}
QByteArray read(const QString& path){QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();}
DestinationConfig config(const QString& root){return {root,{{"club","Club","1","copy"}}};}
ProcessingTask task(const QString& source,const QString& output,const PhotoEdits& edits){ProcessingTask t;t.id=newId();t.photo={newId(),{source}};t.directory=output;t.categoryId="club";t.categoryName="Club";t.temporaryPath=QDir(output).filePath(".photocull-"+t.id+".part");t.edits=edits;return t;}
bool near(QColor a,QColor b,int tolerance=6){return qAbs(a.red()-b.red())<=tolerance&&qAbs(a.green()-b.green())<=tolerance&&qAbs(a.blue()-b.blue())<=tolerance;}
QImage frame(PhotoViewer& viewer){return viewer.grab().toImage().scaled(viewer.size(),Qt::IgnoreAspectRatio,Qt::SmoothTransformation);}
void drag(QWidget* w,QPointF from,QPointF to){
    QMouseEvent press(QEvent::MouseButtonPress,from,from,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(w,&press);
    QMouseEvent move(QEvent::MouseMove,to,to,Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(w,&move);
    QMouseEvent release(QEvent::MouseButtonRelease,to,to,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(w,&release);
}
QRectF displayedRect(QSize viewport,QSize source,double zoom=1){const double scale=qMin(double(viewport.width())/source.width(),double(viewport.height())/source.height())*zoom;const QSizeF size(source.width()*scale,source.height()*scale);return {QPointF((viewport.width()-size.width())/2,(viewport.height()-size.height())/2),size};}
QPointF screenPoint(const QRectF& photo,double x,double y){return photo.topLeft()+QPointF(x*photo.width(),y*photo.height());}
bool rectNear(QRectF a,QRectF b){return qAbs(a.x()-b.x())<.01&&qAbs(a.y()-b.y())<.01&&qAbs(a.width()-b.width())<.01&&qAbs(a.height()-b.height())<.01;}
WatermarkSnapshot watermark(){QString error;auto asset=decodeWatermarkPng(encode(solid({100,50},Qt::red)),"club.png",error);WatermarkSnapshot w;w.asset=asset;w.assetId=asset->id;w.settings.enabled=true;w.settings.sizePercent=30;w.settings.paddingPercent=5;return w;}
}
class EditingTest:public QObject {
    Q_OBJECT
private slots:
    void initTestCase(){qRegisterMetaType<PhotoEdits>();qRegisterMetaType<Photo>();}
    void modelAndSerialization(){
        PhotoEdits e;QVERIFY(e.isIdentity());e.exposure=12;e.brightness=-10;e.contrast=20;e.highlights=-30;e.shadows=40;e.saturation=-15;e.rotation=3;e.crop={.1,.2,.7,.5};
        PhotoEdits restored;QVERIFY(readEdits(editsJson(e),restored));QVERIFY(e==restored);QVERIFY(validateEdits(e).isEmpty());
        e.crop.setX(-.1);QVERIFY(!validateEdits(e).isEmpty());e={};e.rotation=4;QVERIFY(!validateEdits(e).isEmpty());
        auto json=editsJson({});json["exposure"]=.5;QVERIFY(!readEdits(json,restored));QVERIFY(!readEdits({{"exposure",1}},restored));QVERIFY(readEdits({},restored));QVERIFY(restored.isIdentity());
        auto token=std::make_shared<std::atomic_bool>(true);QVERIFY(renderEdits(solid({20,20}),{},token).isNull());
    }
    void individualAdjustments_data(){QTest::addColumn<int>("adjustment");for(int i=0;i<6;++i)QTest::newRow(qPrintable(QString::number(i)))<<i;}
    void individualAdjustments(){
        QFETCH(int,adjustment);PhotoEdits e;int* fields[]={&e.exposure,&e.brightness,&e.contrast,&e.highlights,&e.shadows,&e.saturation};*fields[adjustment]=adjustment==0?10:40;
        auto input=solid({100,100},adjustment==3?QColor(200,160,140):QColor(120,80,60));const auto original=input.copy();auto output=renderEdits(input,e);QVERIFY(!output.isNull());QCOMPARE(input,original);QVERIFY(output.pixelColor(50,50)!=input.pixelColor(50,50));
        if(adjustment==0){e.exposure=10;QVERIFY(near(renderEdits(solid({10,10},QColor(128,128,128)),e).pixelColor(5,5),QColor(176,176,176),2));}
        if(adjustment==2){QVERIFY(renderEdits(solid({10,10},QColor(60,60,60)),e).pixelColor(5,5).red()<60);QVERIFY(renderEdits(solid({10,10},QColor(200,200,200)),e).pixelColor(5,5).red()>200);}
        if(adjustment==3){QCOMPARE(renderEdits(solid({10,10},QColor(40,40,40)),e).pixelColor(5,5),QColor(40,40,40));QVERIFY(renderEdits(solid({10,10},QColor(200,200,200)),e).pixelColor(5,5).red()>200);}
        if(adjustment==4){QCOMPARE(renderEdits(solid({10,10},QColor(200,200,200)),e).pixelColor(5,5),QColor(200,200,200));}
        if(adjustment==5){e.saturation=-100;const auto gray=renderEdits(input,e).pixelColor(5,5);QCOMPARE(gray.red(),gray.green());QCOMPARE(gray.green(),gray.blue());}
    }
    void rotationAndOriginalCrop_data(){QTest::addColumn<int>("rotation");for(int i=0;i<4;++i)QTest::newRow(qPrintable(QString::number(i)))<<i;}
    void rotationAndOriginalCrop(){
        QFETCH(int,rotation);const auto input=pattern({1200,800});PhotoEdits e;e.rotation=rotation;e.crop={.1,.2,.4,.5};const auto output=renderEdits(input,e);
        const auto expected=input.copy(120,160,480,400).transformed(QTransform().rotate(rotation*90),Qt::FastTransformation);
        QCOMPARE(output,expected);QCOMPARE(output.size(),rotation%2?QSize(400,480):QSize(480,400));
        QVERIFY(rectNear(rotatedNormalizedRect(rotatedNormalizedRect(e.crop,rotation),-rotation),e.crop));
    }
    void degreeRotationPreviewAndOutput_data(){
        QTest::addColumn<int>("degrees");
        for(int degrees:{-180,-137,-90,-37,0,23,90,137,180})QTest::newRow(qPrintable(QString::number(degrees)))<<degrees;
    }
    void degreeRotationPreviewAndOutput(){
        QFETCH(int,degrees);const auto input=pattern({240,160});PhotoEdits e;e.rotationDegrees=degrees;
        const auto expected=input.transformed(QTransform().rotate(degrees),degrees%90?Qt::SmoothTransformation:Qt::FastTransformation);
        const auto output=renderEdits(input,e);QCOMPARE(output.size(),expected.size());QCOMPARE(output,expected);QCOMPARE(editedSize(input.size(),e),output.size());
        PhotoEdits restored;QVERIFY(readEdits(editsJson(e),restored));QCOMPARE(restored,e);
        PhotoViewer viewer;viewer.resize(output.size());viewer.show();viewer.setPreview({input,input.size(),{},{}});viewer.setEdits(e);
        const auto shown=frame(viewer);
        for(int y=12;y<output.height()-12;y+=13)for(int x=12;x<output.width()-12;x+=13)
            if(output.pixelColor(x,y).alpha()==255)QVERIFY2(near(shown.pixelColor(x,y),output.pixelColor(x,y),5),qPrintable(QString("%1° pixel %2,%3").arg(degrees).arg(x).arg(y)));
    }
    void degreeSliderCropAndRestart(){
        QTemporaryDir dir;const auto input=pattern({600,400});const auto database=dir.filePath("rotation.sqlite"),source=dir.filePath("photo.png");QVERIFY(input.save(source));
        EditPanel panel;auto* slider=panel.findChild<QSlider*>("editRotation");QVERIFY(slider);QCOMPARE(slider->minimum(),-180);QCOMPARE(slider->maximum(),180);
        QSignalSpy changes(&panel,&EditPanel::changed);slider->setValue(-37);QCOMPARE(changes.size(),1);auto e=changes.last()[0].value<PhotoEdits>();QCOMPARE(editRotationDegrees(e),-37);
        PhotoViewer viewer;viewer.resize(600,600);viewer.show();viewer.setPreview({input,input.size(),{},{}});viewer.setEdits(e);viewer.beginCrop();
        const auto photo=displayedRect(viewer.size(),editRotatedSize(input.size(),e));drag(&viewer,screenPoint(photo,.25,.25),screenPoint(photo,.75,.75));viewer.confirmCrop();e=viewer.edits();QVERIFY(e.cropInRotatedSpace);QVERIFY(rectNear(e.crop,{.25,.25,.5,.5}));
        const auto rotated=input.transformed(QTransform().rotate(-37),Qt::SmoothTransformation);const auto expected=rotated.copy(editCropPixels(input.size(),e));QCOMPARE(renderEdits(input,e),expected);
        QString output;Photo p{"free-angle",{source}};
        {SortingController c;QVERIFY(c.initialize(database));QVERIFY(c.configure(config(dir.filePath("out"))));QVERIFY(c.setEdits(p,e));QVERIFY(!c.assign(p,"club").isEmpty());QTRY_COMPARE(c.assignment(p.id).status,QString("completed"));output=c.assignment(p.id).outputPath;QCOMPARE(QImage(output).convertToFormat(expected.format()),expected);}
        {SortingController c;QVERIFY(c.initialize(database));QCOMPARE(c.edits(p.id),e);QCOMPARE(c.assignment(p.id).edits,e);QCOMPARE(QImage(output).convertToFormat(expected.format()),expected);}
        slider->setValue(180);QCOMPARE(editRotationDegrees(changes.last()[0].value<PhotoEdits>()),180);QTest::mouseDClick(slider,Qt::LeftButton);QCOMPARE(slider->value(),0);
        auto legacy=editsJson({});legacy.remove("rotationDegrees");legacy.remove("cropInRotatedSpace");PhotoEdits restored;QVERIFY(readEdits(legacy,restored));QVERIFY(restored.isIdentity());
        auto invalid=editsJson({});invalid["rotationDegrees"]=181;QVERIFY(!readEdits(invalid,restored));invalid["rotationDegrees"]=.5;QVERIFY(!readEdits(invalid,restored));
    }
    void cropGestures_data(){QTest::addColumn<int>("rotation");QTest::addColumn<double>("zoom");for(int i=0;i<4;++i)for(double zoom:{1.0,1.6})QTest::newRow(qPrintable(QString("r%1-z%2").arg(i).arg(zoom)))<<i<<zoom;}
    void cropGestures(){
        QFETCH(int,rotation);QFETCH(double,zoom);PhotoViewer viewer;viewer.resize(600,400);viewer.show();viewer.setPreview({pattern(),{1200,800},{},{}});
        PhotoEdits e;e.rotation=rotation;QSignalSpy rendered(&viewer,&PhotoViewer::editsRendered);viewer.setEdits(e);QTRY_VERIFY(!rendered.isEmpty());
        viewer.zoomBy(zoom);viewer.beginCrop();const auto photo=displayedRect(viewer.size(),rotatedSize({1200,800},rotation),zoom);
        drag(&viewer,screenPoint(photo,.25,.25),screenPoint(photo,.75,.75));QVERIFY(rectNear(viewer.cropSelection(),QRectF(.25,.25,.5,.5)));
        QSignalSpy confirmed(&viewer,&PhotoViewer::cropConfirmed);viewer.confirmCrop();QCOMPARE(confirmed.size(),1);QVERIFY(!viewer.isCropping());
        const auto crop=confirmed[0][0].toRectF();QVERIFY(rectNear(crop,rotatedNormalizedRect({.25,.25,.5,.5},-rotation)));
        viewer.fit();viewer.resize(480,420);viewer.beginCrop();QVERIFY(rectNear(viewer.cropSelection(),QRectF(.25,.25,.5,.5)));viewer.cancelCrop();
    }
    void cropAspectsAndHandles_data(){QTest::addColumn<double>("ratio");for(double ratio:{0.0,1.0,4.0/3,1.5,16.0/9})QTest::newRow(qPrintable(QString::number(ratio)))<<ratio;}
    void cropAspectsAndHandles(){
        QFETCH(double,ratio);PhotoViewer viewer;viewer.resize(600,400);viewer.show();viewer.setPreview({pattern(),{1200,800},{},{}});viewer.setCropAspect(ratio);viewer.beginCrop();
        drag(&viewer,{120,80},{360,240});auto crop=viewer.cropSelection();QVERIFY(QRectF(0,0,1,1).contains(crop));if(ratio)QVERIFY(qAbs(crop.width()*600/(crop.height()*400)-ratio)<.02);
        const auto old=crop;drag(&viewer,{crop.center().x()*600,crop.center().y()*400},{crop.center().x()*600+30,crop.center().y()*400+20});crop=viewer.cropSelection();QCOMPARE(crop.size(),old.size());QVERIFY(crop.left()>old.left());
        drag(&viewer,{crop.right()*600,crop.bottom()*400},{crop.right()*600+30,crop.bottom()*400+20});QVERIFY(viewer.cropSelection().width()>crop.width());
        QTest::keyClick(&viewer,Qt::Key_Escape);QVERIFY(!viewer.isCropping());QVERIFY(viewer.edits().isIdentity());
        PhotoEdits existing;existing.crop={.2,.2,.5,.4};viewer.setEdits(existing);viewer.setCropAspect(1);viewer.beginCrop();
        const auto fixed=viewer.cropSelection();QVERIFY(qAbs(fixed.width()*600/(fixed.height()*400)-1)<.02);viewer.cancelCrop();
    }
    void asynchronousPreviewAndPanel(){
        PhotoViewer viewer;viewer.resize(600,400);viewer.show();const auto input=solid({1800,1200},QColor(80,100,120));const auto original=input.copy();viewer.setPreview({input,input.size(),{},{}});
        EditPanel panel;connect(&panel,&EditPanel::changed,&viewer,&PhotoViewer::setEdits);panel.show();auto* slider=panel.findChild<QSlider*>("editExposure");QVERIFY(slider);
        QSignalSpy rendered(&viewer,&PhotoViewer::editsRendered);int beats=0;QTimer timer;timer.setInterval(1);connect(&timer,&QTimer::timeout,this,[&]{++beats;});timer.start();
        for(int i=0;i<40;++i)slider->setValue(i%30);
        slider->setValue(10);QTRY_VERIFY(!rendered.isEmpty());QTRY_VERIFY(near(frame(viewer).pixelColor(300,200),renderEdits(input,viewer.edits()).pixelColor(900,600),2));QVERIFY(beats>0);QCOMPARE(input,original);
        QTest::mouseDClick(slider,Qt::LeftButton);QCOMPARE(slider->value(),0);QVERIFY(viewer.edits().isIdentity());
        slider->setValue(20);viewer.resetPhoto();viewer.setPreview({solid({600,400},Qt::blue),{600,400},{},{}});QTest::qWait(80);QCOMPARE(frame(viewer).pixelColor(300,200),QColor(Qt::blue));
        QTest::mouseClick(panel.findChild<QPushButton*>("resetEdits"),Qt::LeftButton);QVERIFY(viewer.edits().isIdentity());
    }
    void cropWhilePanning(){
        PhotoViewer viewer;viewer.resize(600,400);viewer.show();viewer.setPreview({pattern(),{1200,800},{},{}});viewer.zoomBy(2);viewer.beginCrop();
        const auto photo=displayedRect(viewer.size(),{1200,800},2);drag(&viewer,screenPoint(photo,.4,.4),screenPoint(photo,.6,.6));const auto draft=viewer.cropSelection();
        QMouseEvent press(QEvent::MouseButtonPress,{300,200},{300,200},Qt::MiddleButton,Qt::MiddleButton,Qt::NoModifier);QApplication::sendEvent(&viewer,&press);
        QMouseEvent move(QEvent::MouseMove,{420,240},{420,240},Qt::NoButton,Qt::MiddleButton,Qt::NoModifier);QApplication::sendEvent(&viewer,&move);
        QMouseEvent release(QEvent::MouseButtonRelease,{420,240},{420,240},Qt::MiddleButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(&viewer,&release);
        QCOMPARE(viewer.cropSelection(),draft);viewer.confirmCrop();QVERIFY(rectNear(viewer.edits().crop,QRectF(.4,.4,.2,.2)));
    }
    void imageDeviceRatioDoesNotChangeGeometry(){
        PhotoViewer viewer;viewer.resize(600,400);viewer.show();auto image=solid({600,400},Qt::blue);image.setDevicePixelRatio(2);viewer.setPreview({image,image.size(),{},{}});
        QCOMPARE(frame(viewer).pixelColor(550,350),QColor(Qt::blue));QCOMPARE(frame(viewer).pixelColor(50,50),QColor(Qt::blue));
        auto wm=watermark();QString error;QVERIFY(compositeWatermark(image,wm,error));QCOMPARE(image.devicePixelRatio(),1);QVERIFY(near(image.pixelColor(watermarkRect(image.size(),wm.asset->image.size(),wm.settings).center().toPoint()),Qt::red,2));
    }
    void largePreviewRefinesAndCancels(){
        PhotoViewer viewer;viewer.resize(600,400);viewer.show();viewer.beginCrop();viewer.cancelCrop();const auto image=solid({2400,1600},QColor(80,100,120));viewer.setPreview({image,image.size(),{},{}});QVERIFY(!viewer.isCropping());PhotoEdits e;e.exposure=5;
        QSignalSpy rendered(&viewer,&PhotoViewer::editsRendered);viewer.setEdits(e);QTRY_COMPARE_WITH_TIMEOUT(rendered.size(),2,4000);QCOMPARE(viewer.editedSourceSize(),image.size());
        QVERIFY(near(frame(viewer).pixelColor(300,200),renderEdits(image,e).pixelColor(100,100),2));
        e.exposure=10;viewer.setEdits(e);viewer.resetPhoto();viewer.setPreview({solid({600,400},Qt::blue),{600,400},{},{}});QTest::qWait(350);QCOMPARE(frame(viewer).pixelColor(300,200),QColor(Qt::blue));
    }
    void shortcutsRepeatCustomizationAndConflicts(){
        QWidget window;window.show();ShortcutManager manager(&window);QSignalSpy actions(&manager,&ShortcutManager::triggered);QString error;QVERIFY(manager.setDestinations({{"club",QKeySequence(Qt::Key_1)}},error));
        const int holds[]={Qt::Key_E,Qt::Key_B,Qt::Key_C,Qt::Key_H,Qt::Key_S,Qt::Key_T,Qt::Key_R};
        for(int key:holds){QTest::keyPress(&window,Qt::Key(key));QTest::keyClick(&window,Qt::Key_Right);QVERIFY(actions.last()[0].toString().startsWith("edit:"));const auto before=actions.size();
            QKeyEvent repeated(QEvent::KeyPress,Qt::Key_Right,Qt::NoModifier,QString(),true,3);QApplication::sendEvent(&window,&repeated);QCOMPARE(actions.size(),before+1);QTest::keyRelease(&window,Qt::Key(key));}
        QTest::keyClick(&window,Qt::Key_Right);QCOMPARE(actions.last()[0].toString(),"next");QTest::keyClick(&window,Qt::Key_1);QCOMPARE(actions.last()[0].toString(),"assign:club");
        auto bindings=manager.editBindings();auto row=bindings[0].toObject();row["hold"]="U";row["trigger"]="Up";bindings[0]=row;QVERIFY(manager.setEditBindings(bindings,error));
        QTest::keyPress(&window,Qt::Key_U);QTest::keyClick(&window,Qt::Key_Up);QCOMPARE(actions.last()[0].toString(),"edit:exposureUp");QTest::keyRelease(&window,Qt::Key_U);
        auto bad=bindings;row["trigger"]="1";bad[0]=row;QVERIFY(!manager.validateEditBindings(bad,error));row["trigger"]="Up";row["hold"]="F";bad[0]=row;QVERIFY(!manager.validateEditBindings(bad,error));
        bad=bindings;bad[1]=bindings[0];QVERIFY(!manager.validateEditBindings(bad,error));
        EditShortcutsDialog dialog(&manager);QVERIFY(dialog.bindings()==bindings);
    }
    void fullResolutionOutputAndWatermark(){
        QTemporaryDir dir;PhotoEdits e;e.exposure=7;e.brightness=10;e.contrast=15;e.highlights=-10;e.shadows=20;e.saturation=-15;e.rotation=1;e.crop={.25,.25,.5,.5};const auto wm=watermark();
        for(const QString ext:{"jpg","png","dng"}){
            const auto path=dir.filePath("photo."+ext);const auto input=pattern({1200,800});const auto bytes=ext=="dng"?fixture::rawDng({encode(solid({80,40}),"JPEG")}):encode(input,ext=="jpg"?"JPEG":"PNG");QVERIFY(write(path,bytes));
            auto t=task(path,dir.filePath("out-"+ext),e);t.watermark=wm;const auto prepared=ProcessingPipeline::prepare(t,{});QVERIFY2(prepared.status=="prepared",qPrintable(prepared.error));
            const auto completed=ProcessingPipeline::publish(prepared,{});QCOMPARE(completed.status,"completed");const QImage output(completed.outputPath);
            QCOMPARE(output.size(),ext=="dng"?QSize(256,192):QSize(400,600));QCOMPARE(output.colorSpace(),QColorSpace(QColorSpace::SRgb));const auto mark=watermarkRect(output.size(),wm.asset->image.size(),wm.settings);QVERIFY(near(output.pixelColor(mark.center().toPoint()),Qt::red,8));
            if(ext!="dng"){auto expected=renderEdits(QImage(path),e);QString error;QVERIFY(compositeWatermark(expected,wm,error));QVERIFY(near(output.pixelColor(20,20),expected.pixelColor(20,20),8));}
            QCOMPARE(read(path),bytes);
        }
    }
    void previewMatchesEditedSavedImage(){
        QTemporaryDir dir;const auto input=pattern();const auto source=dir.filePath("source.png");QVERIFY(input.save(source));PhotoEdits e;e.exposure=5;e.contrast=15;e.shadows=20;e.saturation=-25;e.rotation=1;e.crop={.1,.1,.8,.8};auto wm=watermark();
        auto t=task(source,dir.filePath("out"),e);t.watermark=wm;const auto p=ProcessingPipeline::prepare(t,{});QVERIFY2(p.status=="prepared",qPrintable(p.error));const auto completed=ProcessingPipeline::publish(p,{});const QImage output(completed.outputPath);
        PhotoViewer viewer;viewer.resize(output.size());viewer.show();viewer.setPreview({input,input.size(),{},{}});viewer.setWatermark(wm);QSignalSpy rendered(&viewer,&PhotoViewer::editsRendered);viewer.setEdits(e);QTRY_VERIFY(!rendered.isEmpty());const auto shown=frame(viewer);
        QCOMPARE(shown.size(),output.size());for(int y=10;y<output.height()-10;y+=17)for(int x=10;x<output.width()-10;x+=17)QVERIFY(near(shown.pixelColor(x,y),output.pixelColor(x,y),3));
    }
    void editPersistenceSnapshotAndRestart(){
        QTemporaryDir dir;const auto database=dir.filePath("session.sqlite"),source=dir.filePath("photo.png");QVERIFY(pattern().save(source));Photo photo{"photo",{source}};PhotoEdits e;e.exposure=10;e.rotation=1;e.crop={.1,.2,.6,.6};auto bindings=ShortcutManager::defaultEditBindings();auto row=bindings[0].toObject();row["hold"]="U";bindings[0]=row;
        {SortingController controller;QVERIFY(controller.initialize(database));QVERIFY(controller.configure(config(dir.filePath("out"))));QVERIFY(controller.setEdits(photo,e));QVERIFY(controller.saveEditBindings(bindings));QVERIFY(!controller.assign(photo,"club").isEmpty());QCOMPARE(controller.assignment(photo.id).edits,e);QTRY_COMPARE(controller.assignment(photo.id).status,QString("completed"));}
        {SortingController controller;QVERIFY(controller.initialize(database));QCOMPARE(controller.edits(photo.id),e);QCOMPARE(controller.editBindings(),bindings);QCOMPARE(controller.assignment(photo.id).edits,e);}
        // Editing an unsorted photo also flushes on immediate shutdown.
        {SortingController controller;QVERIFY(controller.initialize(database));PhotoEdits other;other.shadows=24;QVERIFY(controller.setEdits({"other",{source}},other));controller.shutdown();}
        {SortingController controller;QVERIFY(controller.initialize(database));QCOMPARE(controller.edits("other").shadows,24);}
    }
    void sortedEditRefreshAndExternalChanges(){
        QTemporaryDir dir;const auto source=dir.filePath("photo.jpg");const auto original=encode(solid({600,400}),"JPEG");QVERIFY(write(source,original));std::atomic_int prepares{0};
        SortingController controller(nullptr,[&](ProcessingTask t,const CancelToken& cancel){++prepares;return ProcessingPipeline::prepare(t,cancel);});QVERIFY(controller.initialize(dir.filePath("session.sqlite")));QVERIFY(controller.configure(config(dir.filePath("out"))));Photo photo{"photo",{source}};
        controller.assign(photo,"club");QTRY_COMPARE(controller.assignment(photo.id).status,QString("completed"));const auto first=controller.assignment(photo.id);PhotoEdits e;
        for(int i=1;i<=20;++i){e.exposure=i;QVERIFY(controller.setEdits(photo,e));}
        QTRY_VERIFY_WITH_TIMEOUT(controller.assignment(photo.id).id!=first.id&&controller.assignment(photo.id).status=="completed",8000);const auto second=controller.assignment(photo.id);QCOMPARE(second.edits.exposure,20);QCOMPARE(prepares.load(),2);
#ifdef Q_OS_WIN
        QCOMPARE(second.outputPath,first.outputPath);QCOMPARE(QDir(second.directory).entryList({"*.jpg"},QDir::Files).size(),1);
#endif
        QVERIFY(QImage(second.outputPath).pixelColor(50,50).red()>150);QCOMPARE(read(source),original);
        e={};QVERIFY(controller.setEdits(photo,e));QTRY_VERIFY_WITH_TIMEOUT(controller.assignment(photo.id).id!=second.id&&controller.assignment(photo.id).status=="completed",8000);
        const auto reset=controller.assignment(photo.id);QCOMPARE(read(reset.outputPath),original);
        QVERIFY(controller.setWatermark(watermark()));QVERIFY(write(reset.outputPath,"external user edit"));e.brightness=20;QVERIFY(controller.setEdits(photo,e));
        QTRY_VERIFY_WITH_TIMEOUT(controller.assignment(photo.id).id!=reset.id&&controller.assignment(photo.id).status=="completed",8000);
        QCOMPARE(read(reset.outputPath),QByteArray("external user edit"));QVERIFY(controller.assignment(photo.id).outputPath!=reset.outputPath);QCOMPARE(read(source),original);
        const auto updated=QImage(controller.assignment(photo.id).outputPath);QVERIFY(near(updated.pixelColor(watermarkRect(updated.size(),{100,50},watermark().settings).center().toPoint()),Qt::red,8));QVERIFY(!controller.issues().isEmpty());
    }
    void queuedSnapshotThenAutomaticRefresh(){
        QTemporaryDir dir;const auto source=dir.filePath("photo.jpg");QVERIFY(write(source,encode(solid({600,400}),"JPEG")));std::atomic_bool release{false};std::atomic_int calls{0},firstExposure{-999};
        SortingController controller(nullptr,[&](ProcessingTask t,const CancelToken& cancel){if(calls.fetch_add(1)==0){firstExposure=t.edits.exposure;while(!release&&!cancel->load())QThread::msleep(2);}return ProcessingPipeline::prepare(t,cancel);});
        QVERIFY(controller.initialize(dir.filePath("session.sqlite")));QVERIFY(controller.configure(config(dir.filePath("out"))));QSignalSpy advance(&controller,&SortingController::advanceRequested);
        Photo one{"one",{source}},two{"two",{source}};controller.assign(one,"club");PhotoEdits e;e.exposure=10;QVERIFY(controller.setEdits(one,e));e.exposure=-10;QVERIFY(controller.setEdits(two,e));controller.assign(two,"club");
        QTRY_COMPARE(firstExposure.load(),0);QCOMPARE(controller.assignment(one.id).edits.exposure,0);QCOMPARE(controller.assignment(two.id).edits.exposure,-10);QCOMPARE(advance.size(),2);release=true;
        QTRY_VERIFY_WITH_TIMEOUT(controller.assignment(one.id).edits.exposure==10&&controller.assignment(one.id).status=="completed"&&controller.assignment(two.id).status=="completed",8000);
        QCOMPARE(advance.size(),2);QVERIFY(QImage(controller.assignment(one.id).outputPath).pixelColor(20,20).red()>QImage(controller.assignment(two.id).outputPath).pixelColor(20,20).red());
    }
    void cancelledRefreshKeepsCompletedOutput(){
        QTemporaryDir dir;const auto source=dir.filePath("photo.jpg");QVERIFY(write(source,encode(solid({600,400}),"JPEG")));std::atomic_int calls{0};
        SortingController controller(nullptr,[&](ProcessingTask t,const CancelToken& cancel){if(++calls==2)while(!cancel->load())QThread::msleep(2);return ProcessingPipeline::prepare(t,cancel);});
        QVERIFY(controller.initialize(dir.filePath("session.sqlite")));QVERIFY(controller.configure(config(dir.filePath("out"))));Photo photo{"photo",{source}};controller.assign(photo,"club");QTRY_COMPARE(controller.assignment(photo.id).status,QString("completed"));
        const auto previous=controller.assignment(photo.id);const auto bytes=read(previous.outputPath);PhotoEdits e;e.exposure=10;QVERIFY(controller.setEdits(photo,e));QTRY_COMPARE(calls.load(),2);controller.cancelPending();QTest::qWait(80);
        QCOMPARE(controller.assignment(photo.id).status,QString("cancelled"));QCOMPARE(read(previous.outputPath),bytes);controller.retryFailed();QTRY_COMPARE(controller.assignment(photo.id).status,QString("completed"));
#ifdef Q_OS_WIN
        QCOMPARE(controller.assignment(photo.id).outputPath,previous.outputPath);QCOMPARE(QDir(previous.directory).entryList({"*.jpg"},QDir::Files).size(),1);
#endif
    }
    void failedRefreshRetainsPreviousOutputAndRetry(){
        QTemporaryDir dir;const auto source=dir.filePath("photo.jpg");const auto original=encode(solid({600,400}),"JPEG");QVERIFY(write(source,original));SortingController controller;
        QVERIFY(controller.initialize(dir.filePath("session.sqlite")));QVERIFY(controller.configure(config(dir.filePath("out"))));Photo photo{"photo",{source}};controller.assign(photo,"club");QTRY_COMPARE(controller.assignment(photo.id).status,QString("completed"));const auto previous=controller.assignment(photo.id);const auto saved=read(previous.outputPath);
        QVERIFY(QFile::remove(source));PhotoEdits e;e.exposure=10;QVERIFY(controller.setEdits(photo,e));QTRY_COMPARE(controller.assignment(photo.id).status,QString("failed"));QCOMPARE(read(previous.outputPath),saved);
        QVERIFY(write(source,original));controller.retryFailed();QTRY_COMPARE(controller.assignment(photo.id).status,QString("completed"));
#ifdef Q_OS_WIN
        QCOMPARE(controller.assignment(photo.id).outputPath,previous.outputPath);QCOMPARE(QDir(previous.directory).entryList({"*.jpg"},QDir::Files).size(),1);
#endif
        const auto updated=controller.assignment(photo.id);const auto updatedBytes=read(updated.outputPath);QVERIFY(QFile::remove(source));e.exposure=20;QVERIFY(controller.setEdits(photo,e));
        QVERIFY(!controller.assign(photo,"club").isEmpty());QTRY_COMPARE(controller.assignment(photo.id).status,QString("failed"));QCOMPARE(read(updated.outputPath),updatedBytes);
    }
    void preparedRefreshRecoveryAndRace(){
        QTemporaryDir dir;const auto source=dir.filePath("photo.jpg");QVERIFY(write(source,encode(solid({600,400}),"JPEG")));auto first=ProcessingPipeline::publish(ProcessingPipeline::prepare(task(source,dir.filePath("out"),{}),{}),{});QCOMPARE(first.status,"completed");
        PhotoEdits e;e.exposure=10;auto next=task(source,first.directory,e);next.predecessorPath=first.outputPath;next.predecessorDigest=first.digest;next.predecessorIdentity=first.identity;
        const auto prepared=ProcessingPipeline::prepare(next,{});QCOMPARE(prepared.status,"prepared");
#ifdef Q_OS_WIN
        QCOMPARE(prepared.outputPath,first.outputPath);QCOMPARE(read(first.outputPath),read(source));const auto recovered=ProcessingPipeline::recover(prepared,{});QCOMPARE(recovered.status,"completed");QCOMPARE(recovered.outputPath,first.outputPath);
        const auto again=ProcessingPipeline::recover(prepared,{});QCOMPARE(again.status,"completed");
        next=task(source,first.directory,e);next.predecessorPath=recovered.outputPath;next.predecessorDigest=recovered.digest;next.predecessorIdentity=recovered.identity;
        const auto race=ProcessingPipeline::prepare(next,{});QCOMPARE(race.status,"prepared");QVERIFY(write(recovered.outputPath,"changed after prepare"));const auto result=ProcessingPipeline::publish(race,{});QCOMPARE(result.status,"failed");QCOMPARE(read(recovered.outputPath),QByteArray("changed after prepare"));
#endif
    }
    void schemaTwoMigration(){
        QTemporaryDir dir;const auto database=dir.filePath("legacy.sqlite");QString error;{SessionStore store;QVERIFY(store.open(database,error));PhotoEdits e;e.shadows=15;QVERIFY(store.saveEdits("keep",e,error));}
        const auto name=newId();{auto db=QSqlDatabase::addDatabase("QSQLITE",name);db.setDatabaseName(database);QVERIFY(db.open());QSqlQuery q(db);QVERIFY(q.exec("DROP TABLE photo_edits"));QVERIFY(q.exec("ALTER TABLE tasks DROP COLUMN edits"));QVERIFY(q.exec("PRAGMA user_version=2"));db.close();}QSqlDatabase::removeDatabase(name);
        SessionStore store;QVERIFY(store.open(database,error));SessionSnapshot snapshot;QVERIFY(store.load(snapshot,error));QVERIFY(snapshot.edits.isEmpty());PhotoEdits e;e.saturation=-50;QVERIFY(store.saveEdits("new",e,error));
    }
    void windowWorkflowAndNavigation(){
        QTemporaryDir dir;const auto database=dir.filePath("session.sqlite");QVERIFY(QDir().mkpath(dir.filePath("input")));QVERIFY(pattern({1200,800}).save(dir.filePath("input/IMG_1.jpg")));QVERIFY(pattern({800,1200}).save(dir.filePath("input/IMG_2.jpg")));QString error;
        {SessionStore store;QVERIFY(store.open(database,error));QVERIFY(store.saveConfig(config(dir.filePath("out")),error));QVERIFY(store.saveWatermark(watermark(),error));}
        MainWindow window(12*1024*1024,nullptr,database);window.show();window.openFolder(dir.filePath("input"));QTest::qWait(450);auto* viewer=window.findChild<PhotoViewer*>();QVERIFY(viewer);auto* panel=window.findChild<EditPanel*>();QVERIFY(panel);QVERIFY(!panel->isVisible());QTest::mouseClick(window.findChild<QPushButton*>("toggleEdit"),Qt::LeftButton);QVERIFY(panel->isVisible());
        viewer->setFocus();QTest::keyPress(viewer,Qt::Key_E);QTest::keyClick(viewer,Qt::Key_Right);QTest::keyRelease(viewer,Qt::Key_E);QCOMPARE(viewer->edits().exposure,5);
        QTest::keyClick(viewer,Qt::Key_Right);QTRY_VERIFY(viewer->edits().isIdentity());QTest::keyClick(viewer,Qt::Key_Left);QTRY_COMPARE(viewer->edits().exposure,5);
        panel->findChild<QSlider*>("editShadows")->setValue(20);panel->findChild<QSlider*>("editRotation")->setValue(90);QCOMPARE(editRotationDegrees(viewer->edits()),90);
        QTest::mouseClick(panel->findChild<QPushButton*>("cropPhoto"),Qt::LeftButton);QTRY_VERIFY(viewer->isCropping());const auto photo=displayedRect(viewer->size(),{800,1200});drag(viewer,screenPoint(photo,.2,.2),screenPoint(photo,.8,.8));
        QTest::keyClick(viewer,Qt::Key_1);QVERIFY(viewer->isCropping());QVERIFY(!QFileInfo::exists(dir.filePath("out/Club/IMG_1.jpg")));
        for(auto* button:window.findChildren<QPushButton*>())if(button->text().startsWith("1  Club"))QTest::mouseClick(button,Qt::LeftButton);
        QVERIFY(viewer->isCropping());QVERIFY(!QFileInfo::exists(dir.filePath("out/Club/IMG_1.jpg")));
        if(qEnvironmentVariableIsSet("PHOTOCULL_PHASE4_SCREENSHOT")){QTest::qWait(80);QVERIFY(window.grab().save(qEnvironmentVariable("PHOTOCULL_PHASE4_SCREENSHOT")+".crop.png"));}
        QTest::keyClick(viewer,Qt::Key_Return);QVERIFY(!viewer->isCropping());QVERIFY(viewer->edits().crop!=QRectF(0,0,1,1));QTest::qWait(200);
        if(qEnvironmentVariableIsSet("PHOTOCULL_PHASE4_SCREENSHOT"))QVERIFY(window.grab().save(qEnvironmentVariable("PHOTOCULL_PHASE4_SCREENSHOT")));
        viewer->setFocus();QTest::keyClick(viewer,Qt::Key_1);bool advanced=false;for(const auto* label:window.findChildren<QLabel*>())if(label->text()=="2 / 2")advanced=true;QVERIFY(advanced);
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(dir.filePath("out/Club/IMG_1.jpg")),8000);const QImage output(dir.filePath("out/Club/IMG_1.jpg"));QCOMPARE(output.size(),QSize(480,720));QVERIFY(near(output.pixelColor(watermarkRect(output.size(),{100,50},watermark().settings).center().toPoint()),Qt::red,8));window.shutdown();
    }
    void legacyFolderKeyTakesPriority(){
        QTemporaryDir dir;const auto database=dir.filePath("session.sqlite");QVERIFY(QDir().mkpath(dir.filePath("input")));QVERIFY(pattern().save(dir.filePath("input/photo.jpg")));auto destinations=config(dir.filePath("out"));destinations.folders[0].shortcut="E";QString error;
        {SessionStore store;QVERIFY(store.open(database,error));QVERIFY(store.saveConfig(destinations,error));}
        MainWindow window(8*1024*1024,nullptr,database);window.show();window.openFolder(dir.filePath("input"));QTest::qWait(300);auto* viewer=window.findChild<PhotoViewer*>();auto* manager=window.findChild<ShortcutManager*>();QVERIFY(manager);QVERIFY(manager->editBindings()[0].toObject()["trigger"].toString().isEmpty());
        QTest::keyClick(viewer,Qt::Key_E);QTRY_VERIFY(QFileInfo::exists(dir.filePath("out/Club/photo.jpg")));QVERIFY(viewer->edits().isIdentity());window.shutdown();
    }
};
QTEST_MAIN(EditingTest)
#include "test_editing.moc"
