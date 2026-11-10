#include "main_window.h"
#include "photo_popup.h"
#include "photo_information.h"
#include "filmstrip.h"
#include "edit_panel.h"
#include "settings_dialog.h"
#include "shortcut_manager.h"
#include "session_store.h"
#include "exif_metadata.h"
#include "raw_fixture.h"
#include <QApplication>
#include <QDoubleSpinBox>
#include <QImageWriter>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QCheckBox>
#include <QListWidget>
#include <QSlider>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QDir>
#include <QFile>
#include <QComboBox>
#include <QWheelEvent>
using namespace photocull;
namespace {
void capture(const QString& name,QWidget* widget){const auto path=qEnvironmentVariable("SURESORT_UI_CAPTURE");if(!path.isEmpty()){QTest::qWait(150);QVERIFY(widget->grab().save(QDir(path).filePath(name+".png")));}}
QImage image(QColor color=QColor(80,130,170)){QImage image(600,400,QImage::Format_RGB32);image.fill(color);return image;}
QString field(const PhotoInformation& fields,const QString& name){for(const auto& item:fields)if(item.first==name)return item.second;return {};}
}
class ViewerToolsTest:public QObject {
    Q_OBJECT
private slots:
    void completedThumbnailsTrackCopyMoveUndoAndRestart(){
        QTemporaryDir dir;const auto input=dir.filePath("input"),output=dir.filePath("out"),database=dir.filePath("session.sqlite");QVERIFY(QDir().mkpath(input));
        const auto source=input+"/photo_01.jpg";QVERIFY(image().save(source));QVERIFY(image().save(input+"/photo_02.jpg"));
        QString error;{SessionStore store;QVERIFY(store.open(database,error));QVERIFY(store.saveConfig({output,{{"a","A","1","copy"},{"b","B","2","copy"},{"m","Moved","3","move"}}},error));}
        auto green=[](Filmstrip* strip){const auto pixels=strip->viewport()->grab().toImage();const auto ratio=pixels.devicePixelRatio();const auto color=pixels.pixelColor(qRound(48*ratio),qRound(8*ratio));return color.green()>color.red()+25&&color.green()>color.blue()+12;};
        {
            MainWindow window(12*1024*1024,nullptr,database);window.show();window.openFolder(input);auto* viewer=window.findChild<PhotoViewer*>();auto* strip=window.findChild<Filmstrip*>();QTRY_VERIFY(viewer->hasPhoto());QVERIFY(!green(strip));
            auto assign=[&](const QString& id){for(auto* button:window.findChildren<QPushButton*>())if(button->property("destinationChip").toBool()&&button->property("destinationId").toString()==id){button->click();return;}QFAIL("Missing destination");};
            assign("a");QVERIFY(!green(strip));QTRY_VERIFY(QFileInfo::exists(output+"/A/photo_01.jpg"));QTRY_VERIFY(green(strip));
            // Keep the completed photo selected too: the green border remains visible beside selection styling.
            QTest::mouseClick(strip->viewport(),Qt::LeftButton,Qt::NoModifier,QPoint(40,40));QCOMPARE(strip->currentIndex(),0);QVERIFY(green(strip));assign("b");QTRY_VERIFY(QFileInfo::exists(output+"/B/photo_01.jpg"));QTRY_VERIFY(green(strip));
            window.findChild<QPushButton*>("undoAction")->click();QTRY_VERIFY(!QFileInfo::exists(output+"/B/photo_01.jpg"));QVERIFY(green(strip));capture("completed-thumbnails",&window);window.shutdown();
        }
        {
            MainWindow window(12*1024*1024,nullptr,database);window.show();window.openFolder(input);auto* viewer=window.findChild<PhotoViewer*>();auto* strip=window.findChild<Filmstrip*>();QTRY_VERIFY(viewer->hasPhoto());QTRY_VERIFY(green(strip));
            window.activateWindow();viewer->setFocus();QTest::keyClick(viewer,Qt::Key_Backspace);QTRY_VERIFY(!QFileInfo::exists(output+"/A/photo_01.jpg"));QTRY_VERIFY(!green(strip));
            for(auto* button:window.findChildren<QPushButton*>())if(button->property("destinationChip").toBool()&&button->property("destinationId").toString()=="m"){button->click();break;}
            QTRY_VERIFY(!QFileInfo::exists(source));QTRY_VERIFY(green(strip));window.findChild<QPushButton*>("undoAction")->click();QTRY_VERIFY(QFileInfo::exists(source));QTRY_VERIFY(!green(strip));window.shutdown();
        }
    }
    void metadataAndMissingExif(){
        QTemporaryDir dir;const auto source=dir.filePath("metadata.jpg");
        ExifMetadata metadata;metadata.camera("Canon","EOS Test",400,1.0f/125,2.8f,50,"50mm Test");
        {QFile file(source);QVERIFY(file.open(QIODevice::WriteOnly));MetadataDevice device(file,"jpeg",metadata.serialize({600,400}));QImageWriter writer(&device,"jpeg");QVERIFY(writer.write(image()));}
        auto fields=readPhotoInformation({"photo",{source}},{},{});
        QCOMPARE(field(fields,"Camera"),QString("Canon EOS Test"));QCOMPARE(field(fields,"ISO"),QString("400"));QCOMPARE(field(fields,"Aperture"),QString("f/2.8"));QCOMPARE(field(fields,"Shutter"),QString("1/125 s"));QCOMPARE(field(fields,"Focal length"),QString("50 mm"));QVERIFY(field(fields,"Dimensions").startsWith("600 × 400"));QVERIFY(field(fields,"File size").contains("bytes"));
        const auto plain=dir.filePath("plain.png");QVERIFY(image().save(plain));fields=readPhotoInformation({"plain",{plain}},{},{});QVERIFY(field(fields,"Camera").isEmpty());QVERIFY(!field(fields,"Camera metadata").isEmpty());
        const auto raw=dir.filePath("test.dng");{QFile file(raw);QVERIFY(file.open(QIODevice::WriteOnly));file.write(fixture::rawDng({}));}
        fields=readPhotoInformation({"raw",{raw}},{},{});QVERIFY(field(fields,"Camera").contains("Test DNG"));QVERIFY(field(fields,"Dimensions").startsWith("384 × 512"));
        const auto cancel=std::make_shared<std::atomic_bool>(true);QVERIFY(readPhotoInformation({"raw",{raw}},{},cancel).isEmpty());
    }
    void histogramMatchesPixelsAndEdits(){
        QImage input(3,1,QImage::Format_ARGB32);input.setPixel(0,0,qRgba(255,0,0,255));input.setPixel(1,0,qRgba(0,255,0,255));input.setPixel(2,0,qRgba(0,0,255,0));
        const auto data=photoHistogram(input);QCOMPARE(data.pixels,2u);QCOMPARE(data.channels[0][255],1u);QCOMPARE(data.channels[1][255],1u);QCOMPARE(data.channels[2][0],2u);QCOMPARE(data.channels[3][54],1u);QCOMPARE(data.channels[3][182],1u);
        PhotoViewer viewer;viewer.setPreview({image(QColor(50,50,50)),{600,400},{},{}});const auto before=viewer.histogramImage();PhotoEdits edits;edits.exposure=10;viewer.setEdits(edits);QTRY_VERIFY(viewer.histogramImage().pixelColor(10,10).red()>before.pixelColor(10,10).red());
    }
    void typedEditsAndRotationMagnet(){
        EditPanel panel;panel.show();QSignalSpy changed(&panel,&EditPanel::changed);
        auto* exposure=panel.findChild<QDoubleSpinBox*>("editExposureValue");QVERIFY(exposure);
        exposure->setFocus();exposure->selectAll();QTest::keyClicks(exposure,"1.3");QTest::keyClick(exposure,Qt::Key_Return);QCOMPARE(changed.last()[0].value<PhotoEdits>().exposure,13);
        auto* brightness=panel.findChild<QDoubleSpinBox*>("editBrightnessValue");brightness->setFocus();brightness->selectAll();QTest::keyClicks(brightness,"-21");QTest::keyClick(brightness,Qt::Key_Return);QCOMPARE(changed.last()[0].value<PhotoEdits>().brightness,-21);
        auto* rotation=panel.findChild<QSlider*>("editRotation");rotation->setSliderDown(true);rotation->setValue(88);QCOMPARE(rotation->value(),90);rotation->setValue(95);QCOMPARE(rotation->value(),95);rotation->setValue(-2);QCOMPARE(rotation->value(),0);rotation->setValue(-92);QCOMPARE(rotation->value(),-90);rotation->setSliderDown(false);
        auto* degrees=panel.findChild<QDoubleSpinBox*>("rotationValue");degrees->setFocus();degrees->selectAll();QTest::keyClicks(degrees,"88");QTest::keyClick(degrees,Qt::Key_Return);QCOMPARE(rotation->value(),88);
        panel.setShortcutBindings(ShortcutManager::defaultEditBindings());QVERIFY(!panel.findChildren<QLabel*>("editShortcutHint").first()->text().isEmpty());capture("editable-panel",&panel);
    }
    void crowdedFooterPopupsFilmstripAndFullscreen(){
        QTemporaryDir dir;const auto input=dir.filePath("input"),database=dir.filePath("session.sqlite");QVERIFY(QDir().mkpath(input));
        for(int i=0;i<24;++i)QVERIFY(image(QColor::fromHsv(i*15,100,180)).save(input+QString("/photo_%1_with_a_long_file_name.jpg").arg(i,2,10,QChar('0'))));
        DestinationConfig config;config.outputRoot=dir.filePath("out");for(int i=0;i<30;++i)config.folders.append({QString::number(i),QString("Folder %1 with a very long descriptive name").arg(i),i==0?"1":"","copy"});
        QString error;{SessionStore store;QVERIFY(store.open(database,error));QVERIFY(store.saveConfig(config,error));}
        {
            MainWindow window(12*1024*1024,nullptr,database);window.resize(640,480);window.show();window.openFolder(input);auto* viewer=window.findChild<PhotoViewer*>();QTRY_VERIFY(viewer->hasPhoto());
            auto* strip=window.findChild<Filmstrip*>();QVERIFY(strip->isVisible());QCOMPARE(strip->currentIndex(),0);
            auto* scroll=window.findChild<QScrollArea*>("destinationScroll");QTRY_VERIFY(scroll->horizontalScrollBar()->maximum()>0);
            auto* footer=window.findChild<QWidget*>("photoFooter");QVERIFY(footer->geometry().right()<window.width());QVERIFY(footer->height()<=90);
            auto* add=window.findChild<QPushButton*>("quickAddDestination");scroll->ensureWidgetVisible(add);QTest::qWait(30);QVERIFY(scroll->viewport()->rect().contains(add->mapTo(scroll->viewport(),add->rect().center())));
            scroll->horizontalScrollBar()->setValue(0);capture("crowded-footer-640",&window);
            QTest::mouseClick(strip->viewport(),Qt::LeftButton,Qt::NoModifier,QPoint(40,144));QCOMPARE(strip->currentIndex(),1);QTRY_VERIFY(viewer->hasPhoto());
            window.findChild<QPushButton*>("photoInfoButton")->click();auto* popup=window.findChild<PhotoPopup*>();QVERIFY(popup->isVisible());QVERIFY(!popup->isWindow());
            auto infoName=[&]{for(auto* value:popup->findChildren<QLabel*>("photoInfoValue"))if(value->property("field")=="File")return value->text();return QString();};
            QTRY_VERIFY(infoName().contains("photo_01"));QTest::keyClick(viewer,Qt::Key_Right);QTRY_VERIFY(infoName().contains("photo_02"));
            QVERIFY(viewer->rect().contains(window.findChild<QScrollArea*>("informationTools")->geometry()));capture("photo-information",&window);
            window.findChild<QPushButton*>("histogramButton")->click();QCOMPARE(popup->mode(),PhotoPopup::Histogram);QVERIFY(popup->isVisible());capture("histogram",&window);
            QTest::keyClick(viewer,Qt::Key_Escape);QVERIFY(!popup->isVisible());
            window.findChild<QPushButton*>("fullscreenButton")->click();QVERIFY(window.isFullScreen());QVERIFY(footer->isVisible());QVERIFY(strip->isVisible());QVERIFY(!window.findChild<QWidget*>("photoStatusRow")->isVisible());capture("fullscreen-folders-thumbnails",&window);
            QTest::keyClick(viewer,Qt::Key_Escape);QVERIFY(!window.isFullScreen());QVERIFY(footer->isVisible());QVERIFY(strip->isVisible());
            window.findChild<QPushButton*>("settingsButton")->click();auto* settings=window.findChild<SettingsDialog*>();settings->showPage(SettingsDialog::Viewing);QTRY_COMPARE(QApplication::activeModalWidget(),settings);
            auto* position=settings->findChild<QComboBox*>("filmstripPosition");QVERIFY(position);QCOMPARE(position->currentData().toString(),QString("left"));
            for(const auto& edge:QStringList{"top","left","right","bottom","right"}){
                position->setCurrentIndex(position->findData(edge));QCoreApplication::processEvents();
                QCOMPARE(strip->horizontalScrollBarPolicy(),Qt::ScrollBarAlwaysOff);QCOMPARE(strip->verticalScrollBarPolicy(),Qt::ScrollBarAlwaysOff);
                QVERIFY(strip->viewport()->toolTip().isEmpty());
                const bool vertical=edge=="left"||edge=="right";auto* bar=vertical?strip->verticalScrollBar():strip->horizontalScrollBar();strip->setCurrentIndex(0);
                QWheelEvent wheel(QPointF(30,30),QPointF(strip->mapToGlobal(QPoint(30,30))),QPoint(),QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(strip->viewport(),&wheel);QVERIFY(bar->value()>0);
                const auto stripPoint=strip->mapToGlobal(QPoint()),viewerPoint=viewer->mapToGlobal(QPoint());
                if(edge=="left")QVERIFY(stripPoint.x()<viewerPoint.x());else if(edge=="right")QVERIFY(stripPoint.x()>viewerPoint.x());else if(edge=="top")QVERIFY(stripPoint.y()<viewerPoint.y());else QVERIFY(stripPoint.y()>viewerPoint.y());
                capture("thumbnail-"+edge,&window);
            }
            QSignalSpy actions(window.findChild<ShortcutManager*>(),&ShortcutManager::triggered);QTest::keyClick(viewer,Qt::Key_Right);QVERIFY(actions.isEmpty());
            settings->findChild<QCheckBox*>("showFilmstrip")->setChecked(false);QVERIFY(!strip->isVisible());capture("viewing-settings",settings);settings->close();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
            window.resize(1280,800);window.findChild<QPushButton*>("toggleEdit")->setChecked(true);capture("viewer-with-edits",&window);
            auto* editCard=window.findChild<QScrollArea*>("editCard");auto* tools=window.findChild<QScrollArea*>("photoTools"); const auto editTop=editCard->mapTo(viewer,QPoint()).y();
            window.findChild<QPushButton*>("histogramButton")->click();QCoreApplication::processEvents();
            QVERIFY(popup->isVisible());QCOMPARE(editCard->mapTo(viewer,QPoint()).y(),editTop);QVERIFY(popup->mapTo(viewer,QPoint()).x()<editCard->mapTo(viewer,QPoint()).x());QCOMPARE(editCard->width(),popup->width());QVERIFY(!QRect(editCard->mapTo(viewer,QPoint()),editCard->size()).intersects(QRect(popup->mapTo(viewer,QPoint()),popup->size())));
            auto* exposure=window.findChild<QDoubleSpinBox*>("editExposureValue");tools->ensureWidgetVisible(exposure);QTest::qWait(30);
            QTest::mouseClick(exposure,Qt::LeftButton);QVERIFY(popup->isVisible());exposure->setFocus();exposure->selectAll();QTest::keyClicks(exposure,"1.4");QTest::keyClick(exposure,Qt::Key_Return);QCOMPARE(viewer->edits().exposure,14);QVERIFY(popup->isVisible());
            auto* crop=window.findChild<QPushButton*>("cropPhoto");auto* ratio=window.findChild<QComboBox*>("cropAspect");QCOMPARE(crop->geometry().top(),ratio->geometry().top());
            auto* reset=window.findChild<QPushButton*>("resetEdits");auto* keys=window.findChild<QPushButton*>("editingShortcutsButton");QCOMPARE(reset->geometry().top(),keys->geometry().top());QVERIFY(qAbs(reset->width()-keys->width())<=1);QCOMPARE(reset->height(),keys->height());
            tools->verticalScrollBar()->setValue(0);capture("histogram-with-edits",&window);window.resize(640,480);QCoreApplication::processEvents();QVERIFY(!QRect(editCard->mapTo(viewer,QPoint()),editCard->size()).intersects(QRect(popup->mapTo(viewer,QPoint()),popup->size())));tools->ensureWidgetVisible(keys);QTest::qWait(30);QVERIFY(tools->viewport()->rect().contains(keys->mapTo(tools->viewport(),keys->rect().center())));window.shutdown();
        }
        {MainWindow window(12*1024*1024,nullptr,database);window.show();window.openFolder(input);QTRY_VERIFY(window.findChild<PhotoViewer*>()->hasPhoto());QVERIFY(!window.findChild<Filmstrip*>()->isVisible());QCOMPARE(window.findChild<Filmstrip*>()->property("stripPosition").toString(),QString("right"));window.shutdown();}
    }
};
QTEST_MAIN(ViewerToolsTest)
#include "test_viewer_tools.moc"
