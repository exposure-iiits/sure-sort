#pragma once
#include "folder_scanner.h"
#include <QImage>
#include <QSize>
#include <QByteArray>
#include <optional>

namespace photocull {
struct DecodeRequest {
    Photo photo;
    QSize target;
    bool fullResolution = false;
    bool allowRawFallback = false;
};
struct DecodedImage {
    QImage image;
    QSize sourceSize;
    QString error;
    QString note;
};
struct ThumbnailCandidate { int index; QSize size; bool jpeg; int flip = 0; };
// Public pure helpers are shared by production RAW decoding and synthetic tests.
QVector<int> rankThumbnails(QVector<ThumbnailCandidate> candidates, QSize target, bool largest);
std::optional<int> jpegExifOrientation(const QByteArray& jpeg);
QImage applyRawFlip(QImage image, int flip);
class ImageDecoder {
public:
    static DecodedImage decode(const DecodeRequest& request, const CancelToken& cancel);
private:
    static DecodedImage decodeRaw(const QString& path, const DecodeRequest&, const CancelToken&);
};
}
Q_DECLARE_METATYPE(photocull::DecodedImage)
