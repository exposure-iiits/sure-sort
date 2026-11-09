#pragma once
#include <QImage>
#include <QJsonObject>
#include <QRectF>
#include <memory>
class QPainter;

namespace photocull {
enum class WatermarkPosition { TopLeft, TopRight, BottomLeft, BottomRight };
struct WatermarkSettings {
    bool enabled=false;
    WatermarkPosition position=WatermarkPosition::BottomRight;
    int sizePercent=20,opacityPercent=100,paddingPercent=3;
    bool operator==(const WatermarkSettings& other) const;
};
struct WatermarkAsset {
    QString id,name;
    QByteArray png;
    QImage image; // Oriented sRGB, premultiplied alpha, immutable after import.
};
struct WatermarkSnapshot {
    WatermarkSettings settings;
    QString assetId;
    std::shared_ptr<const WatermarkAsset> asset;
    bool operator==(const WatermarkSnapshot& other) const { return settings==other.settings && assetId==other.assetId; }
};
QString validateWatermark(const WatermarkSnapshot& snapshot);
QJsonObject watermarkSettingsJson(const WatermarkSettings& settings);
bool readWatermarkSettings(const QJsonObject& json,WatermarkSettings& settings);
std::shared_ptr<const WatermarkAsset> decodeWatermarkPng(const QByteArray& png,const QString& name,QString& error);
std::shared_ptr<const WatermarkAsset> importWatermarkPng(const QString& path,QString& error);
QRectF watermarkRect(QSizeF photoSize,QSizeF logoSize,const WatermarkSettings& settings);
// Exactly the same geometry/compositing function is used by viewer and output.
void paintWatermark(QPainter& painter,const QRectF& photoRect,const WatermarkSnapshot& snapshot);
bool compositeWatermark(QImage& photo,const WatermarkSnapshot& snapshot,QString& error);
}
Q_DECLARE_METATYPE(photocull::WatermarkSnapshot)
