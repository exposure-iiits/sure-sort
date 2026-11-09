#include "session_store.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>

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
    { QSqlQuery version(db_); if(!version.exec("PRAGMA user_version") || !version.next()) {error="Cannot read session schema version.";return false;}
      if(version.value(0).toInt()>7) {error="This session was created by a newer Sure Sort version.";return false;} }
    const QStringList schema = {
        "PRAGMA journal_mode=WAL", "PRAGMA synchronous=FULL", "PRAGMA foreign_keys=ON",
        "CREATE TABLE IF NOT EXISTS metadata (key TEXT PRIMARY KEY,value TEXT NOT NULL)",
        "CREATE TABLE IF NOT EXISTS destinations (id TEXT PRIMARY KEY,name TEXT NOT NULL,shortcut TEXT NOT NULL,position INTEGER NOT NULL)",
        "CREATE TABLE IF NOT EXISTS tasks (id TEXT PRIMARY KEY,photo_id TEXT NOT NULL,sources TEXT NOT NULL,category_id TEXT NOT NULL,category_name TEXT NOT NULL,directory TEXT NOT NULL,status TEXT NOT NULL,temp_path TEXT NOT NULL,output_path TEXT NOT NULL,digest BLOB NOT NULL,identity BLOB NOT NULL,error TEXT NOT NULL)",
        "CREATE TABLE IF NOT EXISTS assignments (photo_id TEXT PRIMARY KEY,task_id TEXT NOT NULL REFERENCES tasks(id))",
        "CREATE TABLE IF NOT EXISTS undo_history (sequence INTEGER PRIMARY KEY AUTOINCREMENT,photo_id TEXT NOT NULL,sources TEXT NOT NULL,previous_task TEXT NOT NULL)",
        "CREATE TABLE IF NOT EXISTS watermark_assets (id TEXT PRIMARY KEY,png BLOB NOT NULL,name TEXT NOT NULL)",
        "CREATE TABLE IF NOT EXISTS photo_edits (photo_id TEXT PRIMARY KEY,parameters TEXT NOT NULL)",
        "CREATE TABLE IF NOT EXISTS source_locations (logical TEXT PRIMARY KEY,path TEXT NOT NULL,digest BLOB NOT NULL,identity BLOB NOT NULL)",
        "CREATE TABLE IF NOT EXISTS file_moves (id TEXT PRIMARY KEY,operation TEXT NOT NULL,logical TEXT NOT NULL,source TEXT NOT NULL,target TEXT NOT NULL,temporary TEXT NOT NULL,source_digest BLOB NOT NULL,source_identity BLOB NOT NULL,digest BLOB NOT NULL,identity BLOB NOT NULL,state TEXT NOT NULL,restore_of TEXT NOT NULL DEFAULT '',undo_requested INTEGER NOT NULL DEFAULT 0,restored INTEGER NOT NULL DEFAULT 0)"
    };
    for (const auto& sql : schema) if (!exec(sql, {}, error)) return false;
    // Additive migration: legacy tasks receive default output settings.
    QSet<QString> columns;
    {QSqlQuery q(db_);if(!q.exec("PRAGMA table_info(tasks)")){error=q.lastError().text();return false;}while(q.next())columns.insert(q.value(1).toString());}
    if(!db_.transaction()){error=db_.lastError().text();return false;}
    for(const auto& column:QStringList{"watermark_settings","watermark_asset","edits","output_settings"})if(!columns.contains(column)){
        if(!exec("ALTER TABLE tasks ADD COLUMN "+column+" TEXT NOT NULL DEFAULT "+(column=="watermark_asset"?"''":"'{}'"),{},error)){db_.rollback();return false;}
    }
    for(const auto& column:QStringList{"predecessor_path","predecessor_digest","predecessor_identity"})if(!columns.contains(column)){
        if(!exec("ALTER TABLE tasks ADD COLUMN "+column+" "+(column=="predecessor_path"?"TEXT NOT NULL DEFAULT ''":"BLOB NOT NULL DEFAULT X''"),{},error)){db_.rollback();return false;}
    }
    for(const auto& item:QVector<QPair<QString,QString>>{{"action","TEXT NOT NULL DEFAULT 'copy'"},{"operation_id","TEXT NOT NULL DEFAULT ''"},{"output_ready","INTEGER NOT NULL DEFAULT 0"},{"automatic_retries","INTEGER NOT NULL DEFAULT 0"},{"retry_operation","INTEGER NOT NULL DEFAULT 0"}})
        if(!columns.contains(item.first)&&!exec("ALTER TABLE tasks ADD COLUMN "+item.first+" "+item.second,{},error)){db_.rollback();return false;}
    auto addColumn=[&](const QString& table,const QString& name,const QString& definition){QSqlQuery q(db_);if(!q.exec("PRAGMA table_info("+table+")"))return false;while(q.next())if(q.value(1)==name)return true;return exec("ALTER TABLE "+table+" ADD COLUMN "+name+" "+definition,{},error);};
    if(!addColumn("destinations","action","TEXT NOT NULL DEFAULT 'copy'")||!addColumn("destinations","alternate_shortcut","TEXT NOT NULL DEFAULT ''")||!addColumn("undo_history","action_id","TEXT NOT NULL DEFAULT ''")||!addColumn("undo_history","category_id","TEXT NOT NULL DEFAULT ''")){db_.rollback();return false;}
    {QSqlQuery q(db_);q.exec("PRAGMA table_info(assignments)");bool migrated=false;while(q.next())if(q.value(1)=="category_id")migrated=true;
        if(!migrated&&(!exec("ALTER TABLE assignments RENAME TO assignments_legacy",{},error)||
            !exec("CREATE TABLE assignments (photo_id TEXT NOT NULL,category_id TEXT NOT NULL,task_id TEXT NOT NULL REFERENCES tasks(id),PRIMARY KEY(photo_id,category_id))",{},error)||
            !exec("INSERT INTO assignments SELECT a.photo_id,t.category_id,a.task_id FROM assignments_legacy a JOIN tasks t ON t.id=a.task_id",{},error)||
            !exec("DROP TABLE assignments_legacy",{},error))){db_.rollback();return false;}}
    if(!exec("PRAGMA user_version=7",{},error)){db_.rollback();return false;}
    if(!db_.commit()){error=db_.lastError().text();db_.rollback();return false;}
    return finishDestinationRename(error);
}
bool SessionStore::destinationRenamePending() const {
    QSqlQuery q(db_);return !q.exec("SELECT value FROM metadata WHERE key='pending_destination_rename'")||q.next();
}
bool SessionStore::renameDestination(const QString& id,const QString& name,QString& error){
    error=validateFolderName(name);if(!error.isEmpty())return false;
    QSqlQuery q(db_);q.prepare("SELECT d.name,m.value FROM destinations d JOIN metadata m ON m.key='output_root' WHERE d.id=?");q.addBindValue(id);
    if(!q.exec()||!q.next()){error="Sorting folder is unavailable.";return false;}
    const auto previous=q.value(0).toString(),root=q.value(1).toString();q.finish();
    if(previous==name)return true;
    const auto from=QDir(root).filePath(previous),to=QDir(root).filePath(name);
    if(QFileInfo::exists(to)||QFileInfo(to).isSymLink()){error="A folder with that name already exists. Choose another name.";return false;}
    if(QFileInfo(from).isSymLink()){error="Symbolic-link folders cannot be renamed here.";return false;}
    // Logical inputs retain stable IDs. Do not relocate files imported from the
    // destination itself; normal exports and moved Originals have other logical paths.
    if(!q.exec("SELECT sources FROM tasks")){error=q.lastError().text();return false;}
    while(q.next())for(const auto& source:decodeSources(q.value(0).toString())){
        const auto relative=QDir(from).relativeFilePath(source);
        if(!QDir::isAbsolutePath(relative)&&relative!=".."&&!relative.startsWith("../")){
            error="This destination also contains imported source photos. Its name cannot be changed in this session.";return false;
        }
    }
    q.finish();
    if(!QDir().mkpath(from)){error="Cannot access the sorting folder.";return false;}
    const auto journal=QString::fromUtf8(QJsonDocument(QJsonObject{{"id",id},{"name",name},{"from",from},{"to",to}}).toJson(QJsonDocument::Compact));
    // Persist intent before touching the directory. Startup reconciles a crash
    // on either side of the rename before loading any processing/Undo snapshots.
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
    for(const auto& table:QStringList{"tasks","source_locations","file_moves"}){
        const QStringList columns=table=="tasks"?QStringList{"directory","temp_path","output_path","predecessor_path"}:table=="source_locations"?QStringList{"path"}:QStringList{"source","target","temporary"};
        for(const auto& column:columns){
            const auto statement="UPDATE "+table+" SET "+column+"=? || substr("+column+",length(?) + 1) WHERE "+column+"=? OR substr("+column+",1,length(?) + 1)=? || '/'";
            if(!exec(statement,{to,from,from,from,from},error)){db_.rollback();return false;}
        }
    }
    if(!exec("UPDATE destinations SET name=? WHERE id=?",{name,id},error)||
       !exec("UPDATE tasks SET category_name=? WHERE category_id=? AND directory=?",{name,id,to},error)||
       !exec("DELETE FROM metadata WHERE key='pending_destination_rename'",{},error)) {db_.rollback();return false;}
    if(!db_.commit()){error=db_.lastError().text();db_.rollback();return false;}
    return true;
}
bool SessionStore::load(SessionSnapshot& snapshot, QString& error) {
    if (!db_.isOpen()) { error = "Session database is unavailable."; return false; }
    QSqlQuery q(db_);
    if (!q.exec("SELECT key,value FROM metadata")) { error = q.lastError().text(); return false; }
    while (q.next()) {
        if (q.value(0) == "output_root") snapshot.config.outputRoot = q.value(1).toString();
        if (q.value(0) == "input_folder") snapshot.inputFolder = q.value(1).toString();
        if (q.value(0) == "current_photo") snapshot.currentPhoto = q.value(1).toString();
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
    if (!q.exec("SELECT id,name,shortcut,action,alternate_shortcut FROM destinations ORDER BY position")) { error = q.lastError().text(); return false; }
    while (q.next()) snapshot.config.folders.append({q.value(0).toString(),q.value(1).toString(),q.value(2).toString(),q.value(3).toString(),q.value(4).toString()});
    if (!q.exec("SELECT photo_id,parameters FROM photo_edits")){error=q.lastError().text();return false;}
    while(q.next()){
        const auto document=QJsonDocument::fromJson(q.value(1).toString().toUtf8());PhotoEdits edits;
        if(!document.isObject()||!readEdits(document.object(),edits)){error="Saved photo edits are invalid.";return false;}
        snapshot.edits.insert(q.value(0).toString(),edits);
    }
    if (!q.exec("SELECT id,photo_id,sources,category_id,category_name,directory,status,temp_path,output_path,digest,identity,error,watermark_settings,watermark_asset,edits,predecessor_path,predecessor_digest,predecessor_identity,output_settings,action,operation_id,output_ready,automatic_retries,retry_operation FROM tasks ORDER BY rowid")) { error = q.lastError().text(); return false; }
    while (q.next()) {
        ProcessingTask t;
        t.id = q.value(0).toString(); t.photo = {q.value(1).toString(),decodeSources(q.value(2).toString())};
        t.categoryId = q.value(3).toString(); t.categoryName = q.value(4).toString(); t.directory = q.value(5).toString();
        t.status = q.value(6).toString(); t.temporaryPath = q.value(7).toString(); t.outputPath = q.value(8).toString();
        t.digest = q.value(9).toByteArray(); t.identity = q.value(10).toByteArray(); t.error = q.value(11).toString();
        const auto document=QJsonDocument::fromJson(q.value(12).toString().toUtf8());
        if(!document.isObject()||!readWatermarkSettings(document.object(),t.watermark.settings)){
            error="A task has invalid saved watermark settings.";return false;
        }
        const auto editDocument=QJsonDocument::fromJson(q.value(14).toString().toUtf8());
        if(!editDocument.isObject()||!readEdits(editDocument.object(),t.edits)){error="A task has invalid saved photo edits.";return false;}
        t.predecessorPath=q.value(15).toString();t.predecessorDigest=q.value(16).toByteArray();t.predecessorIdentity=q.value(17).toByteArray();
        const auto outputDoc=QJsonDocument::fromJson(q.value(18).toString().toUtf8());
        if(!outputDoc.isObject()||!readOutput(outputDoc.object(),t.output)){error="A task has invalid saved output settings.";return false;}
        t.action=q.value(19).toString();t.operationId=q.value(20).toString();if(t.operationId.isEmpty())t.operationId=t.id;t.outputReady=q.value(21).toBool();t.automaticRetries=q.value(22).toInt();t.retryOperation=q.value(23).toInt();
        if(t.automaticRetries<0||t.automaticRetries>2||t.retryOperation<0||t.retryOperation>4){error="Invalid saved retry state.";return false;}
        if(t.action!="copy"&&t.action!="move"){error="Invalid saved file action.";return false;}
        t.watermark.assetId=q.value(13).toString();snapshot.tasks.append(t);
    }
    if (!q.exec("SELECT photo_id,task_id,category_id FROM assignments")) { error = q.lastError().text(); return false; }
    while (q.next()) snapshot.assignments.insert(assignmentKey(q.value(0).toString(),q.value(2).toString()),q.value(1).toString());
    if (!q.exec("SELECT photo_id,sources,previous_task,action_id,category_id FROM undo_history ORDER BY sequence")) { error = q.lastError().text(); return false; }
    while (q.next()) snapshot.undo.append({{q.value(0).toString(),decodeSources(q.value(1).toString())},q.value(2).toString(),q.value(3).toString(),q.value(4).toString()});
    QHash<QString,ProcessingTask> byId;QHash<QString,QString> legacyLatest;
    for(const auto& t:snapshot.tasks){byId[t.id]=t;if(snapshot.assignments.value(assignmentKey(t.photo.id,t.categoryId))==t.id)legacyLatest[t.photo.id]=t.id;}
    for(auto it=snapshot.undo.rbegin();it!=snapshot.undo.rend();++it)if(it->categoryId.isEmpty()){
        const auto affected=byId.value(legacyLatest.value(it->photo.id));const auto previous=byId.value(it->previousTask);
        it->actionId=affected.operationId;it->categoryId=affected.id.isEmpty()?previous.categoryId:affected.categoryId;legacyLatest[it->photo.id]=it->previousTask;
    }
    QString assetError;hydrateWatermark(snapshot.watermark,assetError); // Missing logos remain fixable in the settings UI.
    return true;
}
bool SessionStore::saveConfig(const DestinationConfig& config, QString& error) {
    if (!db_.transaction()) { error = db_.lastError().text(); return false; }
    bool ok = exec("INSERT OR REPLACE INTO metadata VALUES('output_root',?)", {config.outputRoot}, error)
        && exec("DELETE FROM destinations", {}, error);
    for (int i=0; ok && i<config.folders.size(); ++i) {
        const auto& f = config.folders[i];
        ok = exec("INSERT INTO destinations(id,name,shortcut,position,action,alternate_shortcut) VALUES(?,?,?,?,?,?)", {f.id,f.name,nonNull(f.shortcut),i,f.action,nonNull(f.alternateShortcut)}, error);
    }
    if (!ok) { db_.rollback(); return false; }
    if (!db_.commit()) { error = db_.lastError().text(); db_.rollback(); return false; }
    return true;
}
bool SessionStore::putTask(const ProcessingTask& t, QString& error) {
    if(!putAsset(t.watermark,error))return false;
    // UPDATE preserves the task referenced by assignments; INSERT OR REPLACE would delete it.
    return exec("INSERT INTO tasks(id,photo_id,sources,category_id,category_name,directory,status,temp_path,output_path,digest,identity,error,watermark_settings,watermark_asset,edits,predecessor_path,predecessor_digest,predecessor_identity,output_settings,action,operation_id,output_ready,automatic_retries,retry_operation) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET status=excluded.status,temp_path=excluded.temp_path,output_path=excluded.output_path,digest=excluded.digest,identity=excluded.identity,error=excluded.error,output_ready=excluded.output_ready,automatic_retries=excluded.automatic_retries,retry_operation=excluded.retry_operation",
        {t.id,t.photo.id,encodeSources(t.photo.sources),t.categoryId,t.categoryName,t.directory,t.status,
         nonNull(t.temporaryPath),nonNull(t.outputPath),t.digest.isNull()?QByteArray(""):t.digest,t.identity.isNull()?QByteArray(""):t.identity,nonNull(t.error),
         QString::fromUtf8(QJsonDocument(watermarkSettingsJson(t.watermark.settings)).toJson(QJsonDocument::Compact)),nonNull(t.watermark.assetId),
         QString::fromUtf8(QJsonDocument(editsJson(t.edits)).toJson(QJsonDocument::Compact)),nonNull(t.predecessorPath),
         t.predecessorDigest.isNull()?QByteArray(""):t.predecessorDigest,t.predecessorIdentity.isNull()?QByteArray(""):t.predecessorIdentity,
         QString::fromUtf8(QJsonDocument(outputJson(t.output)).toJson(QJsonDocument::Compact)),t.action,t.operationId.isEmpty()?t.id:t.operationId,t.outputReady,t.automaticRetries,t.retryOperation},error);
}
bool SessionStore::saveEdits(const QString& photoId,const PhotoEdits& edits,QString& error) {
    error=validateEdits(edits);if(!error.isEmpty())return false;
    return exec("INSERT OR REPLACE INTO photo_edits VALUES(?,?)",{photoId,QString::fromUtf8(QJsonDocument(editsJson(edits)).toJson(QJsonDocument::Compact))},error);
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
bool SessionStore::saveTask(const ProcessingTask& task, QString& error) { return putTask(task,error); }
bool SessionStore::changeAssignment(const Photo& photo, const QString& taskId, const ProcessingTask* created,
                                  const QString& retiredId, bool pushUndo, const QString& previousId,
                                  bool popUndo, QString& error,const QString& categoryId,const QString& actionId) {
    if (!db_.transaction()) { error = db_.lastError().text(); return false; }
    bool ok = true;
    const auto category=!categoryId.isEmpty()?categoryId:created?created->categoryId:QString();
    if (pushUndo) ok = exec("INSERT INTO undo_history(photo_id,sources,previous_task,action_id,category_id) VALUES(?,?,?,?,?)", {photo.id,encodeSources(photo.sources),nonNull(previousId),nonNull(actionId.isEmpty()?(created?(created->operationId.isEmpty()?created->id:created->operationId):retiredId):actionId),nonNull(category)},error);
    if (ok && !retiredId.isEmpty()) ok = exec("UPDATE tasks SET status=?,automatic_retries=0,retry_operation=0 WHERE id=?",{created&&!created->predecessorPath.isEmpty()?"superseded":"retired",retiredId},error);
    if (ok && created) ok = putTask(*created,error);
    if(ok&&!retiredId.isEmpty())ok=exec("DELETE FROM assignments WHERE task_id=?",{retiredId},error);
    if(ok&&taskId.isEmpty())ok=exec("DELETE FROM assignments WHERE photo_id=? AND category_id=?",{photo.id,category},error);
    else if(ok)ok=exec("INSERT OR REPLACE INTO assignments(photo_id,category_id,task_id) VALUES(?,?,?)",{photo.id,created?created->categoryId:category,taskId},error);
    if (ok && popUndo) ok = exec("DELETE FROM undo_history WHERE sequence=(SELECT MAX(sequence) FROM undo_history)",{},error);
    if (!ok) { db_.rollback(); return false; }
    if (!db_.commit()) { error = db_.lastError().text(); db_.rollback(); return false; }
    return true;
}
QStringList SessionStore::resolveSources(const QStringList& paths) const {
    QStringList result;QSqlQuery q(db_);q.prepare("SELECT path FROM source_locations WHERE logical=?");
    for(const auto& path:paths){q.bindValue(0,path);result.append(q.exec()&&q.next()?q.value(0).toString():path);q.finish();}return result;
}
qint64 SessionStore::moveSequence(const QString& operation) const {
    QSqlQuery q(db_);q.prepare("SELECT MAX(rowid) FROM file_moves WHERE operation=? AND restore_of=''");q.addBindValue(operation);return q.exec()&&q.next()?q.value(0).toLongLong():0;
}
bool SessionStore::savePosition(const QString& folder, const QString& photoId, QString& error) {
    if (!db_.transaction()) { error = db_.lastError().text(); return false; }
    if (!exec("INSERT OR REPLACE INTO metadata VALUES('input_folder',?)",{folder},error) ||
        !exec("INSERT OR REPLACE INTO metadata VALUES('current_photo',?)",{nonNull(photoId)},error)) { db_.rollback(); return false; }
    if (!db_.commit()) { error = db_.lastError().text(); return false; }
    return true;
}
}
