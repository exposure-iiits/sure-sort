#include "output_settings.h"
#include <QJsonValue>
namespace photocull {
QString validateOutput(const OutputSettings& s){return s.jpegQuality<80||s.jpegQuality>100||(s.resolution!=100&&s.resolution!=75&&s.resolution!=50)?QString("Invalid output settings."):QString();}
QJsonObject outputJson(const OutputSettings& s){return {{"quality",s.jpegQuality},{"resolution",s.resolution},{"metadata",s.preserveMetadata}};}
bool readOutput(const QJsonObject& j,OutputSettings& s){
    if(j.isEmpty()){s={};return true;}
    if(!j["quality"].isDouble()||!j["resolution"].isDouble()||!j["metadata"].isBool())return false;
    const double quality=j["quality"].toDouble(),resolution=j["resolution"].toDouble();
    if(quality!=j["quality"].toInt(-9999)||resolution!=j["resolution"].toInt(-9999))return false;
    s={int(quality),int(resolution),j["metadata"].toBool()};return validateOutput(s).isEmpty();
}
QSize outputSize(QSize size,const OutputSettings& s){return {qMax(1,qRound(size.width()*s.resolution/100.0)),qMax(1,qRound(size.height()*s.resolution/100.0))};}
}
