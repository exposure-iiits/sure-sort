#include "session_store.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QCryptographicHash>

namespace photocull {
namespace {
QString encodeSources(const QStringList& paths) { return QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(paths)).toJson(QJsonDocument::Compact)); }
QStringList decodeSources(const QString& json) { QStringList result; for (const auto& v : QJsonDocument::fromJson(json.toUtf8()).array()) result.append(v.toString()); return result; }
QString nonNull(const QString& value) { return value.isNull()?QString(""):value; }
}
SessionStore::~SessionStore() {
    if (db_.isValid()) db_.close();
    db_ = {};
    if (!connection_.isEmpty()) QSqlDatabase::removeDatabase(connection_);
}
bool SessionStore::exec(const QString& sql, const QVariantList& args, QString& error) {
    QSqlQuery q(db_);
    if (!q.prepare(sql)) { error = q.lastError().text(); return false; }
    for (const auto& arg : args) q.addBindValue(arg);
    if (!q.exec()) { error = q.lastError().text(); return false; }
    return true;
}
bool SessionStore::open(const QString& path, QString& error) {
    if (db_.isOpen()) return true;
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) { error = "Cannot create session directory."; return false; }
    lock_ = std::make_unique<QLockFile>(path + ".lock");
    lock_->setStaleLockTime(0); // Sessions last hours; only dead-process detection may reclaim the lock.
    if (!lock_->tryLock(0)) { error = "This session is already open in another Sure Sort instance."; return false; }
    connection_ = "photocull-" + newId();
    db_ = QSqlDatabase::addDatabase("QSQLITE", connection_);
    db_.setDatabaseName(path); db_.setConnectOptions("QSQLITE_BUSY_TIMEOUT=1000");
    if (!db_.open()) { error = db_.lastError().text(); return false; }
    { QSqlQuery version(db_); if(!version.exec("PRAGMA application_id") || !version.next()) {error="Cannot read progress format.";return false;}
      const int format=version.value(0).toInt();QSqlQuery tables(db_);tables.exec("SELECT name FROM sqlite_master WHERE type='table'");
      if((format!=1397970004 && tables.next()) || (format!=0 && format!=1397970004)) {error="Unsupported session format. Use a new copy-only session file.";return false;}
      version.exec("PRAGMA user_version");if(!version.next()||version.value(0).toInt()>2){error="Progress requires a newer Sure Sort version.";return false;}}
    const QStringList schema = {
        "PRAGMA journal_mode=DELETE", "PRAGMA synchronous=FULL", "PRAGMA foreign_keys=ON",
        "CREATE TABLE IF NOT EXISTS metadata (key TEXT PRIMARY KEY,value TEXT NOT NULL)",
        "CREATE TABLE IF NOT EXISTS destinations (id TEXT PRIMARY KEY,name TEXT NOT NULL,shortcut TEXT NOT NULL,position INTEGER NOT NULL,alternate_shortcut TEXT NOT NULL DEFAULT '')",
        "CREATE TABLE IF NOT EXISTS photos (id TEXT PRIMARY KEY,sources TEXT NOT NULL)",
        "CREATE TABLE IF NOT EXISTS tasks (id TEXT PRIMARY KEY,photo_id TEXT NOT NULL,sources TEXT NOT NULL,category_id TEXT NOT NULL,category_name TEXT NOT NULL,directory TEXT NOT NULL,status TEXT NOT NULL,temp_path TEXT NOT NULL,output_path TEXT NOT NULL,digest BLOB NOT NULL,identity BLOB NOT NULL,error TEXT NOT NULL,watermark_settings TEXT NOT NULL,watermark_asset TEXT NOT NULL,edits TEXT NOT NULL,predecessor_path TEXT NOT NULL,predecessor_digest BLOB NOT NULL,predecessor_identity BLOB NOT NULL,output_settings TEXT NOT NULL,automatic_retries INTEGER NOT NULL DEFAULT 0,retry_operation INTEGER NOT NULL DEFAULT 0,removal_requested INTEGER NOT NULL DEFAULT 0)",
        "CREATE TABLE IF NOT EXISTS assignments (photo_id TEXT NOT NULL,category_id TEXT NOT NULL,task_id TEXT NOT NULL REFERENCES tasks(id),PRIMARY KEY(photo_id,category_id))",
        "DROP TABLE IF EXISTS undo_history",
        "CREATE TABLE IF NOT EXISTS watermark_assets (id TEXT PRIMARY KEY,png BLOB NOT NULL,name TEXT NOT NULL)",
        "CREATE TABLE IF NOT EXISTS photo_edits (photo_id TEXT PRIMARY KEY,parameters TEXT NOT NULL)",
        "PRAGMA application_id=1397970004", "PRAGMA user_version=2"
    };
    for (const auto& sql : schema) if (!exec(sql, {}, error)) return false;
    return finishDestinationRename(error);
}
bool SessionStore::destinationRenamePending() const {
    QSqlQuery q(db_);return !q.exec("SELECT value FROM metadata WHERE key='pending_destination_rename'")||q.next();
}
bool SessionStore::renameDestination(const QString& id,const QString& name,QString& error){
    error=validateFolderName(name);if(!error.isEmpty())return false;
    QSqlQuery q(db_);q.prepare("SELECT d.name,m.value FROM destinations d JOIN metadata m ON m.key='output_root' WHERE d.id=?");q.addBindValue(id);
    if(!q.exec()||!q.next()){error="Sorting folder is unavailable.";return false;}
    const auto previous=q.value(0).toString(),root=absolutePath(q.value(1).toString());q.finish();
    if(previous==name)return true;
    const auto from=QDir(root).filePath(previous),to=QDir(root).filePath(name);
    if(QFileInfo::exists(to)||QFileInfo(to).isSymLink()){error="A folder with that name already exists. Choose another name.";return false;}
    if(QFileInfo(from).isSymLink()){error="Symbolic-link folders cannot be renamed here.";return false;}
    // A destination containing imported sources cannot be renamed.
    if(!q.exec("SELECT sources FROM tasks")){error=q.lastError().text();return false;}
    while(q.next())for(const auto& source:decodePhotoSources(q.value(0).toString())){
        const auto relative=QDir(from).relativeFilePath(source);
        if(!QDir::isAbsolutePath(relative)&&relative!=".."&&!relative.startsWith("../")){
            error="This destination also contains imported source photos. Its name cannot be changed in this session.";return false;
        }
    }
    q.finish();
    if(!QDir().mkpath(from)){error="Cannot access the sorting folder.";return false;}
    const auto journal=QString::fromUtf8(QJsonDocument(QJsonObject{{"id",id},{"name",name},{"from",from},{"to",to}}).toJson(QJsonDocument::Compact));
    // Persist intent before touching the directory. Startup reconciles a crash
    // on either side of the rename before loading any processing snapshots.
    if(!exec("INSERT OR REPLACE INTO metadata VALUES('pending_destination_rename',?)",{journal},error))return false;
    return finishDestinationRename(error);
}
bool SessionStore::finishDestinationRename(QString& error){
    QSqlQuery q(db_);if(!q.exec("SELECT value FROM metadata WHERE key='pending_destination_rename'")){error=q.lastError().text();return false;}
    if(!q.next())return true;
    const auto journal=QJsonDocument::fromJson(q.value(0).toString().toUtf8()).object();q.finish();
    const auto from=journal["from"].toString(),to=journal["to"].toString(),id=journal["id"].toString(),name=journal["name"].toString();
    if(from.isEmpty()||to.isEmpty()||id.isEmpty()||!validateFolderName(name).isEmpty()){error="Invalid pending folder rename.";return false;}
    const bool oldExists=QFileInfo::exists(from),newExists=QFileInfo::exists(to);
    if(oldExists&&newExists){error="Folder rename is blocked because both folder names exist. Both folders were preserved.";return false;}
    if(QFileInfo(from).isSymLink()||QFileInfo(to).isSymLink()){error="Folder rename found a symbolic link. Files were preserved.";return false;}
    if(oldExists&&!QDir().rename(from,to)){
        // No filesystem change happened, so it is safe to discard this intent.
        QString ignored;exec("DELETE FROM metadata WHERE key='pending_destination_rename'",{},ignored);
        error="Cannot rename the folder. Close files using it and try again.";return false;
    }
    if(!QFileInfo(to).isDir()){error="The renamed folder is unavailable. Reconnect the output drive.";return false;}
    if(!db_.transaction()){error=db_.lastError().text();return false;}
    for(const auto& column:QStringList{"directory","temp_path","output_path","predecessor_path"}){
        QSqlQuery paths(db_);if(!paths.exec("SELECT id,"+column+" FROM tasks")){error=paths.lastError().text();db_.rollback();return false;}
        QVector<QPair<QString,QString>> updates;
        while(paths.next()){const auto path=absolutePath(paths.value(1).toString());const auto relative=QDir(from).relativeFilePath(path);if(!path.isEmpty()&&!QDir::isAbsolutePath(relative)&&relative!=".."&&!relative.startsWith("../"))updates.append({paths.value(0).toString(),storedPath(QDir(to).filePath(relative))});}
        paths.finish();for(const auto& item:updates)if(!exec("UPDATE tasks SET "+column+"=? WHERE id=?",{item.second,item.first},error)){db_.rollback();return false;}
    }
    if(!exec("UPDATE destinations SET name=? WHERE id=?",{name,id},error)||
       !exec("UPDATE tasks SET category_name=? WHERE category_id=? AND directory=?",{name,id,storedPath(to)},error)||
       !exec("DELETE FROM metadata WHERE key='pending_destination_rename'",{},error)) {db_.rollback();return false;}
    if(!db_.commit()){error=db_.lastError().text();db_.rollback();return false;}
    return true;
}
bool SessionStore::load(SessionSnapshot& snapshot, QString& error) {
    photoIds_.clear();reverseIds_.clear();
    if(!sourceRoot_.isEmpty()){
        QSqlQuery photos(db_);if(!photos.exec("SELECT id,sources FROM photos")){error=photos.lastError().text();return false;}
        while(photos.next()){
            const auto paths=decodePhotoSources(photos.value(1).toString());if(paths.isEmpty())continue;
            const QFileInfo file(paths.first());QString identity=file.absolutePath()+"/"+(file.suffix().compare("png",Qt::CaseInsensitive)==0?"png:":"photo:")+file.completeBaseName().toCaseFolded();
#ifdef Q_OS_WIN
            identity=identity.toCaseFolded();
#endif
            const auto id=QString::fromLatin1(QCryptographicHash::hash(identity.toUtf8(),QCryptographicHash::Sha256).toHex());photoIds_[photos.value(0).toString()]=id;reverseIds_[id]=photos.value(0).toString();
        }
    }

    if (!db_.isOpen()) { error = "Session database is unavailable."; return false; }
    QSqlQuery q(db_);
    if (!q.exec("SELECT key,value FROM metadata")) { error = q.lastError().text(); return false; }
    while (q.next()) {
        if (q.value(0) == "output_root") snapshot.config.outputRoot=absolutePath(q.value(1).toString());
        if (q.value(0) == "input_folder") snapshot.inputFolder=sourceRoot_.isEmpty()?q.value(1).toString():sourceRoot_;
        if (q.value(0) == "current_photo") snapshot.currentPhoto=currentId(q.value(1).toString());
        if(q.value(0)=="output_settings"){
            const auto doc=QJsonDocument::fromJson(q.value(1).toString().toUtf8());
            if(!doc.isObject()||!readOutput(doc.object(),snapshot.output)){error="Saved output settings are invalid.";return false;}
        }
        if(q.value(0)=="edit_bindings"){
            const auto document=QJsonDocument::fromJson(q.value(1).toString().toUtf8());
            if(!document.isArray()){error="Saved editing shortcuts are invalid.";return false;}snapshot.editBindings=document.array();
        }
        if(q.value(0)=="ui_preferences"){
            const auto document=QJsonDocument::fromJson(q.value(1).toString().toUtf8());
            if(document.isObject())snapshot.uiPreferences=document.object();
        }
        if(q.value(0)=="watermark"){
            const auto document=QJsonDocument::fromJson(q.value(1).toString().toUtf8());
            const auto json=document.object();
            if(!document.isObject()||!readWatermarkSettings(json,snapshot.watermark.settings)){error="Saved watermark settings are invalid.";return false;}
            snapshot.watermark.assetId=json.value("asset").toString();
        }
    }
    if (!q.exec("SELECT id,name,shortcut,alternate_shortcut FROM destinations ORDER BY position")) { error = q.lastError().text(); return false; }
    while (q.next()) snapshot.config.folders.append({q.value(0).toString(),q.value(1).toString(),q.value(2).toString(),q.value(3).toString()});
    if (!q.exec("SELECT photo_id,parameters FROM photo_edits")){error=q.lastError().text();return false;}
    while(q.next()){
        const auto document=QJsonDocument::fromJson(q.value(1).toString().toUtf8());PhotoEdits edits;
        if(!document.isObject()||!readEdits(document.object(),edits)){error="Saved photo edits are invalid.";return false;}
        snapshot.edits.insert(currentId(q.value(0).toString()),edits);
    }
    if (!q.exec("SELECT id,photo_id,sources,category_id,category_name,directory,status,temp_path,output_path,digest,identity,error,watermark_settings,watermark_asset,edits,predecessor_path,predecessor_digest,predecessor_identity,output_settings,automatic_retries,retry_operation,removal_requested FROM tasks ORDER BY rowid")) { error = q.lastError().text(); return false; }
    while (q.next()) {
        ProcessingTask t;
        t.id = q.value(0).toString(); t.photo = {currentId(q.value(1).toString()),decodePhotoSources(q.value(2).toString())};
        t.categoryId = q.value(3).toString(); t.categoryName = q.value(4).toString(); t.directory=absolutePath(q.value(5).toString());
        t.status = q.value(6).toString(); t.temporaryPath=absolutePath(q.value(7).toString()); t.outputPath=absolutePath(q.value(8).toString());
        t.digest = q.value(9).toByteArray(); t.identity = q.value(10).toByteArray(); t.error = q.value(11).toString();
        const auto document=QJsonDocument::fromJson(q.value(12).toString().toUtf8());
        if(!document.isObject()||!readWatermarkSettings(document.object(),t.watermark.settings)){
            error="A task has invalid saved watermark settings.";return false;
        }
        const auto editDocument=QJsonDocument::fromJson(q.value(14).toString().toUtf8());
        if(!editDocument.isObject()||!readEdits(editDocument.object(),t.edits)){error="A task has invalid saved photo edits.";return false;}
        t.predecessorPath=absolutePath(q.value(15).toString());t.predecessorDigest=q.value(16).toByteArray();t.predecessorIdentity=q.value(17).toByteArray();
        const auto outputDoc=QJsonDocument::fromJson(q.value(18).toString().toUtf8());
        if(!outputDoc.isObject()||!readOutput(outputDoc.object(),t.output)){error="A task has invalid saved output settings.";return false;}
        t.automaticRetries=q.value(19).toInt();t.retryOperation=q.value(20).toInt();t.removalRequested=q.value(21).toBool();
        if(t.automaticRetries<0||t.automaticRetries>2||(t.retryOperation!=0&&t.retryOperation!=1&&t.retryOperation!=2&&t.retryOperation!=4)){error="Invalid saved retry state.";return false;}
        t.watermark.assetId=q.value(13).toString();snapshot.tasks.append(t);
    }
    if (!q.exec("SELECT photo_id,task_id,category_id FROM assignments")) { error = q.lastError().text(); return false; }
    while (q.next()) snapshot.assignments.insert(assignmentKey(currentId(q.value(0).toString()),q.value(2).toString()),q.value(1).toString());
    QString assetError;hydrateWatermark(snapshot.watermark,assetError); // Missing logos remain fixable in the settings UI.
    return true;
}
bool SessionStore::saveConfig(const DestinationConfig& config, QString& error) {
    if (!db_.transaction()) { error = db_.lastError().text(); return false; }
    bool ok = exec("INSERT OR REPLACE INTO metadata VALUES('output_root',?)", {storedPath(config.outputRoot)}, error)
        && exec("DELETE FROM destinations", {}, error);
    for (int i=0; ok && i<config.folders.size(); ++i) {
        const auto& f = config.folders[i];
        ok = exec("INSERT INTO destinations(id,name,shortcut,position,alternate_shortcut) VALUES(?,?,?,?,?)", {f.id,f.name,nonNull(f.shortcut),i,nonNull(f.alternateShortcut)}, error);
    }
    if (!ok) { db_.rollback(); return false; }
    if (!db_.commit()) { error = db_.lastError().text(); db_.rollback(); return false; }
    return true;
}
bool SessionStore::putTask(const ProcessingTask& t, QString& error) {
    if(!savePhoto(t.photo,error)||!putAsset(t.watermark,error))return false;
    // UPDATE preserves the task referenced by assignments; INSERT OR REPLACE would delete it.
    return exec("INSERT INTO tasks(id,photo_id,sources,category_id,category_name,directory,status,temp_path,output_path,digest,identity,error,watermark_settings,watermark_asset,edits,predecessor_path,predecessor_digest,predecessor_identity,output_settings,automatic_retries,retry_operation,removal_requested) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET status=excluded.status,temp_path=excluded.temp_path,output_path=excluded.output_path,digest=excluded.digest,identity=excluded.identity,error=excluded.error,removal_requested=excluded.removal_requested,automatic_retries=excluded.automatic_retries,retry_operation=excluded.retry_operation",
        {t.id,storedId(t.photo.id),encodePhotoSources(t.photo.sources),t.categoryId,t.categoryName,storedPath(t.directory),t.status,
         nonNull(storedPath(t.temporaryPath)),nonNull(storedPath(t.outputPath)),t.digest.isNull()?QByteArray(""):t.digest,t.identity.isNull()?QByteArray(""):t.identity,nonNull(t.error),
         QString::fromUtf8(QJsonDocument(watermarkSettingsJson(t.watermark.settings)).toJson(QJsonDocument::Compact)),nonNull(t.watermark.assetId),
         QString::fromUtf8(QJsonDocument(editsJson(t.edits)).toJson(QJsonDocument::Compact)),nonNull(storedPath(t.predecessorPath)),
         t.predecessorDigest.isNull()?QByteArray(""):t.predecessorDigest,t.predecessorIdentity.isNull()?QByteArray(""):t.predecessorIdentity,
         QString::fromUtf8(QJsonDocument(outputJson(t.output)).toJson(QJsonDocument::Compact)),t.automaticRetries,t.retryOperation,t.removalRequested},error);
}
bool SessionStore::saveEdits(const QString& photoId,const PhotoEdits& edits,QString& error) {
    error=validateEdits(edits);if(!error.isEmpty())return false;
    return exec("INSERT OR REPLACE INTO photo_edits VALUES(?,?)",{storedId(photoId),QString::fromUtf8(QJsonDocument(editsJson(edits)).toJson(QJsonDocument::Compact))},error);
}
bool SessionStore::saveOutput(const OutputSettings& settings,QString& error){
    error=validateOutput(settings);if(!error.isEmpty())return false;
    return exec("INSERT OR REPLACE INTO metadata VALUES('output_settings',?)",{QString::fromUtf8(QJsonDocument(outputJson(settings)).toJson(QJsonDocument::Compact))},error);
}
bool SessionStore::saveEditBindings(const QJsonArray& bindings,QString& error) {
    return exec("INSERT OR REPLACE INTO metadata VALUES('edit_bindings',?)",{QString::fromUtf8(QJsonDocument(bindings).toJson(QJsonDocument::Compact))},error);
}
bool SessionStore::saveUiPreferences(const QJsonObject& preferences,QString& error) {
    return exec("INSERT OR REPLACE INTO metadata VALUES('ui_preferences',?)",{QString::fromUtf8(QJsonDocument(preferences).toJson(QJsonDocument::Compact))},error);
}
bool SessionStore::putAsset(const WatermarkSnapshot& snapshot,QString& error,bool replace) {
    if(!snapshot.asset)return true;
    QSqlQuery existing(db_);existing.prepare("SELECT 1 FROM watermark_assets WHERE id=?");existing.addBindValue(snapshot.assetId);
    if(!existing.exec()){error=existing.lastError().text();return false;}
    if(existing.next()&&!replace){assets_.insert(snapshot.assetId,snapshot.asset);return true;}
    if(!exec("INSERT OR REPLACE INTO watermark_assets VALUES(?,?,?)",{snapshot.assetId,snapshot.asset->png,nonNull(snapshot.asset->name)},error))return false;
    assets_.insert(snapshot.assetId,snapshot.asset);return true;
}
bool SessionStore::hydrateWatermark(WatermarkSnapshot& snapshot,QString& error) {
    if(snapshot.assetId.isEmpty())return !snapshot.settings.enabled;
    if(snapshot.asset)return true;
    const auto cached=assets_.value(snapshot.assetId).lock();
    if(cached){snapshot.asset=cached;return true;}
    QSqlQuery q(db_);q.prepare("SELECT png,name FROM watermark_assets WHERE id=?");q.addBindValue(snapshot.assetId);
    if(!q.exec()||!q.next()){error="Saved watermark PNG is unavailable.";return false;}
    auto asset=decodeWatermarkPng(q.value(0).toByteArray(),q.value(1).toString(),error);
    if(!asset||asset->id!=snapshot.assetId){error="Saved watermark PNG is damaged or has changed.";return false;}
    snapshot.asset=asset;assets_.insert(snapshot.assetId,asset);return true;
}
bool SessionStore::saveWatermark(const WatermarkSnapshot& snapshot,QString& error) {
    error=validateWatermark(snapshot);if(!error.isEmpty())return false;
    if(!db_.transaction()){error=db_.lastError().text();return false;}
    auto json=watermarkSettingsJson(snapshot.settings);json.insert("asset",snapshot.assetId);
    if(!putAsset(snapshot,error,true)||!exec("INSERT OR REPLACE INTO metadata VALUES('watermark',?)",
        {QString::fromUtf8(QJsonDocument(json).toJson(QJsonDocument::Compact))},error)){db_.rollback();return false;}
    if(!db_.commit()){error=db_.lastError().text();db_.rollback();return false;}return true;
}
bool SessionStore::saveTask(const ProcessingTask& task, QString& error) {
    if(!db_.transaction()){error=db_.lastError().text();return false;}
    if(!putTask(task,error)){db_.rollback();return false;}
    if(!db_.commit()){error=db_.lastError().text();db_.rollback();return false;}return true;
}
bool SessionStore::changeAssignment(const Photo& photo, const QString& taskId, const ProcessingTask* created,
                                  const QString& retiredId, QString& error,const QString& categoryId) {
    if (!db_.transaction()) { error = db_.lastError().text(); return false; }
    bool ok = true;
    const auto category=!categoryId.isEmpty()?categoryId:created?created->categoryId:QString();
    if (ok && !retiredId.isEmpty()) ok = exec("UPDATE tasks SET status=?,automatic_retries=0,retry_operation=0,removal_requested=0 WHERE id=?",{created&&!created->predecessorPath.isEmpty()?"superseded":"retired",retiredId},error);
    if (ok && created) ok = putTask(*created,error);
    if(ok&&!retiredId.isEmpty())ok=exec("DELETE FROM assignments WHERE task_id=?",{retiredId},error);
    if(ok&&taskId.isEmpty())ok=exec("DELETE FROM assignments WHERE photo_id=? AND category_id=?",{storedId(photo.id),category},error);
    else if(ok)ok=exec("INSERT OR REPLACE INTO assignments(photo_id,category_id,task_id) VALUES(?,?,?)",{storedId(photo.id),created?created->categoryId:category,taskId},error);
    if (!ok) { db_.rollback(); return false; }
    if (!db_.commit()) { error = db_.lastError().text(); db_.rollback(); return false; }
    return true;
}
bool SessionStore::savePosition(const QString& folder, const QString& photoId, QString& error) {
    if (!db_.transaction()) { error = db_.lastError().text(); return false; }
    if (!exec("INSERT OR REPLACE INTO metadata VALUES('input_folder',?)",{sourceRoot_.isEmpty()?folder:QString(".")},error) ||
        !exec("INSERT OR REPLACE INTO metadata VALUES('current_photo',?)",{nonNull(storedId(photoId))},error)) { db_.rollback(); return false; }
    if (!db_.commit()) { error = db_.lastError().text(); return false; }
    return true;
}
QString SessionStore::storedPath(const QString& path) const {
    if(path.isEmpty()||sourceRoot_.isEmpty())return path;
    const auto relative=QDir(sourceRoot_).relativeFilePath(path);
    return !QDir::isAbsolutePath(relative)&&relative!=".."&&!relative.startsWith("../")?relative:path;
}
QString SessionStore::absolutePath(const QString& path) const {return path.isEmpty()||sourceRoot_.isEmpty()||QDir::isAbsolutePath(path)?path:QDir(sourceRoot_).absoluteFilePath(path);}
QString SessionStore::encodePhotoSources(const QStringList& paths) const {QStringList stored;for(const auto& path:paths)stored.append(storedPath(path));return encodeSources(stored);}
QStringList SessionStore::decodePhotoSources(const QString& json) const {auto paths=decodeSources(json);for(auto& path:paths)path=absolutePath(path);return paths;}
bool SessionStore::savePhoto(const Photo& photo,QString& error){return exec("INSERT INTO photos(id,sources) VALUES(?,?) ON CONFLICT(id) DO UPDATE SET sources=excluded.sources",{storedId(photo.id),encodePhotoSources(photo.sources)},error);}
bool SessionStore::saveCatalog(const Catalog& photos,QString& error){
    if(!db_.transaction()){error=db_.lastError().text();return false;}
    for(const auto& photo:photos)if(!savePhoto(photo,error)){db_.rollback();return false;}
    if(!db_.commit()){error=db_.lastError().text();db_.rollback();return false;}return true;
}
QString SessionStore::metadata(const QString& key) const {QSqlQuery q(db_);q.prepare("SELECT value FROM metadata WHERE key=?");q.addBindValue(key);return q.exec()&&q.next()?q.value(0).toString():QString();}
bool SessionStore::setMetadata(const QString& key,const QString& value,QString& error){return exec("INSERT OR REPLACE INTO metadata VALUES(?,?)",{key,nonNull(value)},error);}
bool SessionStore::forgetAssignment(const QString& photoId,const QString& categoryId,QString& error){return exec("DELETE FROM assignments WHERE photo_id=? AND category_id=?",{storedId(photoId),categoryId},error);}

}
