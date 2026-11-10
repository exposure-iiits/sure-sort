#include "exif_metadata.h"
#include <QFile>
#include <QSet>
#include <QDateTime>
#include <QLocale>
#include <QtEndian>
#include <cmath>
#include <functional>
#include <algorithm>
namespace photocull {
namespace {
QByteArray u16(quint16 v){QByteArray b(2,Qt::Uninitialized);qToLittleEndian(v,b.data());return b;}
QByteArray u32(quint32 v){QByteArray b(4,Qt::Uninitialized);qToLittleEndian(v,b.data());return b;}
QByteArray be32(quint32 v){QByteArray b(4,Qt::Uninitialized);qToBigEndian(v,b.data());return b;}
quint32 crc32(QByteArrayView data){quint32 crc=0xffffffff;for(auto byte:data){crc^=quint8(byte);for(int i=0;i<8;++i)crc=(crc>>1)^(0xedb88320U&quint32(-int(crc&1)));}return ~crc;}
}
ExifMetadata ExifMetadata::read(const QString& path){
    ExifMetadata result;QFile file(path);if(!file.open(QIODevice::ReadOnly))return result;
    auto read=[&](qint64 offset,qint64 size)->QByteArray{if(offset<0||size<0||size>65536||offset>file.size()-size||!file.seek(offset))return {};return file.read(size);};
    qint64 base=0,limit=file.size();const auto header=read(0,8);
    if(header.startsWith(QByteArray::fromHex("ffd8"))){
        qint64 offset=2;bool found=false;
        for(int segments=0;segments<512&&offset<qMin<qint64>(file.size(),16*1024*1024);++segments){
            auto marker=read(offset,2);if(marker.size()!=2||quint8(marker[0])!=255)break;
            const auto code=quint8(marker[1]);if(code==0xda||code==0xd9)break;
            if(code==255){++offset;continue;}if(code==1||(code>=0xd0&&code<=0xd7)){offset+=2;continue;}
            auto len=read(offset+2,2);if(len.size()!=2)break;const int length=qFromBigEndian<quint16>(len.constData());if(length<2)break;
            if(code==0xe1&&length>=16&&read(offset+4,6)==QByteArray("Exif\0\0",6)){base=offset+10;limit=offset+2+length;found=true;break;}
            offset+=2+length;
        }
        if(!found)return result;
    }else if(header==QByteArray::fromHex("89504e470d0a1a0a")){
        qint64 offset=8;bool found=false;
        for(int chunks=0;chunks<2048&&offset<file.size()-12;++chunks){auto chunk=read(offset,8);if(chunk.size()!=8)break;
            const auto length=qFromBigEndian<quint32>(chunk.constData());if(length>quint64(file.size()-offset-12))break;
            if(chunk.mid(4)=="eXIf"&&length<=65527){const auto bytes=read(offset+4,qint64(length)+4),crc=read(offset+8+length,4);
                if(crc.size()!=4||crc32(bytes)!=qFromBigEndian<quint32>(crc.constData()))return result;
                base=offset+8;limit=base+length;found=true;break;}
            offset+=qint64(length)+12;
        }if(!found)return result;
    }
    const auto tiff=read(base,8);if(tiff.size()!=8)return result;
    const bool little=tiff.startsWith("II");if(!little&&!tiff.startsWith("MM"))return result;
    auto n16=[&](const char* p){return little?qFromLittleEndian<quint16>(p):qFromBigEndian<quint16>(p);};
    auto n32=[&](const char* p){return little?qFromLittleEndian<quint32>(p):qFromBigEndian<quint32>(p);};
    if(n16(tiff.constData()+2)!=42)return result;
    auto relative=[&](quint64 offset,qint64 size){return size>=0&&offset<=quint64(qMax<qint64>(0,limit-base-size))&&base+qint64(offset)<=limit-size?read(base+qint64(offset),size):QByteArray();};
    const QSet<int> imageTags={270,271,272,282,283,296,306,315,33432};
    const QSet<int> exifTags={33434,33437,34850,34855,34864,34865,34866,34867,36864,36867,36868,36880,36881,36882,37121,37377,37378,37379,37380,37381,37382,37383,37384,37385,37386,37510,37520,37521,37522,40960,41483,41486,41487,41488,41495,41728,41729,41985,41986,41987,41988,41989,41990,41991,41992,41993,41994,42032,42033,42034,42035,42036,42037};
    int budget=48000;
    std::function<void(quint32,int)> directory=[&](quint32 offset,int kind){
        const auto countBytes=relative(offset,2);if(countBytes.size()!=2)return;const int count=n16(countBytes.constData());if(count>512)return;
        const auto entries=relative(quint64(offset)+2,count*12);if(entries.size()!=count*12)return;
        for(int i=0;i<count;++i){const auto* e=entries.constData()+i*12;const int tag=n16(e),type=n16(e+2);const quint32 countValue=n32(e+4);
            if(kind==0&&(tag==34665||tag==34853)&&type==4&&countValue==1){directory(n32(e+8),tag==34665?1:2);continue;}
            if(!(kind==0?imageTags.contains(tag):kind==1?exifTags.contains(tag):tag<=31))continue;
            const int widths[]={0,1,1,2,4,8,1,1,2,4,8,4,8};if(type<1||type>12)continue;
            const quint64 size=quint64(countValue)*widths[type];if(!size||size>4096||size>quint64(budget))continue;
            QByteArray bytes=size<=4?QByteArray(e+8,qsizetype(size)):relative(n32(e+8),qint64(size));if(bytes.size()!=qsizetype(size))continue;
            if(!little&&widths[type]>1){const int unit=(type==5||type==10)?4:widths[type];for(int j=0;j<bytes.size();j+=unit)std::reverse(bytes.begin()+j,bytes.begin()+j+unit);}
            (kind==0?result.image_:kind==1?result.exif_:result.gps_).insert(quint16(tag),{quint16(type),countValue,bytes});budget-=int(size);
        }
    };
    directory(n32(tiff.constData()+4),0);return result;
}
void ExifMetadata::camera(const char* make,const char* model,float iso,float shutter,float aperture,float focal,const char* lens){
    auto text=[](Directory& dir,int tag,const char* value){const auto bytes=QByteArray(value).left(255);if(!bytes.isEmpty()&&!dir.contains(tag))dir.insert(tag,{2,quint32(bytes.size()+1),bytes+char(0)});};
    text(image_,271,make);text(image_,272,model);text(exif_,42036,lens);
    if(std::isfinite(iso)&&iso>0&&!exif_.contains(34855))exif_.insert(34855,{3,1,u16(quint16(qBound(1.0,double(iso),65535.0)))});
    auto rational=[&](int tag,float value){if(std::isfinite(value)&&value>0&&value<4000&&!exif_.contains(tag))exif_.insert(tag,{5,1,u32(quint32(std::round(value*1000000)))+u32(1000000)});};
    rational(33434,shutter);rational(33437,aperture);rational(37386,focal);
}
QVector<QPair<QString, QString>> ExifMetadata::displayFields() const {
    QVector<QPair<QString, QString>> fields;
    auto text=[](const Directory& directory,int tag){return QString::fromUtf8(directory.value(tag).bytes).remove(QChar(0)).trimmed();};
    auto number=[](const Directory& directory,int tag)->double {
        const auto value=directory.value(tag);
        if(value.type==3&&value.bytes.size()>=2)return qFromLittleEndian<quint16>(value.bytes.constData());
        if(value.type==4&&value.bytes.size()>=4)return qFromLittleEndian<quint32>(value.bytes.constData());
        if(value.type==5&&value.bytes.size()>=8){const auto denominator=qFromLittleEndian<quint32>(value.bytes.constData()+4);if(denominator)return double(qFromLittleEndian<quint32>(value.bytes.constData()))/denominator;}
        return 0;
    };
    auto add=[&](const QString& name,const QString& value){if(!value.isEmpty())fields.append({name,value});};
    const auto make=text(image_,271),model=text(image_,272);
    add("Camera",model.startsWith(make,Qt::CaseInsensitive)?model:(make+" "+model).trimmed());
    add("Lens",text(exif_,42036));
    const auto iso=number(exif_,34855),aperture=number(exif_,33437),shutter=number(exif_,33434),focal=number(exif_,37386);
    if(iso>0)add("ISO",QString::number(iso,'g',7));
    if(aperture>0)add("Aperture","f/"+QString::number(aperture,'g',3));
    if(shutter>0)add("Shutter",shutter<1?QString("1/%1 s").arg(qRound64(1/shutter)):QString::number(shutter,'g',4)+" s");
    if(focal>0)add("Focal length",QString::number(focal,'g',4)+" mm");
    const auto date=text(exif_,36867).isEmpty()?text(image_,306):text(exif_,36867);
    const auto captured=QDateTime::fromString(date,"yyyy:MM:dd HH:mm:ss");
    if(captured.isValid()){
        add("Date taken",QLocale().toString(captured.date(),QLocale::LongFormat));
        add("Time taken",captured.time().toString("HH:mm:ss")+(text(exif_,36881).isEmpty()?QString():" "+text(exif_,36881)));
    }
    add("Photographer",text(image_,315));add("Copyright",text(image_,33432));
    return fields;
}
QByteArray ExifMetadata::serialize(QSize size) const {
    auto image=image_,exif=exif_,gps=gps_;
    image.insert(256,{4,1,u32(size.width())});image.insert(257,{4,1,u32(size.height())});image.insert(274,{3,1,u16(1)});
    exif.insert(40961,{3,1,u16(1)});exif.insert(40962,{4,1,u32(size.width())});exif.insert(40963,{4,1,u32(size.height())});
    image.insert(34665,{4,1,u32(0)});if(!gps.isEmpty())image.insert(34853,{4,1,u32(0)});
    const quint32 exifOffset=8+2+image.size()*12+4,gpsOffset=exifOffset+2+exif.size()*12+4;
    image[34665].bytes=u32(exifOffset);if(!gps.isEmpty())image[34853].bytes=u32(gpsOffset);
    const int start=gps.isEmpty()?int(gpsOffset):int(gpsOffset+2+gps.size()*12+4);
    QByteArray out(start,char(0));out.replace(0,8,QByteArray("II",2)+u16(42)+u32(8));
    auto write=[&](const Directory& entries,int offset){out.replace(offset,2,u16(quint16(entries.size())));offset+=2;
        for(auto it=entries.cbegin();it!=entries.cend();++it){const auto& v=it.value();QByteArray value=v.bytes;
            if(value.size()>4){if(out.size()%2)out.append(char(0));const auto p=out.size();out.append(value);value=u32(quint32(p));}else value.append(QByteArray(4-value.size(),char(0)));
            out.replace(offset,12,u16(it.key())+u16(v.type)+u32(v.count)+value);offset+=12;}
    };
    write(image,8);write(exif,int(exifOffset));if(!gps.isEmpty())write(gps,int(gpsOffset));return out;
}
MetadataDevice::MetadataDevice(QIODevice& output,const QByteArray& format,const QByteArray& tiff):output_(output),prefix_(format=="jpeg"?2:33){
    if(!tiff.isEmpty()){
        if(format=="jpeg"){const auto payload=QByteArray("Exif\0\0",6)+tiff;insertion_=QByteArray::fromHex("ffe1");const quint16 length=quint16(payload.size()+2);insertion_.append(char(length>>8));insertion_.append(char(length));insertion_+=payload;}
        else {const auto payload=QByteArray("eXIf")+tiff;insertion_=be32(quint32(tiff.size()))+payload+be32(crc32(payload));}
    }open(QIODevice::WriteOnly);
}
qint64 MetadataDevice::writeData(const char* data,qint64 length){
    const qint64 original=length;
    if(prefix_){const int n=int(qMin<qint64>(prefix_,length));if(output_.write(data,n)!=n)return -1;data+=n;length-=n;prefix_-=n;
        if(!prefix_&&!insertion_.isEmpty()){if(output_.write(insertion_)!=insertion_.size())return -1;insertion_.clear();}}
    if(length&&output_.write(data,length)!=length)return -1;
    return original;
}
}
