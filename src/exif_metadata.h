#pragma once
#include <QByteArray>
#include <QIODevice>
#include <QMap>
#include <QSize>
#include <QString>
namespace photocull {
// Bounded TIFF/EXIF subset. Structural offsets, thumbnails and MakerNotes are
// deliberately not copied to an image with different pixels and dimensions.
class ExifMetadata {
public:
    static ExifMetadata read(const QString& path);
    void camera(const char* make,const char* model,float iso,float shutter,float aperture,float focal,const char* lens);
    QByteArray serialize(QSize finalSize) const;
private:
    struct Value {quint16 type;quint32 count;QByteArray bytes;};
    using Directory=QMap<quint16,Value>;
    Directory image_,exif_,gps_;
};
// Inserts a small metadata block while the encoder streams to disk; never
// buffers the encoded photograph or rewrites a second full-size file.
class MetadataDevice : public QIODevice {
public:
    MetadataDevice(QIODevice& output,const QByteArray& format,const QByteArray& tiff);
    bool isSequential() const override {return true;}
protected:
    qint64 readData(char*,qint64) override {return -1;}
    qint64 writeData(const char*,qint64) override;
private:
    QIODevice& output_;
    QByteArray insertion_;
    int prefix_;
};
}
