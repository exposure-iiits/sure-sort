#include "folder_scanner.h"
#include "image_cache.h"
#include "image_decoder.h"
#include "main_window.h"
#include "photo_viewer.h"
#include "preview_controller.h"
#include "shortcut_manager.h"
#include "raw_fixture.h"
#include <QApplication>
#include <QBuffer>
#include <QColorSpace>
#include <QFile>
#include <QImageReader>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <libraw/libraw.h>
#include <atomic>

using namespace photocull;
namespace {
bool writeFile(const QString& path, QByteArray bytes = "fixture") {
    QFile f(path); return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size();
}
QByteArray jpegWithOrientation(int orientation) {
    QImage image(80,40,QImage::Format_RGB32);
    image.fill(Qt::red);
    for (int y=0;y<40;++y) for (int x=0;x<80;++x)
        image.setPixelColor(x,y,y<20 ? (x<40 ? Qt::red:Qt::blue) : (x<40 ? Qt::green:Qt::yellow));
    QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); image.save(&buffer,"JPEG",100);
    QByteArray exif = QByteArray::fromHex("45786966000049492a0008000000010012010300010000000000000000000000");
    exif[24] = char(orientation);
    QByteArray segment = QByteArray::fromHex("ffe1");
    segment.append(char((exif.size()+2)>>8)); segment.append(char((exif.size()+2)&255)); segment += exif;
    bytes.insert(2,segment); return bytes;
}
DecodedImage fakeImage(const QString& id, QSize target) {
    QImage image(target,QImage::Format_RGB32); image.fill(id.endsWith('1') ? Qt::red : Qt::blue);
    return {image,{6000,4000},{},{}};
}
Catalog catalog(int count) {
    Catalog result;
    for(int i=0;i<count;++i) result.append({QString::number(i),{QString("%1.jpg").arg(i)}});
    return result;
}
}
class PhotoCullTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        qRegisterMetaType<Catalog>(); qRegisterMetaType<DecodedImage>();
        qInfo("LibRaw runtime: %s; Qt: %s",LibRaw::version(),qVersion());
    }
    void filteringPairingAndSort() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        for (const QString name : {"IMG_10.JPG","IMG_2.jPeG","IMG_2.cR3","IMG_1.PNG","notes.txt","IMG_3.NEF","IMG_3.JPG","IMG_3.png"})
            QVERIFY(writeFile(dir.filePath(name)));
        QVERIFY(QDir(dir.path()).mkdir("nested")); QVERIFY(writeFile(dir.filePath("nested/IMG_11.JPG")));
        const auto result=scanFolder(dir.path());
        QVERIFY(result.error.isEmpty()); QCOMPARE(result.photos.size(),6);
        QCOMPARE(result.photos[0].name(),"IMG_1.PNG"); QCOMPARE(result.photos[1].name(),"IMG_2.jPeG");
        QCOMPARE(result.photos[1].sources.size(),2); QCOMPARE(result.photos[1].typeLabel(),"CR3+JPEG");
        QCOMPARE(result.photos[4].name(),"IMG_10.JPG");
        QCOMPARE(result.photos.last().displayPath(),dir.filePath("nested/IMG_11.JPG"));
        const auto second=scanFolder(dir.path());
        for(int i=0;i<result.photos.size();++i) QCOMPARE(result.photos[i].id,second.photos[i].id);
        for (const QString extension : {"ARW","cr2","CR3","NEF","DNG","RAF","RW2","ORF","JPEG","JPG","PNG"})
            QVERIFY(isSupportedExtension(extension));
        QVERIFY(!isSupportedExtension("gif")); QVERIFY(!isSupportedExtension("txt"));
    }
    void emptyInvalidAndCancelledScan() {
        QTemporaryDir dir;
        QVERIFY(scanFolder(dir.path()).photos.isEmpty()); QVERIFY(scanFolder(dir.path()).error.isEmpty());
        QVERIFY(!scanFolder(dir.filePath("missing")).error.isEmpty());
        QVERIFY(writeFile(dir.filePath("file.jpg")));
        QVERIFY(!scanFolder(dir.filePath("file.jpg")).error.isEmpty());
        auto token=std::make_shared<std::atomic_bool>(true);
        QVERIFY(scanFolder(dir.path(),token).photos.isEmpty());
    }
    void byteAccurateLru() {
        // RGB888 width 3 has row padding: width*height*depth is not the byte cost.
        QImage image(3,5,QImage::Format_RGB888); image.fill(Qt::red);
        const qint64 cost=image.sizeInBytes(); QCOMPARE(cost,60);
        ImageCache cache(cost*2); DecodedImage value{image,image.size(),{},{}}, hit;
        CacheKey a{"a",{3,5}}, b{"b",{3,5}}, c{"c",{3,5}};
        cache.insert(a,value); cache.insert(b,value); QCOMPARE(cache.bytes(),cost*2);
        QVERIFY(cache.get(a,hit)); cache.insert(c,value);
        QVERIFY(cache.contains(a)); QVERIFY(!cache.contains(b)); QVERIFY(cache.contains(c));
        cache.insert(a,value); QCOMPARE(cache.bytes(),cost*2);
        cache.setBudget(cost); QCOMPARE(cache.bytes(),cost); QVERIFY(cache.contains(a));
        cache.setBudget(cost-1); QCOMPARE(cache.bytes(),0);
        cache.insert(a,value); QCOMPARE(cache.bytes(),0);
        cache.setBudget(1000); cache.insert({"too-large",{1,1}},value); QCOMPARE(cache.size(),0);
        cache.insert(a,value); cache.clear(); QCOMPARE(cache.bytes(),0); QCOMPARE(cache.size(),0);
    }
    void scaledDecodeAndColor() {
        QTemporaryDir dir; QImage image(600,400,QImage::Format_RGB32); image.fill(QColor(90,120,180));
        image.setColorSpace(QColorSpace::DisplayP3);
        const auto path=dir.filePath(QString::fromUtf8("colour_परीक्षण.png")); QVERIFY(image.save(path));
        auto token=std::make_shared<std::atomic_bool>(false);
        for (const QSize target : {QSize(120,80),QSize(53,200),QSize(1000,1000)}) {
            const auto result=ImageDecoder::decode({{"png",{path}},target,false,true},token);
            QVERIFY2(!result.image.isNull(),qPrintable(result.error));
            QVERIFY(result.image.width()<=target.width()); QVERIFY(result.image.height()<=target.height());
            QCOMPARE(result.sourceSize,QSize(600,400)); QCOMPARE(result.image.colorSpace(),QColorSpace(QColorSpace::SRgb));
        }
        const auto full=ImageDecoder::decode({{"png",{path}},{10,10},true,false},token);
        QCOMPARE(full.image.size(),QSize(600,400));
    }
    void orientation_data() {
        QTest::addColumn<int>("orientation");
        for (int i=1;i<=8;++i) QTest::newRow(qPrintable(QString::number(i))) << i;
    }
    void orientation() {
        QFETCH(int,orientation); QTemporaryDir dir;
        const auto bytes=jpegWithOrientation(orientation);
        QCOMPARE(jpegExifOrientation(bytes).value_or(0),orientation);
        const auto path=dir.filePath("orient.jpg"); QVERIFY(writeFile(path,bytes));
        const auto result=ImageDecoder::decode({{"o",{path}},{400,400},false,true},{});
        QVERIFY2(!result.image.isNull(),qPrintable(result.error));
        const QSize expected=orientation>=5 ? QSize(40,80):QSize(80,40);
        QCOMPARE(result.sourceSize,expected); QCOMPARE(result.image.size(),expected);
        // dcraw flips for EXIF 1..8. Check actual pixels, not just width/height.
        const int flips[]={0,1,3,2,4,6,7,5};
        const QImage plain=QImage::fromData(jpegWithOrientation(1));
        const auto flipped=applyRawFlip(plain,flips[orientation-1]);
        QCOMPARE(result.image.pixelColor(2,2),flipped.pixelColor(2,2));
        const auto scaled=ImageDecoder::decode({{"o",{path}},{16,12},false,true},{});
        QVERIFY(scaled.image.width()<=16); QVERIFY(scaled.image.height()<=12);
    }
    void previewRankingAndMalformedMetadata() {
        QVector<ThumbnailCandidate> candidates={{0,{160,120},true},{1,{6000,4000},true},
            {2,{1920,1280},true},{3,{3840,2560},true},{4,{8000,6000},false}};
        QCOMPARE(rankThumbnails(candidates,{1600,900},false).first(),2);
        QCOMPARE(rankThumbnails(candidates,{9000,6000},false).first(),4);
        QCOMPARE(rankThumbnails(candidates,{100,100},true).first(),4);
        QCOMPARE(rankThumbnails({{0,{1200,800},true,6},{1,{2400,1600},true,6}},{900,1400},false).first(),1);
        QVERIFY(!jpegExifOrientation("broken").has_value());
        auto truncated=jpegWithOrientation(6).left(20); QVERIFY(!jpegExifOrientation(truncated).has_value());
    }
    void syntheticRawPreviewAndFallback() {
        QTemporaryDir dir;
        const auto jpeg=jpegWithOrientation(6);
        const auto path=dir.filePath(QString::fromUtf8("RAW_परीक्षण.dng"));
        QVERIFY(writeFile(path,fixture::rawDng({jpeg})));
        auto result=ImageDecoder::decode({{"dng",{path}},{30,30},false,false},{});
        QVERIFY2(!result.image.isNull(),qPrintable(result.error)); QCOMPARE(result.note,"Embedded RAW preview");
        QCOMPARE(result.sourceSize,QSize(40,80)); QCOMPARE(result.image.size(),QSize(15,30));
        auto full=ImageDecoder::decode({{"dng",{path}},{30,30},true,false},{});
        QCOMPARE(full.image.size(),QSize(40,80));
        // Explicit EXIF orientation 1 must override the RAW file's flip=6.
        QVERIFY(writeFile(path,fixture::rawDng({jpegWithOrientation(1)})));
        full=ImageDecoder::decode({{"dng",{path}},{30,30},true,false},{});
        QCOMPARE(full.image.size(),QSize(80,40));
        // No embedded EXIF: use LibRaw flip exactly once despite synthesized EXIF.
        QImage plain(80,40,QImage::Format_RGB32); plain.fill(Qt::green);
        QByteArray noExif; QBuffer buffer(&noExif); buffer.open(QIODevice::WriteOnly); QVERIFY(plain.save(&buffer,"JPEG"));
        QVERIFY(!jpegExifOrientation(noExif).has_value()); QVERIFY(writeFile(path,fixture::rawDng({noExif})));
        full=ImageDecoder::decode({{"dng",{path}},{30,30},true,false},{});
        QCOMPARE(full.image.size(),QSize(40,80));
        QImage larger(160,80,QImage::Format_RGB32); larger.fill(Qt::blue);
        QByteArray largerJpeg; QBuffer largeBuffer(&largerJpeg); largeBuffer.open(QIODevice::WriteOnly); QVERIFY(larger.save(&largeBuffer,"JPEG"));
        QVERIFY(writeFile(path,fixture::rawDng({noExif,largerJpeg})));
        result=ImageDecoder::decode({{"dng",{path}},{25,50},false,false},{});
        QCOMPARE(result.sourceSize,QSize(40,80)); // Smallest adequate preview.
        result=ImageDecoder::decode({{"dng",{path}},{60,120},false,false},{});
        QCOMPARE(result.sourceSize,QSize(80,160));
        full=ImageDecoder::decode({{"dng",{path}},{10,10},true,false},{});
        QCOMPARE(full.image.size(),QSize(80,160)); // Detail selects the largest.
        QVERIFY(writeFile(path,fixture::rawDng({noExif},true)));
        full=ImageDecoder::decode({{"dng",{path}},{30,30},true,false},{});
        QVERIFY2(!full.image.isNull(),qPrintable(full.error)); QCOMPARE(full.image.size(),QSize(40,80));
        // Detail and prefetch must not develop sensor data when no preview exists.
        QVERIFY(writeFile(path,fixture::rawDng({})));
        full=ImageDecoder::decode({{"dng",{path}},{30,30},true,false},{}); QVERIFY(full.image.isNull());
        result=ImageDecoder::decode({{"dng",{path}},{30,30},false,false},{}); QVERIFY(result.image.isNull());
        result=ImageDecoder::decode({{"dng",{path}},{30,30},false,true},{});
        QVERIFY2(!result.image.isNull(),qPrintable(result.error)); QCOMPARE(result.note,"Half-size RAW fallback");
        QVERIFY(result.image.width()<=30); QVERIFY(result.image.height()<=30);
    }
    void missingCorruptAndCompanionFallback() {
        QTemporaryDir dir;
        for(const QString name:{"broken.jpg","broken.cr3","broken.png"}) {
            const auto path=dir.filePath(name); QVERIFY(writeFile(path,"not a photograph"));
            const auto result=ImageDecoder::decode({{"bad",{path}},{200,100},false,true},{});
            QVERIFY(result.image.isNull()); QVERIFY(!result.error.isEmpty());
        }
        auto missing=ImageDecoder::decode({{"missing",{dir.filePath("gone.jpg")}},{20,20},false,true},{});
        QVERIFY(!missing.error.isEmpty());
        QImage image(20,10,QImage::Format_RGB32); image.fill(Qt::red); QVERIFY(image.save(dir.filePath("valid.png")));
        const auto paired=ImageDecoder::decode({{"p",{dir.filePath("broken.jpg"),dir.filePath("valid.png")}},{20,20},false,true},{});
        QVERIFY(!paired.image.isNull());
    }
    void latestRequestWinsAndBoundedWork() {
        std::atomic_int calls{0}, active{0}, maximum{0};
        PreviewController controller(nullptr,[&](const DecodeRequest& request,const CancelToken&) {
            ++calls; const int count=++active; int prev=maximum.load();
            while(prev<count && !maximum.compare_exchange_weak(prev,count)) {}
            QThread::msleep(35); --active; return fakeImage(request.photo.id,request.target);
        });
        controller.setCatalog(catalog(200)); QSignalSpy spy(&controller,&PreviewController::previewReady);
        controller.select(0,{80,60});
        for(int i=1;i<199;++i) { controller.select(i,{80,60}); QVERIFY(controller.pendingCount()<=1); QVERIFY(controller.runningCount()<=3); }
        for(int i=198;i>=150;--i) controller.select(i,{80,60});
        const auto generation=controller.generation();
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(),1,3000);
        QCOMPARE(spy[0][0].toULongLong(),generation); QCOMPARE(spy[0][1].toString(),"150");
        QVERIFY(calls.load() < 20); QVERIFY(maximum.load()<=3);
        controller.shutdown();
    }
    void usefulNeighborsSurviveNavigation() {
        std::atomic_int secondCalls{0}, secondCancelled{0}, active{0}, maximum{0};
        PreviewController controller(nullptr,[&](const DecodeRequest& request,const CancelToken& cancel) {
            const int count=++active;int prior=maximum.load();while(prior<count&&!maximum.compare_exchange_weak(prior,count)){}
            if(request.photo.id=="2")++secondCalls;
            QThread::msleep(request.photo.id=="0"?5:100);
            if(request.photo.id=="2"&&cancel->load())++secondCancelled;
            --active;return fakeImage(request.photo.id,request.target);
        });
        controller.setCatalog(catalog(10));QSignalSpy ready(&controller,&PreviewController::previewReady);
        controller.select(0,{80,60});QTRY_COMPARE(ready.size(),1);
        QTRY_COMPARE(secondCalls.load(),1);
        controller.select(1,{80,60});QTRY_COMPARE(ready.size(),2);
        controller.select(2,{80,60});QTRY_COMPARE(ready.size(),3);
        QCOMPARE(secondCalls.load(),1);QCOMPARE(secondCancelled.load(),0);QVERIFY(maximum.load()<=3);
        controller.setCacheBudget(80*60*4);controller.setCatalog(catalog(10));controller.select(0,{80,60});
        QTRY_COMPARE(ready.size(),4);QTRY_COMPARE(controller.runningCount(),0);QCOMPARE(controller.pendingCount(),0);
        controller.shutdown();
    }
    void folderSwitchResizeAndDetail() {
        PreviewController controller(nullptr,[](const DecodeRequest& request,const CancelToken&) {
            QThread::msleep(25); return fakeImage(request.photo.id,request.fullResolution ? QSize(600,400) : request.target);
        });
        controller.setCatalog(catalog(20)); QSignalSpy preview(&controller,&PreviewController::previewReady);
        QSignalSpy detail(&controller,&PreviewController::detailReady);
        controller.select(0,{90,60}); controller.setCatalog({{"replacement",{"other.jpg"}}}); controller.select(0,{80,50});
        QTRY_COMPARE(preview.size(),1); QCOMPARE(preview[0][1].toString(),"replacement");
        const auto bytes=controller.cacheBytes();
        controller.requestDetail(true); QTRY_COMPARE(detail.size(),1); QCOMPARE(controller.cacheBytes(),bytes);
        controller.select(0,{40,25}); QTRY_COMPARE(preview.size(),2);
        QCOMPARE(qvariant_cast<DecodedImage>(preview[1][2]).image.size(),QSize(40,25));
        controller.requestDetail(true); controller.requestDetail(false); QTest::qWait(80); QCOMPARE(detail.size(),1);
        controller.shutdown();
    }
    void asynchronousScanSwitchAndShutdown() {
        QTemporaryDir a,b;
        for(int i=0;i<250;++i) QVERIFY(writeFile(a.filePath(QString::number(i)+".jpg")));
        QVERIFY(writeFile(b.filePath("only.png")));
        FolderScanner scanner; QSignalSpy spy(&scanner,&FolderScanner::finished);
        scanner.start(a.path(),1); scanner.start(b.path(),2);
        QTRY_COMPARE(spy.size(),1); QCOMPARE(spy[0][0].toULongLong(),2);
        QCOMPARE(qvariant_cast<Catalog>(spy[0][1]).size(),1);
        scanner.start(a.path(),3); scanner.shutdown();
        { PreviewController controller(nullptr,[](const DecodeRequest& r,const CancelToken&) {
            QThread::msleep(20); return fakeImage(r.photo.id,r.target);
          }); controller.setCatalog(catalog(3)); controller.select(0,{60,40}); }
        QCoreApplication::processEvents(); // Queued callbacks to destroyed receivers must be removed.
    }
    void shortcutsAndHoldChords() {
        QWidget window; ShortcutManager manager(&window); QSignalSpy spy(&manager,&ShortcutManager::triggered);
        manager.bindChord("exposureUp",Qt::Key_E,QKeySequence(Qt::Key_Right));
        window.show(); window.activateWindow(); window.setFocus();
        QTest::keyClick(&window,Qt::Key_Right); QCOMPARE(spy.last()[0].toString(),"next");
        const auto count=spy.size(); QTest::keyClick(&window,Qt::Key_1); QCOMPARE(spy.size(),count);
        QTest::keyPress(&window,Qt::Key_E); QTest::keyClick(&window,Qt::Key_Right);
        QCOMPARE(spy.last()[0].toString(),"exposureUp");
        QTest::keyRelease(&window,Qt::Key_E); QTest::keyClick(&window,Qt::Key_Right);
        QCOMPARE(spy.last()[0].toString(),"next");
        QTest::keyPress(&window,Qt::Key_E); QEvent deactivate(QEvent::WindowDeactivate); QApplication::sendEvent(&window,&deactivate);
        QTest::keyClick(&window,Qt::Key_Right); QCOMPARE(spy.last()[0].toString(),"next");
    }
    void viewerAndWindowSmoke() {
        QTemporaryDir dir;
        QImage image(900,600,QImage::Format_RGB32); image.fill(QColor("#334859")); QVERIFY(image.save(dir.filePath("IMG_1.jpg")));
        MainWindow window(4*1024*1024,nullptr,dir.filePath("session.sqlite")); window.resize(960,660); window.show(); window.openFolder(dir.path());
        auto* viewer=window.findChild<PhotoViewer*>(); QVERIFY(viewer);
        QTest::qWait(400); viewer->actualSize(); QVERIFY(!viewer->isFit());
        QCOMPARE(qRound(viewer->zoomPercent()),100); viewer->zoomBy(1.25); QVERIFY(viewer->zoomPercent()>100);
        viewer->fit(); QVERIFY(viewer->isFit()); window.resize(1040,700); QTest::qWait(250);
        if (qEnvironmentVariableIsSet("PHOTOCULL_TEST_SCREENSHOT"))
            QVERIFY(window.grab().save(qEnvironmentVariable("PHOTOCULL_TEST_SCREENSHOT")));
        window.openFolder(dir.filePath("missing")); QTest::qWait(50); window.shutdown();
    }
};
QTEST_MAIN(PhotoCullTest)
#include "test_photocull.moc"
