#pragma once
#include <QByteArray>
#include <QBuffer>
#include <QImage>
#include <QVector>
#include <algorithm>

// Original, synthetic DNG writer for tests. Contains no camera data or third-party fixtures.
namespace fixture {
inline QByteArray le16(quint16 n) { QByteArray b; b.append(char(n)); b.append(char(n>>8)); return b; }
inline QByteArray le32(quint32 n) { return le16(quint16(n)) + le16(quint16(n>>16)); }
struct Tag { quint16 key, type; quint32 count; QByteArray data; };
inline Tag shortTag(quint16 key, quint16 value) { return {key,3,1,le16(value)}; }
inline Tag longTag(quint16 key, quint32 value) { return {key,4,1,le32(value)}; }
inline QByteArray rawDng(const QVector<QByteArray>& previews, bool bitmapPreview = false, int orientation = 6) {
    constexpr int width=512,height=384;
    QByteArray sensor; sensor.reserve(width*height*2);
    for(int y=0;y<height;++y) for(int x=0;x<width;++x) sensor += le16(quint16(8000+x*40+y*30));
    QByteArray matrix;
    for(int i=0;i<9;++i) matrix += le32(i%4==0 ? 1 : 0) + le32(1);
    QVector<QVector<Tag>> ifds;
    ifds.append({longTag(254,0),longTag(256,width),longTag(257,height),shortTag(258,16),
        shortTag(259,1),shortTag(262,32803),{271,2,10,QByteArray("PhotoCull\0",10)},
        {272,2,10,QByteArray("Test DNG\0\0",10)},longTag(273,0),shortTag(274,quint16(orientation)),
        shortTag(277,1),longTag(278,height),longTag(279,quint32(sensor.size())),
        {33421,3,2,le16(2)+le16(2)},{33422,1,4,QByteArray::fromHex("00010102")},
        {50706,1,4,QByteArray::fromHex("01040000")},{50707,1,4,QByteArray::fromHex("01010000")},
        {50708,2,16,QByteArray("PhotoCull DNG\0\0\0\0",16)},longTag(50717,65535),
        {50721,10,9,matrix},shortTag(50778,21)});
    QVector<QByteArray> payloads{sensor};
    for (const auto& bytes:previews) {
        const QImage image=bitmapPreview ? QImage(80,40,QImage::Format_RGB888) : QImage::fromData(bytes,"JPEG");
        const int pw=image.width(),ph=image.height();
        QByteArray payload=bytes;
        if(bitmapPreview) {
            payload.clear();
            for(int y=0;y<ph;++y) for(int x=0;x<pw;++x) { payload.append(char(240));payload.append(char(50));payload.append(char(20)); }
        }
        ifds.append({longTag(254,1),longTag(256,quint32(pw)),longTag(257,quint32(ph)),
            {258,3,3,le16(8)+le16(8)+le16(8)},shortTag(259,bitmapPreview?1:7),
            shortTag(262,bitmapPreview?2:6),longTag(273,0),shortTag(274,quint16(orientation)),
            shortTag(277,3),longTag(278,quint32(ph)),longTag(279,quint32(payload.size())),shortTag(284,1)});
        payloads.append(payload);
    }
    QVector<quint32> offsets;
    quint32 cursor=8;
    for(const auto& tags:ifds) { offsets.append(cursor); cursor += 2+quint32(tags.size())*12+4; }
    QByteArray data(int(cursor),'\0'); data.replace(0,8,QByteArray("II",2)+le16(42)+le32(8));
    auto append=[&](QByteArray value) {
        if(data.size()%2) data.append('\0');
        const auto offset=quint32(data.size()); data+=value; return offset;
    };
    for(int i=0;i<ifds.size();++i) {
        auto tags=ifds[i]; std::sort(tags.begin(),tags.end(),[](const Tag& a,const Tag& b){return a.key<b.key;});
        const quint32 payloadOffset=append(payloads[i]);
        QByteArray ifd=le16(quint16(tags.size()));
        for(auto tag:tags) {
            if(tag.key==273) tag.data=le32(payloadOffset);
            ifd+=le16(tag.key)+le16(tag.type)+le32(tag.count);
            if(tag.data.size()>4) ifd+=le32(append(tag.data));
            else { tag.data.resize(4); ifd+=tag.data; }
        }
        ifd+=le32(i+1<offsets.size()?offsets[i+1]:0);
        data.replace(int(offsets[i]),ifd.size(),ifd);
    }
    return data;
}
}
