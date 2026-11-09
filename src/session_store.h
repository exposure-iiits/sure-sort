#pragma once
#include "sorting_types.h"
#include <QSqlDatabase>
#include <QLockFile>
#include <memory>

namespace photocull {
// Small GUI-thread transactions only. Sensor development, file hashing and output I/O are workers' work.
class SessionStore {
public:
    ~SessionStore();
    bool open(const QString& path, QString& error);
    bool load(SessionSnapshot& snapshot, QString& error);
    bool saveConfig(const DestinationConfig&, QString& error);
    bool renameDestination(const QString& id,const QString& name,QString& error);
    bool destinationRenamePending() const;
    bool saveTask(const ProcessingTask&, QString& error);
    bool changeAssignment(const Photo&, const QString& taskId, const ProcessingTask* created,
                          const QString& retiredId, bool pushUndo, const QString& previousId,
                          bool popUndo, QString& error,const QString& categoryId={},const QString& actionId={});
    QStringList resolveSources(const QStringList&) const;
    qint64 moveSequence(const QString& operation) const;
    bool savePosition(const QString& folder, const QString& photoId, QString& error);
    bool saveWatermark(const WatermarkSnapshot& snapshot,QString& error);
    bool hydrateWatermark(WatermarkSnapshot& snapshot,QString& error);
    bool saveEdits(const QString& photoId,const PhotoEdits& edits,QString& error);
    bool saveEditBindings(const QJsonArray& bindings,QString& error);
    bool saveUiPreferences(const QJsonObject& preferences,QString& error);
    bool saveOutput(const OutputSettings&,QString& error);
    bool isOpen() const { return db_.isOpen(); }
private:
    bool exec(const QString& sql, const QVariantList& args, QString& error);
    bool putTask(const ProcessingTask&, QString& error);
    bool putAsset(const WatermarkSnapshot&,QString& error,bool replace=false);
    bool finishDestinationRename(QString& error);
    QSqlDatabase db_;
    QString connection_;
    std::unique_ptr<QLockFile> lock_;
    QHash<QString,std::weak_ptr<const WatermarkAsset>> assets_;
};
}
