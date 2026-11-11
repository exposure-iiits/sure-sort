#pragma once
#include "photo.h"
#include <QKeySequence>
#include <QByteArray>
#include <QHash>
#include "watermark.h"
#include "photo_edits.h"
#include "output_settings.h"
#include <QJsonArray>
#include <QJsonObject>

namespace photocull {
struct Destination { QString id, name, shortcut; QString alternateShortcut = {}; };
struct DestinationConfig { QString outputRoot; QVector<Destination> folders; };
struct ProcessingTask {
    QString id;
    Photo photo;
    QString categoryId, categoryName, directory;
    QString status = "queued";
    QString temporaryPath, outputPath;
    QByteArray digest, identity;
    QString error;
    WatermarkSnapshot watermark;
    PhotoEdits edits;
    OutputSettings output;
    QString predecessorPath;
    QByteArray predecessorDigest,predecessorIdentity;
    bool removalRequested=false;
    int automaticRetries=0;
    int retryOperation=0; // 0: none; 1: prepare, 2: recover, 4: cleanup.
};
struct SessionSnapshot {
    DestinationConfig config;
    QString inputFolder, currentPhoto;
    QVector<ProcessingTask> tasks;
    QHash<QString, QString> assignments;
    WatermarkSnapshot watermark;
    QHash<QString,PhotoEdits> edits;
    QJsonArray editBindings;
    QJsonObject uiPreferences;
    OutputSettings output;
};
QString validateFolderName(const QString& name);
QString validateDestinationConfig(const DestinationConfig& config);
QString newId();
QString assignmentKey(const QString& photoId,const QString& categoryId);
}
Q_DECLARE_METATYPE(photocull::ProcessingTask)
Q_DECLARE_METATYPE(photocull::DestinationConfig)
