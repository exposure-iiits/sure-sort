#include "watermark.h"
#include <QBuffer>
#include <QColorSpace>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>
#include <algorithm>
#include <cmath>

namespace photocull {
bool WatermarkSettings::operator==(const WatermarkSettings& other) const {
    return enabled==other.enabled&&position==other.position&&sizePercent==other.sizePercent&&
           opacityPercent==other.opacityPercent&&paddingPercent==other.paddingPercent;
}
QString validateWatermark(const WatermarkSnapshot& snapshot) {
    const auto& s=snapshot.settings;
    if(int(s.position)<0||int(s.position)>3||s.sizePercent<1||s.sizePercent>100||
       s.opacityPercent<0||s.opacityPercent>100||s.paddingPercent<0||s.paddingPercent>25)
        return "Watermark settings are outside their supported ranges.";
    if(s.enabled && (!snapshot.asset || snapshot.asset->image.isNull() || snapshot.assetId!=snapshot.asset->id))
        return "Select a valid PNG watermark, or disable watermarking.";
    return {};
}
QJsonObject watermarkSettingsJson(const WatermarkSettings& s) {
    return {{"enabled",s.enabled},{"position",int(s.position)},{"size",s.sizePercent},{"opacity",s.opacityPercent},{"padding",s.paddingPercent}};
}
bool readWatermarkSettings(const QJsonObject& json,WatermarkSettings& settings) {
    // Only the empty object is a legacy (Phase 2) default. Damaged settings
    // must never silently turn a watermarked job into an unwatermarked copy.
    if(!json.isEmpty()) {
        if(!json.value("enabled").isBool())return false;
        for(const auto* key:{"position","size","opacity","padding"}) {
            const auto value=json.value(key);
            if(!value.isDouble()||value.toDouble()!=value.toInt(-1))return false;
        }
    }
    settings.enabled=json.value("enabled").toBool(false);
    settings.position=WatermarkPosition(json.value("position").toInt(3));
    settings.sizePercent=json.value("size").toInt(20); settings.opacityPercent=json.value("opacity").toInt(100);
    settings.paddingPercent=json.value("padding").toInt(3);
    auto checked=settings;checked.enabled=false;
    return validateWatermark({checked,{},{}}).isEmpty();
}
std::shared_ptr<const WatermarkAsset> decodeWatermarkPng(const QByteArray& png,const QString& name,QString& error) {
    constexpr qsizetype limit=16*1024*1024;
    if(png.isEmpty()||png.size()>limit){error="Choose a PNG logo no larger than 16 MiB.";return {};}
    QBuffer buffer;buffer.setData(png);buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);reader.setAutoTransform(true);
    const auto size=reader.size();
    if(reader.format()!="png"||!size.isValid()||qint64(size.width())*size.height()>16LL*1024*1024){
        error="Choose a valid PNG logo of at most 16 megapixels.";return {};
    }
    auto image=reader.read();
    if(image.isNull()){error="Cannot decode PNG: "+reader.errorString();return {};}
    if(image.colorSpace().isValid())image.convertToColorSpace(QColorSpace::SRgb);
    else image.setColorSpace(QColorSpace::SRgb);
    image=image.convertToFormat(QImage::Format_ARGB32_Premultiplied);image.setDevicePixelRatio(1);
    auto asset=std::make_shared<WatermarkAsset>();
    asset->id=QString::fromLatin1(QCryptographicHash::hash(png,QCryptographicHash::Sha256).toHex());
    asset->name=name;asset->png=png;asset->image=std::move(image);return asset;
}
std::shared_ptr<const WatermarkAsset> importWatermarkPng(const QString& path,QString& error) {
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)){error="Cannot open watermark: "+file.errorString();return {};}
    if(file.size()>16*1024*1024){error="Choose a PNG logo no larger than 16 MiB.";return {};}
    const auto png=file.read(16*1024*1024+1);
    if(file.error()!=QFileDevice::NoError){error="Cannot read watermark PNG.";return {};}
    return decodeWatermarkPng(png,QFileInfo(path).fileName(),error);
}
QRectF watermarkRect(QSizeF photo,QSizeF logo,const WatermarkSettings& s) {
    if(photo.isEmpty()||logo.isEmpty())return {};
    const double shorter=qMin(photo.width(),photo.height());
    const double padding=shorter*qBound(0,s.paddingPercent,25)/100.0;
    const double availableWidth=qMax(0.0,photo.width()-2*padding),availableHeight=qMax(0.0,photo.height()-2*padding);
    const double edge=shorter*qBound(1,s.sizePercent,100)/100.0;
    const double scale=std::min({edge/qMax(logo.width(),logo.height()),availableWidth/logo.width(),availableHeight/logo.height()});
    const QSizeF size(logo.width()*scale,logo.height()*scale);
    const bool right=s.position==WatermarkPosition::TopRight||s.position==WatermarkPosition::BottomRight;
    const bool bottom=s.position==WatermarkPosition::BottomLeft||s.position==WatermarkPosition::BottomRight;
    return {QPointF(right?photo.width()-padding-size.width():padding,bottom?photo.height()-padding-size.height():padding),size};
}
void paintWatermark(QPainter& painter,const QRectF& photo,const WatermarkSnapshot& snapshot) {
    if(!snapshot.settings.enabled||snapshot.settings.opacityPercent==0||!snapshot.asset||photo.isEmpty())return;
    const auto local=watermarkRect(photo.size(),snapshot.asset->image.size(),snapshot.settings);
    painter.save();painter.setClipRect(photo,Qt::IntersectClip);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setRenderHint(QPainter::SmoothPixmapTransform,true);
    painter.setOpacity(snapshot.settings.opacityPercent/100.0);
    painter.drawImage(local.translated(photo.topLeft()),snapshot.asset->image);
    painter.restore();
}
bool compositeWatermark(QImage& photo,const WatermarkSnapshot& snapshot,QString& error) {
    error=validateWatermark(snapshot);
    if(!error.isEmpty())return false;
    if(!snapshot.settings.enabled)return true;
    if(photo.isNull()){error="Cannot watermark an empty photograph.";return false;}
    if(photo.colorSpace().isValid())photo.convertToColorSpace(QColorSpace::SRgb);
    else photo.setColorSpace(QColorSpace::SRgb);
    // Indexed, mono and grayscale inputs need a paintable color format.
    photo=photo.convertToFormat(photo.hasAlphaChannel()?QImage::Format_ARGB32_Premultiplied:QImage::Format_RGB32);
    photo.setDevicePixelRatio(1); // Output composition always uses physical image pixels.
    QPainter painter(&photo);
    if(!painter.isActive()){error="Cannot render watermark onto this image.";return false;}
    paintWatermark(painter,QRectF(QPointF(),photo.size()),snapshot);return true;
}
}
