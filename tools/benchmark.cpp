#include "folder_scanner.h"
#include "image_cache.h"
#include "watermark.h"
#include "photo_edits.h"
#include <QBuffer>
#include <QPainter>
#include <QGuiApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QSysInfo>
#include <QThread>
#include <algorithm>

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    const int entries = argc > 1 ? QString::fromLocal8Bit(argv[1]).toInt() : 1000;
    if (entries < 1 || entries > 100000) return 6;
    QTemporaryDir dir;
    if (!dir.isValid()) return 1;
    QTextStream out(stdout);
    QImage image(6000,4000,QImage::Format_RGB32);
    for (int y = 0; y < image.height(); ++y) {
        auto* row = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) row[x] = qRgb((x*17+y*3)%256,(x+y*7)%256,(x*3+y*13)%256);
    }
    const auto path = dir.filePath("IMG_0.jpg");
    if (!image.save(path,"JPEG",90)) return 2;
    image = {};
    for (int i=1; i<entries; ++i) {
        QFile file(dir.filePath(QString("IMG_%1.JPG").arg(i)));
        if (!file.open(QIODevice::WriteOnly)) return 3;
        file.write("scan-only fixture");
    }
    QElapsedTimer timer; timer.start();
    auto catalog = photocull::scanFolder(dir.path());
    const double scanMs = timer.nsecsElapsed()/1e6;
    photocull::Photo photo{"fixture",{path}};
    const auto cancel = std::make_shared<std::atomic_bool>(false);
    QVector<double> timings;
    photocull::DecodedImage result;
    for (int i=0; i<10; ++i) {
        timer.restart();
        result = photocull::ImageDecoder::decode({photo,{1920,1080},false,true},cancel);
        if (result.image.isNull()) { out << result.error << '\n'; return 4; }
        timings.append(timer.nsecsElapsed()/1e6);
    }
    std::sort(timings.begin(),timings.end());
    photocull::ImageCache cache;
    const photocull::CacheKey key{photo.id,{1920,1080}};
    cache.insert(key,result);
    timer.restart();
    for (int i=0; i<10000; ++i) { photocull::DecodedImage hit; if (!cache.get(key,hit)) return 5; }
    const double cacheUs = timer.nsecsElapsed()/1000.0/10000;
    QImage logo(1200,600,QImage::Format_ARGB32_Premultiplied);logo.fill(QColor(245,220,160,150));
    QByteArray png;QBuffer buffer(&png);buffer.open(QIODevice::WriteOnly);if(!logo.save(&buffer,"PNG"))return 7;
    QString error;photocull::WatermarkSnapshot watermark;watermark.asset=photocull::decodeWatermarkPng(png,"benchmark.png",error);
    if(!watermark.asset)return 8;
    watermark.assetId=watermark.asset->id;watermark.settings.enabled=true;watermark.settings.opacityPercent=75;
    QImage canvas(1920,1080,QImage::Format_RGB32);canvas.fill(Qt::gray);
    timer.restart();
    {QPainter painter(&canvas);for(int i=0;i<300;++i)photocull::paintWatermark(painter,QRectF(QPointF(),canvas.size()),watermark);}
    const double overlayMs=timer.nsecsElapsed()/1e6/300;
    QImage full(6000,4000,QImage::Format_RGB32);full.fill(Qt::gray);QVector<double> compositeTimes;
    for(int i=0;i<10;++i){auto output=full;timer.restart();if(!photocull::compositeWatermark(output,watermark,error))return 9;compositeTimes.append(timer.nsecsElapsed()/1e6);}
    std::sort(compositeTimes.begin(),compositeTimes.end());
    photocull::PhotoEdits edits;edits.exposure=7;edits.brightness=5;edits.contrast=15;edits.highlights=-10;edits.shadows=20;edits.saturation=10;
    QVector<double> editPreviewTimes,editFullTimes;
    for(int i=0;i<10;++i){timer.restart();auto preview=photocull::renderEdits(canvas,edits);if(preview.isNull())return 10;editPreviewTimes.append(timer.nsecsElapsed()/1e6);}
    for(int i=0;i<10;++i){timer.restart();auto output=photocull::renderEdits(full,edits);if(output.isNull())return 11;editFullTimes.append(timer.nsecsElapsed()/1e6);}
    std::sort(editPreviewTimes.begin(),editPreviewTimes.end());std::sort(editFullTimes.begin(),editFullTimes.end());
    out << "OS: " << QSysInfo::prettyProductName() << "; CPU arch: " << QSysInfo::currentCpuArchitecture()
        << "; logical processors: " << QThread::idealThreadCount() << '\n'
        << "Qt " << qVersion() << "; fixture JPEG bytes: " << QFileInfo(path).size() << '\n'
        << "Scan entries/photos: " << catalog.photos.size() << "; ms: " << scanMs << '\n'
        << "24MP JPEG -> 1920x1080, 10 warm filesystem trials; min/median/max ms: "
        << timings.front() << '/' << timings[5] << '/' << timings.back() << '\n'
        << "Decoded size: " << result.image.width() << 'x' << result.image.height() << "; cache bytes: " << cache.bytes() << '\n'
        << "10000 cache lookups, mean us: " << cacheUs << '\n'
        << "Cached 1200x600 alpha PNG, 20% size/75% opacity/3% padding; 1920x1080 overlay-only mean ms (300 paints): " << overlayMs << '\n'
        << "24MP copy-on-write + composite (no decode/encode), min/median/max ms (10 trials): "
        << compositeTimes.front() << '/' << compositeTimes[5] << '/' << compositeTimes.back() << '\n'
        << "Six combined tone controls, 1920x1080 copy + render min/median/max ms (10 trials): " << editPreviewTimes.front() << '/' << editPreviewTimes[5] << '/' << editPreviewTimes.back() << '\n'
        << "Six combined tone controls, 24MP copy + render min/median/max ms (10 trials): " << editFullTimes.front() << '/' << editFullTimes[5] << '/' << editFullTimes.back() << '\n';
}
