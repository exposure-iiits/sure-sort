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
    void setSourceRoot(const QString& root) { sourceRoot_=root; }
    bool savePhoto(const Photo&,QString& error);
    bool saveCatalog(const Catalog&,QString& error);
    QString metadata(const QString& key) const;
    bool setMetadata(const QString& key,const QString& value,QString& error);
    bool forgetAssignment(const QString& photoId,const QString& categoryId,QString& error);
    bool load(SessionSnapshot& snapshot, QString& error);
    bool saveConfig(const DestinationConfig&, QString& error);
    bool renameDestination(const QString& id,const QString& name,QString& error);
    bool destinationRenamePending() const;
    bool saveTask(const ProcessingTask&, QString& error);
    bool changeAssignment(const Photo&, const QString& taskId, const ProcessingTask* created,
                          const QString& retiredId, bool pushUndo, const QString& previousId,
                          bool popUndo, QString& error,const QString& categoryId={});
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
    QString storedPath(const QString&) const;
    QString absolutePath(const QString&) const;
    QString encodePhotoSources(const QStringList&) const;
    QStringList decodePhotoSources(const QString&) const;
    QString storedId(const QString& id) const {return reverseIds_.value(id,id);}
    QString currentId(const QString& id) const {return photoIds_.value(id,id);}
    QSqlDatabase db_;
    QString connection_;
    std::unique_ptr<QLockFile> lock_;
    QHash<QString,std::weak_ptr<const WatermarkAsset>> assets_;
    QString sourceRoot_;
    QHash<QString,QString> photoIds_,reverseIds_;
};
}
