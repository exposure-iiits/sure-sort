#pragma once
#include "photo.h"
#include "folder_scanner.h"
#include <QSize>
namespace photocull {
using PhotoInformation = QVector<QPair<QString, QString>>;
PhotoInformation readPhotoInformation(const Photo& photo, QSize sourceSize, const CancelToken& cancel);
}
