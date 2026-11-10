#include "sorting_controller.h"
#include "output_dialog.h"
#include "main_window.h"
#include "exif_metadata.h"
#include "raw_fixture.h"
#include <QBuffer>
#include <QCheckBox>
#include <QColorSpace>
#include <QComboBox>
#include <QElapsedTimer>
#include <QFile>
#include <QImageReader>
#include <QImageWriter>
#include <QPushButton>
#include <QAction>
#include <QSignalSpy>
#include <QSlider>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QtEndian>
#include <atomic>
#include <libraw/libraw.h>
using namespace photocull;
namespace {
bool write(const QString& p,const QByteArray& bytes){QFile f(p);return f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size();}
QByteArray read(const QString& p){QFile f(p);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();}
QImage pattern(QSize size={601,403}){QImage image(size,QImage::Format_RGB32);for(int y=0;y<size.height();++y)for(int x=0;x<size.width();++x)image.setPixel(x,y,qRgb((x*17+y*11)%256,(y*13+x*3)%256,(x+y*7)%256));image.setColorSpace(QColorSpace::SRgb);return image;}
QByteArray encode(const QImage& image,const char* format="JPEG"){QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);QImageWriter writer(&buffer,format);writer.setQuality(100);writer.write(image);return bytes;}
ProcessingTask task(const QString& path,const QString& directory){ProcessingTask t;t.id=newId();t.photo={t.id,{path}};t.directory=directory;t.categoryId="club";t.categoryName="Club";t.temporaryPath=directory+"/.photocull-"+t.id+".part";return t;}
DestinationConfig config(const QString& root){return {root,{{"club","Club","1","copy"},{"other","Other","2","copy"}}};}
QByteArray tiff(bool big=false){
    auto s=[big](quint16 n){auto bytes=fixture::le16(n);if(big)std::reverse(bytes.begin(),bytes.end());return bytes;};
    auto l=[big](quint32 n){auto bytes=fixture::le32(n);if(big)std::reverse(bytes.begin(),bytes.end());return bytes;};
    // Make, orientation, EXIF pointer, GPS pointer, thumbnail pointer. EXIF has
    // a date and exposure; MakerNote deliberately contains a stale pointer.
    QByteArray b=(big?QByteArray("MM"):QByteArray("II"))+s(42)+l(8);
    b+=s(4);b+=s(271)+s(2)+l(5)+l(140);b+=s(274)+s(3)+l(1)+s(6)+s(0);
    b+=s(34665)+s(4)+l(1)+l(62);b+=s(34853)+s(4)+l(1)+l(104);b+=l(130);
    b+=s(3);b+=s(33434)+s(5)+l(1)+l(146);b+=s(36867)+s(2)+l(20)+l(154);b+=s(37500)+s(7)+l(8)+l(174);b+=l(0);
    b+=s(1);b+=s(1)+s(2)+l(2)+QByteArray("N\0\0\0",4);b+=l(0);b.resize(140);
    b+=QByteArray("SONY\0",5);b.append(char(0));b+=l(1)+l(125);b+=QByteArray("2025:01:02 03:04:05\0",20);b+=QByteArray("BADNOTE!",8);return b;
}
QByteArray withExif(QByteArray jpeg,const QByteArray& data){const auto payload=QByteArray("Exif\0\0",6)+data;QByteArray app=QByteArray::fromHex("ffe1");const int length=payload.size()+2;app.append(char(length>>8));app.append(char(length));app+=payload;jpeg.insert(2,app);return jpeg;}
QByteArray jpegTiff(const QByteArray& bytes){const int start=bytes.indexOf(QByteArray("Exif\0\0",6));if(start<0)return {};const int length=qFromBigEndian<quint16>(bytes.constData()+start-2);return bytes.mid(start+6,length-8);}
QByteArray entry(const QByteArray& tiff,int offset,int tag){if(offset<0||offset+2>tiff.size())return {};const int count=qFromLittleEndian<quint16>(tiff.constData()+offset);for(int i=0;i<count;++i){const int pos=offset+2+i*12;if(pos+12>tiff.size())return {};if(qFromLittleEndian<quint16>(tiff.constData()+pos)==tag)return tiff.mid(pos,12);}return {};}
quint32 number(const QByteArray& tiff,int offset,int tag){const auto e=entry(tiff,offset,tag);return e.size()==12?qFromLittleEndian<quint32>(e.constData()+8):0;}
}
class OutputTest:public QObject {
    Q_OBJECT
private slots:
    void settingsValidation(){
        OutputSettings s;QVERIFY(readOutput({},s));QCOMPARE(s.jpegQuality,95);QCOMPARE(s.resolution,100);QVERIFY(s.preserveMetadata);
        for(int q:{79,101}){s.jpegQuality=q;QVERIFY(!validateOutput(s).isEmpty());}s={};s.resolution=60;QVERIFY(!validateOutput(s).isEmpty());
        auto j=outputJson({80,50,false});QVERIFY(readOutput(j,s));QVERIFY(s==OutputSettings({80,50,false}));j["quality"]=80.2;QVERIFY(!readOutput(j,s));j["quality"]=1e100;QVERIFY(!readOutput(j,s));j=outputJson({});j.remove("metadata");QVERIFY(!readOutput(j,s));
    }
    void formatsAndResolution_data(){QTest::addColumn<QString>("extension");QTest::addColumn<int>("percent");QTest::addColumn<int>("quality");for(const QString ext:{"jpg","png","dng"})for(int percent:{100,75,50})for(int quality:{80,100})QTest::newRow(qPrintable(ext+QString::number(percent)+"-"+QString::number(quality)))<<ext<<percent<<quality;}
    void formatsAndResolution(){
        QFETCH(QString,extension);QFETCH(int,percent);QFETCH(int,quality);QTemporaryDir dir;const auto source=dir.filePath("photo."+extension);
        const auto original=extension=="dng"?fixture::rawDng({encode(pattern({80,40}))}):encode(pattern(),extension=="png"?"PNG":"JPEG");QVERIFY(write(source,original));
        auto t=task(source,dir.filePath("out"));t.output={quality,percent,true};const auto prepared=ProcessingPipeline::prepare(t,{});QCOMPARE(prepared.status,"prepared");
        const auto result=ProcessingPipeline::publish(prepared,{});QCOMPARE(result.status,"completed");QImageReader reader(result.outputPath);reader.setAutoTransform(true);const auto image=reader.read();QVERIFY2(!image.isNull(),qPrintable(reader.errorString()));
        QCOMPARE(image.size(),outputSize(extension=="dng"?QSize(384,512):QSize(601,403),t.output));QCOMPARE(QImageReader::imageFormat(result.outputPath),extension=="png"?QByteArray("png"):QByteArray("jpeg"));
        QVERIFY(image.colorSpace()==QColorSpace(QColorSpace::SRgb));QCOMPARE(read(source),original);
        if(extension=="dng")QVERIFY(read(result.outputPath).contains("Test DNG"));
    }
    void qualityActuallyChangesEncoding(){QTemporaryDir dir;const auto source=dir.filePath("photo.jpg");QVERIFY(write(source,encode(pattern())));QVector<QByteArray> results;
        for(int quality:{80,95,100}){auto t=task(source,dir.filePath(QString::number(quality)));t.output.jpegQuality=quality;t.edits.brightness=1;auto p=ProcessingPipeline::prepare(t,{});QCOMPARE(p.status,"prepared");results.append(read(p.temporaryPath));}
        QVERIFY(results[0].size()<results[1].size());QVERIFY(results[1].size()<results[2].size());
    }
    void sensorOutputUsesSrgbTransfer(){
        QTemporaryDir dir;const auto source=dir.filePath("photo.dng");QVERIFY(write(source,fixture::rawDng({encode(pattern({80,40}))})));
        auto reference=[&](bool srgb){auto raw=std::make_unique<LibRaw>();auto& p=raw->imgdata.params;p.output_color=1;p.output_bps=8;p.use_camera_wb=1;p.user_qual=3;p.auto_bright_thr=.001f;if(srgb){p.gamm[0]=1.0/2.4;p.gamm[1]=12.92;}
#ifdef Q_OS_WIN
            int rc=raw->open_file(reinterpret_cast<const wchar_t*>(source.utf16()));
#else
            int rc=raw->open_file(QFile::encodeName(source).constData());
#endif
            if(rc||raw->unpack()||raw->dcraw_process())return QImage();
            auto* memory=raw->dcraw_make_mem_image(&rc);if(!memory||rc)return QImage();QImage image(memory->data,memory->width,memory->height,memory->width*memory->colors,QImage::Format_RGB888);image=image.copy();LibRaw::dcraw_clear_mem(memory);return image;
        };
        const auto srgb=reference(true),bt709=reference(false);QVERIFY(!srgb.isNull());QVERIFY(!bt709.isNull());auto t=task(source,dir.filePath("out"));t.output.jpegQuality=100;const auto p=ProcessingPipeline::prepare(t,{});QCOMPARE(p.status,"prepared");const QImage actual(p.temporaryPath);QCOMPARE(actual.size(),srgb.size());
        double srgbError=0,btError=0;int samples=0;for(int y=20;y<actual.height()-20;y+=20)for(int x=20;x<actual.width()-20;x+=20){const int value=actual.pixelColor(x,y).red();srgbError+=qAbs(value-srgb.pixelColor(x,y).red());btError+=qAbs(value-bt709.pixelColor(x,y).red());++samples;}
        QVERIFY(srgbError/samples<2);QVERIFY(srgbError<btError);
    }
    void exifSanitized_data(){QTest::addColumn<bool>("big");QTest::newRow("little")<<false;QTest::newRow("big")<<true;}
    void exifSanitized(){
        QFETCH(bool,big);QTemporaryDir dir;const auto source=dir.filePath("photo.jpg");const auto original=withExif(encode(pattern({600,400})),tiff(big));QVERIFY(write(source,original));
        const auto fields=ExifMetadata::read(source).displayFields();QMap<QString,QString> info;for(const auto& field:fields)info.insert(field.first,field.second);
        QCOMPARE(info.value("Camera"),QString("SONY"));QCOMPARE(info.value("Shutter"),QString("1/125 s"));QVERIFY(!info.value("Date taken").isEmpty());QCOMPARE(info.value("Time taken"),QString("03:04:05"));
        auto t=task(source,dir.filePath("out"));t.output.resolution=50;t.edits.rotation=1;auto p=ProcessingPipeline::prepare(t,{});QCOMPARE(p.status,"prepared");const auto bytes=read(p.temporaryPath),meta=jpegTiff(bytes);QVERIFY(meta.startsWith("II"));
        QVERIFY(meta.contains("SONY"));QVERIFY(meta.contains("2025:01:02 03:04:05"));QVERIFY(!meta.contains("BADNOTE!"));
        QCOMPARE(number(meta,8,274),1u);QCOMPARE(number(meta,8,256),300u);QCOMPARE(number(meta,8,257),200u);
        const int exif=int(number(meta,8,34665));QCOMPARE(number(meta,exif,40962),300u);QCOMPARE(number(meta,exif,40963),200u);QCOMPARE(number(meta,exif,40961),1u);QVERIFY(entry(meta,exif,37500).isEmpty());QVERIFY(number(meta,8,34853)>0);
        const int exposure=int(number(meta,exif,33434));QVERIFY(exposure>0&&exposure+8<=meta.size());QCOMPARE(qFromLittleEndian<quint32>(meta.constData()+exposure),1u);QCOMPARE(qFromLittleEndian<quint32>(meta.constData()+exposure+4),125u);
        QCOMPARE(entry(meta,int(number(meta,8,34853)),1).mid(8,2),QByteArray("N\0",2));
        const int count=qFromLittleEndian<quint16>(meta.constData()+8);QCOMPARE(qFromLittleEndian<quint32>(meta.constData()+10+count*12),0u);
        QImageReader reader(p.temporaryPath);reader.setAutoTransform(true);QCOMPARE(reader.read().size(),QSize(300,200));QCOMPARE(read(source),original);
        t=task(source,dir.filePath("stripped"));t.output.preserveMetadata=false;p=ProcessingPipeline::prepare(t,{});QCOMPARE(p.status,"prepared");QVERIFY(!read(p.temporaryPath).contains(QByteArray("Exif\0\0",6)));QCOMPARE(QImage(p.temporaryPath).size(),QSize(400,600));
    }
    void pngMetadataAndAlpha(){
        QTemporaryDir dir;const auto jpeg=dir.filePath("source.jpg");QVERIFY(write(jpeg,withExif(encode(pattern()),tiff())));auto meta=ExifMetadata::read(jpeg);
        QImage input(600,400,QImage::Format_ARGB32);input.fill(qRgba(40,80,100,120));input.setColorSpace(QColorSpace::SRgb);input.setText("Comment","original comment");
        const auto source=dir.filePath("photo.png");{QFile output(source);QVERIFY(output.open(QIODevice::WriteOnly));MetadataDevice device(output,"png",meta.serialize(input.size()));QImageWriter writer(&device,"png");QVERIFY(writer.write(input));}
        auto t=task(source,dir.filePath("out"));t.output.resolution=50;auto p=ProcessingPipeline::prepare(t,{});QCOMPARE(p.status,"prepared");QVERIFY(read(p.temporaryPath).contains("eXIf"));QVERIFY(read(p.temporaryPath).contains("SONY"));
        const auto roundtrip=ExifMetadata::read(p.temporaryPath).serialize({300,200});QVERIFY(roundtrip.contains("SONY"));QCOMPARE(QImage(p.temporaryPath).size(),QSize(300,200));QCOMPARE(QImage(p.temporaryPath).pixelColor(10,10).alpha(),120);
        t=task(source,dir.filePath("stripped"));t.output.preserveMetadata=false;p=ProcessingPipeline::prepare(t,{});QCOMPARE(p.status,"prepared");QVERIFY(!read(p.temporaryPath).contains("eXIf"));QVERIFY(QImage(p.temporaryPath).text("Comment").isEmpty());
    }
    void originalProfileIsConverted(){QTemporaryDir dir;const auto source=dir.filePath("wide.png");QImage input(60,40,QImage::Format_ARGB32);input.fill(qRgba(160,80,50,180));input.setColorSpace(QColorSpace::DisplayP3);QVERIFY(input.save(source));auto t=task(source,dir.filePath("out"));t.output.resolution=50;const auto p=ProcessingPipeline::prepare(t,{});QCOMPARE(p.status,"prepared");const auto expected=input.convertedToColorSpace(QColorSpace::SRgb);const QImage actual(p.temporaryPath);QCOMPARE(actual.colorSpace(),QColorSpace(QColorSpace::SRgb));QCOMPARE(actual.pixelColor(10,10),expected.pixelColor(10,10));}
    void malformedExifIsBounded(){QTemporaryDir dir;for(const auto& data:{QByteArray("II*\0\xff\xff\xff\xff",8),QByteArray("MM\0*\0\0\0\x08\xff\xff",10),tiff().left(40)}){const auto source=dir.filePath("bad.jpg");QVERIFY(write(source,withExif(encode(pattern({40,30})),data)));auto t=task(source,dir.filePath(newId()));t.edits.brightness=2;const auto p=ProcessingPipeline::prepare(t,{});QCOMPARE(p.status,"prepared");QVERIFY(!QImage(p.temporaryPath).isNull());}}
    void editsWatermarkThenResize(){
        QTemporaryDir dir;const auto source=dir.filePath("photo.png");auto input=pattern({600,400});QVERIFY(input.save(source));auto t=task(source,dir.filePath("out"));t.edits.exposure=5;t.edits.shadows=20;t.edits.saturation=-10;t.edits.rotation=1;t.edits.crop={.1,.2,.7,.6};t.output.resolution=50;
        QImage logo(80,30,QImage::Format_ARGB32);logo.fill(qRgba(240,20,30,160));QString error;t.watermark.asset=decodeWatermarkPng(encode(logo,"PNG"),"logo",error);QVERIFY(t.watermark.asset);t.watermark.assetId=t.watermark.asset->id;t.watermark.settings.enabled=true;t.watermark.settings.sizePercent=25;t.watermark.settings.opacityPercent=70;
        auto expected=renderEdits(input,t.edits,{});QVERIFY(compositeWatermark(expected,t.watermark,error));expected=expected.scaled(outputSize(expected.size(),t.output),Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
        const auto p=ProcessingPipeline::prepare(t,{});QCOMPARE(p.status,"prepared");QCOMPARE(QImage(p.temporaryPath).convertToFormat(QImage::Format_ARGB32),expected.convertToFormat(QImage::Format_ARGB32));
    }
    void persistenceRetryUndoAndRefresh(){
        QTemporaryDir dir;const auto database=dir.filePath("session.sqlite"),source=dir.filePath("photo.jpg");const Photo photo{"photo",{source}};
        {SortingController c;QVERIFY(c.initialize(database));QVERIFY(c.configure(config(dir.filePath("out"))));QVERIFY(c.setOutput({80,50,true}));QVERIFY(!c.assign(photo,"club").isEmpty());QTRY_COMPARE(c.assignment(photo.id).status,QString("failed"));QVERIFY(c.setOutput({100,75,false}));QVERIFY(write(source,encode(pattern({600,400})))) ;c.retryFailed();QTRY_COMPARE(c.assignment(photo.id).status,QString("completed"));QVERIFY(c.assignment(photo.id).output==OutputSettings({80,50,true}));QCOMPARE(QImage(c.assignment(photo.id).outputPath).size(),QSize(300,200));
            const auto old=c.assignment(photo.id).outputPath;const auto id=c.assign(photo,"club");QTRY_COMPARE(c.assignment(photo.id).status,QString("completed"));QVERIFY(c.assignment(photo.id).id==id);QCOMPARE(c.assignment(photo.id).outputPath,old);QCOMPARE(QImage(old).size(),QSize(450,300));
            c.assign(photo,"other");QTRY_COMPARE(c.assignment(photo.id).status,QString("completed"));QVERIFY(c.setOutput({90,100,true}));c.undo();QTRY_COMPARE(c.assignment(photo.id).status,QString("completed"));QVERIFY(c.assignment(photo.id).output==OutputSettings({100,75,false}));c.shutdown();}
        {SortingController c;QVERIFY(c.initialize(database));QVERIFY(c.output()==OutputSettings({90,100,true}));QVERIFY(c.assignment(photo.id).output==OutputSettings({100,75,false}));QCOMPARE(c.processingStatus(),QString("Completed: 1 | Processing: 0 | Pending: 0 | Failed: 0"));}
    }
    void interruptedSnapshotRecovery(){
        QTemporaryDir dir;const auto source=dir.filePath("photo.jpg"),database=dir.filePath("session.sqlite");QVERIFY(write(source,encode(pattern({600,400}))));auto t=task(source,dir.filePath("out/Club"));t.photo.id="photo";t.output={80,50,false};t.edits.rotation=1;QString error;
        {SessionStore store;QVERIFY(store.open(database,error));QVERIFY(store.saveConfig(config(dir.filePath("out")),error));QVERIFY(store.saveOutput({100,75,true},error));QVERIFY(store.changeAssignment(t.photo,t.id,&t,{},false,{},false,error));t=ProcessingPipeline::prepare(t,{});QCOMPARE(t.status,"prepared");QVERIFY(store.saveTask(t,error));}
        {SortingController c;QVERIFY(c.initialize(database));QTRY_COMPARE(c.assignment("photo").status,QString("completed"));QVERIFY(c.assignment("photo").output==OutputSettings({80,50,false}));QCOMPARE(QImage(c.assignment("photo").outputPath).size(),QSize(200,300));QVERIFY(!read(c.assignment("photo").outputPath).contains(QByteArray("Exif\0\0",6)));}
    }
    void largeBatchKeepsEventsMoving(){
        QTemporaryDir dir;constexpr int count=510;QVector<Photo> photos;const auto jpeg=encode(pattern({120,80})),png=encode(pattern({120,80}),"PNG"),raw=fixture::rawDng({encode(pattern({80,40}))});
        for(int i=0;i<count;++i){const QString ext=i%51==0?"dng":i%2?"jpg":"png";const auto path=dir.filePath(QString("photo%1.").arg(i)+ext);QVERIFY(write(path,ext=="dng"?raw:ext=="jpg"?jpeg:png));photos.append({QString::number(i),{path}});}
        std::atomic_int active{0},peak{0},prepared{0};SortingController c(nullptr,[&](ProcessingTask t,const CancelToken& cancel){const int running=++active;peak.store(qMax(peak.load(),running));auto result=ProcessingPipeline::prepare(std::move(t),cancel);--active;++prepared;return result;});
        QVERIFY(c.initialize(dir.filePath("session.sqlite")));QVERIFY(c.configure(config(dir.filePath("out"))));QVERIFY(c.setOutput({95,50,true}));QSignalSpy advanced(&c,&SortingController::advanceRequested);
        int next=0,ticks=0; qint64 maxGap=0,last=0;QElapsedTimer elapsed;elapsed.start();QTimer heart;heart.setInterval(5);connect(&heart,&QTimer::timeout,this,[&]{++ticks;const auto now=elapsed.elapsed();maxGap=qMax(maxGap,now-last);last=now;});heart.start();
        QTimer assign;assign.setInterval(0);connect(&assign,&QTimer::timeout,this,[&]{if(next==count){assign.stop();return;}c.assign(photos[next++],"club");});assign.start();
        QTRY_COMPARE_WITH_TIMEOUT(next,count,60000);QTRY_COMPARE_WITH_TIMEOUT(c.remaining(),0,120000);QCOMPARE(c.failed(),0);QCOMPARE(prepared.load(),count);QCOMPARE(peak.load(),1);QCOMPARE(advanced.size(),count);QVERIFY(ticks>10);QVERIFY(maxGap<2000);
        QCOMPARE(c.processingStatus(),QString("Completed: 510 | Processing: 0 | Pending: 0 | Failed: 0"));
        QCOMPARE(QDir(dir.filePath("out/Club")).entryList(QDir::Files|QDir::Hidden|QDir::NoDotAndDotDot).size(),count);
        for(int i=0;i<count;++i){const auto original=read(photos[i].sources[0]);QCOMPARE(original,i%51==0?raw:i%2?jpeg:png);QVERIFY(!QImage(c.assignment(photos[i].id).outputPath).isNull());}
        qInfo("510 mixed files (10 sensor RAW): %lld ms; UI timer ticks: %d; max event gap: %lld ms; peak processing workers: %d",elapsed.elapsed(),ticks,maxGap,peak.load());
    }
    void outputSettingsInWindow(){
        QTemporaryDir dir;MainWindow window(8*1024*1024,nullptr,dir.filePath("session.sqlite"));window.show();auto* button=window.findChild<QAction*>("outputSettings");QVERIFY(button);button->trigger();auto* dialog=window.findChild<OutputDialog*>();QVERIFY(dialog);QSignalSpy changes(dialog,&OutputDialog::changed);
        dialog->findChild<QSlider*>("jpegQuality")->setValue(80);dialog->findChild<QComboBox*>("outputResolution")->setCurrentIndex(2);dialog->findChild<QCheckBox*>("preserveMetadata")->setChecked(false);QCOMPARE(changes.size(),3);
        if(qEnvironmentVariableIsSet("PHOTOCULL_PHASE5_SCREENSHOT"))QVERIFY(dialog->grab().save(qEnvironmentVariable("PHOTOCULL_PHASE5_SCREENSHOT")));
        dialog->close();window.shutdown();
    }
};
QTEST_MAIN(OutputTest)
#include "test_output.moc"
