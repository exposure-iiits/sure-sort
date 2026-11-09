#pragma once
#include <QString>
#include <QStringList>
#include <QMetaType>
#include <QVector>

namespace photocull {
struct Photo {
    QString id;
    QStringList sources;
    QString displayPath() const;
    QString name() const;
    QString typeLabel() const;
};
using Catalog = QVector<Photo>;
bool isRawExtension(const QString& extension);
bool isSupportedExtension(const QString& extension);
}
Q_DECLARE_METATYPE(photocull::Photo)
Q_DECLARE_METATYPE(photocull::Catalog)
