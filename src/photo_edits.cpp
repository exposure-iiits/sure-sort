#include "photo_edits.h"
#include <QColorSpace>
#include <QTransform>
#include <array>
#include <cmath>

namespace photocull {
bool PhotoEdits::operator==(const PhotoEdits& o) const {
    return exposure==o.exposure&&brightness==o.brightness&&contrast==o.contrast&&highlights==o.highlights&&
           shadows==o.shadows&&saturation==o.saturation&&rotation==o.rotation&&crop==o.crop&&
           rotationDegrees==o.rotationDegrees&&cropInRotatedSpace==o.cropInRotatedSpace;
}
QString validateEdits(const PhotoEdits& e) {
    if(e.exposure < -50||e.exposure>50||e.rotation<0||e.rotation>3||e.rotationDegrees < -180||e.rotationDegrees>180)return "Invalid exposure or rotation.";
    for(int value:{e.brightness,e.contrast,e.highlights,e.shadows,e.saturation})if(value < -100||value>100)return "Invalid adjustment value.";
    for(double v:{e.crop.x(),e.crop.y(),e.crop.width(),e.crop.height()})if(!std::isfinite(v))return "Invalid crop coordinates.";
    if(e.crop.left()<0||e.crop.top()<0||e.crop.width()<0.001||e.crop.height()<0.001||e.crop.right()>1.0000001||e.crop.bottom()>1.0000001)
        return "Crop must stay inside the original photograph.";
    return {};
}
QJsonObject editsJson(const PhotoEdits& e) {
    return {{"exposure",e.exposure},{"brightness",e.brightness},{"contrast",e.contrast},{"highlights",e.highlights},
            {"shadows",e.shadows},{"saturation",e.saturation},{"rotation",e.rotation},
            {"rotationDegrees",e.rotationDegrees},{"cropInRotatedSpace",e.cropInRotatedSpace},
            {"x",e.crop.x()},{"y",e.crop.y()},{"width",e.crop.width()},{"height",e.crop.height()}};
}
bool readEdits(const QJsonObject& json,PhotoEdits& e) {
    e={};if(json.isEmpty())return true; // Legacy jobs have no edits.
    int* fields[]={&e.exposure,&e.brightness,&e.contrast,&e.highlights,&e.shadows,&e.saturation,&e.rotation};
    const char* keys[]={"exposure","brightness","contrast","highlights","shadows","saturation","rotation"};
    for(int i=0;i<7;++i){auto v=json.value(keys[i]);if(!v.isDouble()||v.toDouble()!=v.toInt(-9999))return false;*fields[i]=v.toInt();}
    if(json.contains("rotationDegrees")){
        const auto value=json["rotationDegrees"];if(!value.isDouble()||value.toDouble()!=value.toInt(-9999))return false;e.rotationDegrees=value.toInt();
    }
    if(json.contains("cropInRotatedSpace")){
        if(!json["cropInRotatedSpace"].isBool())return false;
        e.cropInRotatedSpace=json["cropInRotatedSpace"].toBool();
    }
    for(auto key:{"x","y","width","height"})if(!json.value(key).isDouble())return false;
    e.crop={json["x"].toDouble(),json["y"].toDouble(),json["width"].toDouble(),json["height"].toDouble()};
    return validateEdits(e).isEmpty();
}
QRectF rotatedNormalizedRect(QRectF rect,int turns) {
    for(int i=0;i<((turns%4+4)%4);++i)rect={1-rect.bottom(),rect.left(),rect.height(),rect.width()};
    // Floating point subtraction at a corner may produce a tiny negative edge.
    return rect.intersected(QRectF(0,0,1,1));
}
QSize rotatedSize(QSize size,int turns){return turns%2?size.transposed():size;}
int editRotationDegrees(const PhotoEdits& e){
    int angle=(e.rotation*90+e.rotationDegrees)%360;
    if(angle>180)angle-=360;
    if(angle < -180)angle+=360;
    return angle;
}
QTransform editRotationTransform(QSize original,const PhotoEdits& e){
    return QImage::trueMatrix(QTransform().rotate(editRotationDegrees(e)),original.width(),original.height());
}
QSize editRotatedSize(QSize original,const PhotoEdits& e){
    if(original.isEmpty())return {};
    const int angle=editRotationDegrees(e);
    if(angle%90==0)return rotatedSize(original,angle/90);
    return QTransform().rotate(angle).mapRect(QRectF(QPointF(),original)).toAlignedRect().size();
}
QRectF editRotatedCrop(QSize original,const PhotoEdits& e){
    if(e.cropInRotatedSpace)return e.crop;
    if(e.crop==QRectF(0,0,1,1))return e.crop;
    const int angle=editRotationDegrees(e);
    if(angle%90==0)return rotatedNormalizedRect(e.crop,angle/90);
    const QRectF pixels(e.crop.x()*original.width(),e.crop.y()*original.height(),e.crop.width()*original.width(),e.crop.height()*original.height());
    const auto mapped=editRotationTransform(original,e).mapRect(pixels);const auto size=editRotatedSize(original,e);
    return QRectF(mapped.x()/size.width(),mapped.y()/size.height(),mapped.width()/size.width(),mapped.height()/size.height()).intersected(QRectF(0,0,1,1));
}
QRect editCropPixels(QSize original,const PhotoEdits& e) {
    const auto size=editRotatedSize(original,e);if(size.isEmpty())return {};
    const auto r=editRotatedCrop(original,e);
    const int left=qBound(0,qRound(r.left()*size.width()),size.width()-1),top=qBound(0,qRound(r.top()*size.height()),size.height()-1);
    const int right=qBound(left+1,qRound(r.right()*size.width()),size.width()),bottom=qBound(top+1,qRound(r.bottom()*size.height()),size.height());
    return {left,top,right-left,bottom-top};
}
QSize editedSize(QSize original,const PhotoEdits& e){return editCropPixels(original,e).size();}
QImage renderEdits(QImage image,const PhotoEdits& e,const CancelToken& cancel) {
    if(image.isNull()||!validateEdits(e).isEmpty()||(cancel&&cancel->load()))return {};
    if(e.isIdentity())return image;
    const auto originalSize=image.size();
    if(image.colorSpace().isValid()&&image.colorSpace()!=QColorSpace(QColorSpace::SRgb))image.convertToColorSpace(QColorSpace::SRgb);
    else if(!image.colorSpace().isValid())image.setColorSpace(QColorSpace::SRgb);
    if(e.hasTone()) {
        std::array<int,256> lut;
        const double gain=std::exp2(e.exposure/10.0);
        auto smooth=[](double x){x=qBound(0.0,x,1.0);return x*x*(3-2*x);};
        for(int i=0;i<256;++i){
            double v=i/255.0;
            const double linear=v<=0.04045?v/12.92:std::pow((v+0.055)/1.055,2.4);
            const double exposed=linear*gain;v=exposed<=0.0031308?12.92*exposed:1.055*std::pow(exposed,1/2.4)-0.055;
            v+=e.brightness/400.0;v=(v-0.5)*(1+e.contrast/100.0)+0.5;
            v+=e.highlights/100.0*0.35*smooth((v-0.5)*2)+e.shadows/100.0*0.35*(1-smooth(v*2));
            lut[i]=qBound(0,qRound(v*255),255);
        }
        image=image.convertToFormat(image.hasAlphaChannel()?QImage::Format_ARGB32:QImage::Format_RGB32); // Straight channels; preserve alpha.
        const int sat=100+e.saturation;
        for(int y=0;y<image.height();++y){
            if((y&15)==0&&cancel&&cancel->load())return {};
            auto* row=reinterpret_cast<QRgb*>(image.scanLine(y));
            for(int x=0;x<image.width();++x){const auto px=row[x];const int r=lut[qRed(px)],g=lut[qGreen(px)],b=lut[qBlue(px)];
                if(e.saturation){const int l=(2126*r+7152*g+722*b+5000)/10000,neutral=l*(100-sat);row[x]=qRgba(qBound(0,(neutral+r*sat+50)/100,255),qBound(0,(neutral+g*sat+50)/100,255),qBound(0,(neutral+b*sat+50)/100,255),qAlpha(px));}
                else row[x]=qRgba(r,g,b,qAlpha(px));
            }
        }
    }
    if(cancel&&cancel->load())return {};
    const int angle=editRotationDegrees(e);
    if(angle)image=image.transformed(QTransform().rotate(angle),angle%90?Qt::SmoothTransformation:Qt::FastTransformation);
    const auto crop=editCropPixels(originalSize,e);if(crop!=image.rect())image=image.copy(crop);
    image.setDevicePixelRatio(1);return image;
}
}
