#include "photo_information.h"
#include "exif_metadata.h"
#include <QFileInfo>
#include <QImageReader>
#include <QFile>
#include <QLocale>
#include <QDateTime>
#include <libraw/libraw.h>
namespace photocull {
PhotoInformation readPhotoInformation(const Photo& photo, QSize sourceSize, const CancelToken& cancel) {
    PhotoInformation result;
    const auto path=photo.displayPath();const QFileInfo file(path);
    result.append({"File",file.fileName()});
    result.append({"Format",photo.typeLabel()});
    result.append({"File size",QLocale().formattedDataSize(file.size())+QString(" (%1 bytes)").arg(QLocale().toString(file.size()))});
    auto metadata=ExifMetadata::read(path);
    if(cancel&&cancel->load())return {};
    if(isRawExtension(file.suffix())){
        LibRaw raw;
#ifdef Q_OS_WIN
        const int status=raw.open_file(reinterpret_cast<const wchar_t*>(path.utf16()));
#else
        const int status=raw.open_file(QFile::encodeName(path).constData());
#endif
        if(status==LIBRAW_SUCCESS){
            const auto& data=raw.imgdata;
            metadata.camera(data.idata.make,data.idata.model,data.other.iso_speed,data.other.shutter,data.other.aperture,data.other.focal_len,data.lens.Lens);
            sourceSize=QSize(data.sizes.width,data.sizes.height);if(data.sizes.flip==5||data.sizes.flip==6)sourceSize.transpose();
            bool hasDate=false;for(const auto& field:metadata.displayFields())hasDate|=field.first=="Date taken";
            if(!hasDate&&data.other.timestamp>0){
                const auto date=QDateTime::fromSecsSinceEpoch(data.other.timestamp);
                result.append({"Date taken",QLocale().toString(date.date(),QLocale::LongFormat)});
                result.append({"Time taken",date.time().toString("HH:mm:ss")});
            }
        }
    }else if(!sourceSize.isValid()){
        QImageReader reader(path);sourceSize=reader.size();
        if(reader.transformation()&QImageIOHandler::TransformationRotate90)sourceSize.transpose();
    }
    if(cancel&&cancel->load())return {};
    if(sourceSize.isValid())result.append({"Dimensions",QString("%1 × %2 · %3 MP").arg(sourceSize.width()).arg(sourceSize.height()).arg(double(sourceSize.width())*sourceSize.height()/1000000,0,'f',1)});
    const auto fields=metadata.displayFields();result+=fields;
    if(fields.isEmpty())result.append(QPair<QString,QString>{"Camera metadata","Not available in this file"});
    result.append({"Modified",QLocale().toString(file.lastModified(),QLocale::ShortFormat)});
    if(photo.sources.size()>1)result.append({"Paired files",QString::number(photo.sources.size())+" · "+photo.sources.join("\n")});
    result.append({"Location",file.absoluteFilePath()});
    return result;
}
}
