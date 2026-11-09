#pragma once
#include "folder_scanner.h"
#include <QImage>
#include <QJsonObject>
#include <QRectF>
#include <QTransform>

namespace photocull {
struct PhotoEdits {
    int exposure=0; // Tenths of an EV, -5 to +5 EV.
    int brightness=0,contrast=0,highlights=0,shadows=0,saturation=0; // -100 to 100.
    int rotation=0; // Clockwise quarter turns, after the original's EXIF orientation.
    QRectF crop{0,0,1,1}; // Normalized, UNROTATED original image coordinates.
    int rotationDegrees=0; // Additional clockwise degrees; legacy quarter turns remain readable.
    bool cropInRotatedSpace=false; // Free-angle crops use normalized rotated-canvas coordinates.
    bool operator==(const PhotoEdits& other) const;
    bool operator!=(const PhotoEdits& other) const {return !(*this==other);}
    bool isIdentity() const {return *this==PhotoEdits{};}
    bool hasTone() const {return exposure||brightness||contrast||highlights||shadows||saturation;}
};
QString validateEdits(const PhotoEdits& edits);
QJsonObject editsJson(const PhotoEdits& edits);
bool readEdits(const QJsonObject& json,PhotoEdits& edits);
QRectF rotatedNormalizedRect(QRectF rect,int clockwiseTurns);
QSize rotatedSize(QSize original,int clockwiseTurns);
int editRotationDegrees(const PhotoEdits& edits);
QTransform editRotationTransform(QSize original,const PhotoEdits& edits);
QSize editRotatedSize(QSize original,const PhotoEdits& edits);
QRectF editRotatedCrop(QSize original,const PhotoEdits& edits);
QRect editCropPixels(QSize original,const PhotoEdits& edits);
QSize editedSize(QSize original,const PhotoEdits& edits);
// Shared by asynchronous display rendering and original-resolution output.
QImage renderEdits(QImage original,const PhotoEdits& edits,const CancelToken& cancel={});
}
Q_DECLARE_METATYPE(photocull::PhotoEdits)
