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
struct Destination { QString id, name, shortcut; QString action="move"; QString alternateShortcut = {}; };
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
    QString action="copy",operationId;
    bool outputReady=false;
    int automaticRetries=0;
    int retryOperation=0; // 0: none; 1: prepare, 2: recover, 3: move, 4: cleanup.
    QStringList readSources; // Resolved on the GUI thread at dispatch; originals stay in the durable snapshot.
};
struct UndoEntry { Photo photo; QString previousTask; QString actionId,categoryId; };
struct SessionSnapshot {
    DestinationConfig config;
    QString inputFolder, currentPhoto;
    QVector<ProcessingTask> tasks;
    QHash<QString, QString> assignments;
    QVector<UndoEntry> undo;
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
