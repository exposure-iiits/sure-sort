#pragma once
#include <QJsonObject>
#include <QSize>
#include <QString>
namespace photocull {
struct OutputSettings {
    int jpegQuality=95;
    int resolution=100;
    bool preserveMetadata=true;
    bool operator==(const OutputSettings& o) const {return jpegQuality==o.jpegQuality&&resolution==o.resolution&&preserveMetadata==o.preserveMetadata;}
    bool operator!=(const OutputSettings& o) const {return !(*this==o);}
};
QString validateOutput(const OutputSettings&);
QJsonObject outputJson(const OutputSettings&);
bool readOutput(const QJsonObject&,OutputSettings&);
QSize outputSize(QSize,const OutputSettings&);
}
