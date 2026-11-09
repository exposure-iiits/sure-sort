#include "source_mover.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif
namespace photocull {
namespace {
bool cancelled(const CancelToken& token){return token&&token->load();}
QString freePath(const QString& directory,const QString& filename){
    const QFileInfo name(filename);for(int i=1;i<1000000;++i){const auto path=QDir(directory).filePath(i==1?filename:name.completeBaseName()+"_"+QString::number(i)+'.'+name.suffix());if(!QFileInfo::exists(path)&&!QFileInfo(path).isSymLink())return path;}return {};
}
}
SourceMover::SourceMover(const QString& path){
    connection_="source-moves-"+newId();db_=QSqlDatabase::addDatabase("QSQLITE",connection_);db_.setDatabaseName(path);db_.setConnectOptions("QSQLITE_BUSY_TIMEOUT=1000");
    if(!db_.open())error_=db_.lastError().text();else exec("PRAGMA synchronous=FULL");
}
SourceMover::~SourceMover(){db_.close();db_={};QSqlDatabase::removeDatabase(connection_);}
bool SourceMover::exec(const QString& sql,const QVariantList& args){
    if(!error_.isEmpty())return false;
    QSqlQuery q(db_);if(!q.prepare(sql)){error_=q.lastError().text();return false;}
    for(const auto& arg:args)q.addBindValue(arg);
    if(!q.exec()){error_=q.lastError().text();return false;}return true;
}
bool SourceMover::save(const Transfer& t){
    auto bytes=[](const QByteArray& b){return b.isNull()?QByteArray(""):b;};
    return exec("INSERT INTO file_moves(id,operation,logical,source,target,temporary,source_digest,source_identity,digest,identity,state,restore_of,undo_requested,restored) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET target=excluded.target,temporary=excluded.temporary,digest=excluded.digest,identity=excluded.identity,state=excluded.state,undo_requested=excluded.undo_requested,restored=excluded.restored",
        {t.id,t.operation,t.logical,t.source,t.target,t.temporary,bytes(t.sourceDigest),bytes(t.sourceIdentity),bytes(t.digest),bytes(t.identity),t.state,t.restoreOf.isNull()?QString(""):t.restoreOf,t.undoRequested,t.restored});
}
QVector<SourceMover::Transfer> SourceMover::transfers(const QString& where,const QVariantList& args){
    QVector<Transfer> result;if(!error_.isEmpty())return result;QSqlQuery q(db_);q.prepare("SELECT id,operation,logical,source,target,temporary,source_digest,source_identity,digest,identity,state,restore_of,undo_requested,restored FROM file_moves WHERE "+where);
    for(const auto& arg:args)q.addBindValue(arg);
    if(!q.exec()){error_=q.lastError().text();return {};}
    while(q.next())result.append({q.value(0).toString(),q.value(1).toString(),q.value(2).toString(),q.value(3).toString(),q.value(4).toString(),q.value(5).toString(),q.value(6).toByteArray(),q.value(7).toByteArray(),q.value(8).toByteArray(),q.value(9).toByteArray(),q.value(10).toString(),q.value(11).toString(),q.value(12).toBool(),q.value(13).toBool()});
    return result;
}
QString SourceMover::resolve(const QString& logical){QSqlQuery q(db_);q.prepare("SELECT path FROM source_locations WHERE logical=?");q.addBindValue(logical);if(!q.exec()){error_=q.lastError().text();return {};}return q.next()?q.value(0).toString():logical;}
bool SourceMover::complete(Transfer& t){
    if(!db_.transaction()){error_=db_.lastError().text();return false;}
    t.state="complete";bool ok=save(t)&&
        exec("UPDATE source_locations SET path=?,digest=?,identity=? WHERE path=?",{t.target,t.digest,t.identity,t.source})&&
        exec("INSERT OR REPLACE INTO source_locations VALUES(?,?,?,?)",{t.logical,t.target,t.digest,t.identity});
    if(ok&&!t.restoreOf.isEmpty())ok=exec("UPDATE file_moves SET restored=1 WHERE id=?",{t.restoreOf})&&
        exec("UPDATE file_moves SET identity=? WHERE target=? AND digest=? AND restored=0 AND state='complete'",{t.identity,t.target,t.digest});
    if(!ok||!db_.commit()){if(error_.isEmpty())error_=db_.lastError().text();db_.rollback();return false;}return true;
}
bool SourceMover::run(Transfer& t,const CancelToken& cancel){
    if(t.state=="complete")return true;
    if(cancelled(cancel)){error_="Original relocation paused.";return false;}
    if(t.state=="planned"){
        QByteArray hash,identity;if(!ProcessingPipeline::fingerprint(t.source,hash,identity,error_)||hash!=t.sourceDigest||identity!=t.sourceIdentity){error_="Original changed before relocation; preserved: "+t.source;return false;}
        // An interrupted, unjournaled partial is never assumed to be owned.
        if(QFileInfo::exists(t.temporary)){t.temporary=QDir(QFileInfo(t.target).absolutePath()).filePath(".suresort-original-"+newId()+".part");if(!save(t))return false;}
        QFile input(t.source),output(t.temporary);if(!input.open(QIODevice::ReadOnly)||!output.open(QIODevice::WriteOnly|QIODevice::NewOnly)){error_="Cannot stage original relocation: "+t.source;return false;}
        const qint64 expected=input.size();qint64 copied=0;
        while(!input.atEnd()&&!cancelled(cancel)){const auto block=input.read(1024*1024);if((block.isEmpty()&&input.error()!=QFileDevice::NoError)||output.write(block)!=block.size()){error_="Could not copy original; source preserved.";return false;}copied+=block.size();}
        if(cancelled(cancel)){error_="Original relocation paused.";return false;}
        if(copied!=expected||input.size()!=expected||input.error()!=QFileDevice::NoError||!output.flush()){error_="Original changed or the destination could not be written; source preserved.";return false;}
        output.setFileTime(input.fileTime(QFileDevice::FileModificationTime),QFileDevice::FileModificationTime);
#ifdef Q_OS_WIN
        if(!FlushFileBuffers(reinterpret_cast<HANDLE>(_get_osfhandle(output.handle())))){error_="Cannot synchronize original backup; source preserved.";return false;}
#else
        if(::fsync(output.handle())!=0){error_="Cannot synchronize original backup.";return false;}
#endif
        output.close();input.close();
        if(!ProcessingPipeline::fingerprint(t.temporary,t.digest,t.identity,error_)||t.digest!=t.sourceDigest){error_="Original backup verification failed; source preserved.";return false;}
        t.state="prepared";if(!save(t))return false;
    }
    if(cancelled(cancel)){error_="Original relocation paused.";return false;}
    if(t.state=="prepared"){
        ProcessingTask output;output.outputPath=t.target;output.temporaryPath=t.temporary;output.digest=t.digest;output.identity=t.identity;
        auto result=ProcessingPipeline::recover(output,cancel);
        if(result.status!="completed"&&t.restoreOf.isEmpty()&&QFileInfo::exists(t.target)&&QFileInfo::exists(t.temporary)){
            t.target=freePath(QFileInfo(t.target).absolutePath(),QFileInfo(t.target).fileName());if(t.target.isEmpty()||!save(t))return false;
            output.outputPath=t.target;result=ProcessingPipeline::publish(output,cancel);
        }
        if(result.status!="completed"){error_="Original backup could not be finalized: "+result.error;return false;}
        t.identity=result.identity;t.state="published";if(!save(t))return false;
    }
    if(cancelled(cancel)){error_="Original relocation paused.";return false;}
    if(t.state=="published"){
        if(!ProcessingPipeline::removeWithBackup(t.source,t.sourceDigest,t.sourceIdentity,t.target,t.digest,t.identity,error_))return false;
        return complete(t);
    }
    error_="Invalid original relocation journal.";return false;
}
bool SourceMover::moveOriginals(const ProcessingTask& task,const CancelToken& cancel){
    if(!error_.isEmpty())return false;
    const auto directory=QDir(task.directory).filePath("Originals");const QFileInfo folder(directory);
    if(folder.isSymLink()
#ifdef Q_OS_WIN
        ||folder.isJunction()
#endif
        ||!QDir().mkpath(directory)){error_="Cannot create an unlinked Originals folder.";return false;}
    for(const auto& logical:task.photo.sources){
        auto existing=transfers("operation=? AND logical=? AND restore_of='' AND restored=0 ORDER BY rowid DESC LIMIT 1",{task.operationId,logical});if(!error_.isEmpty())return false;
        Transfer t;
        if(!existing.isEmpty())t=existing.first();
        else {t.id=newId();t.operation=task.operationId;t.logical=logical;t.source=resolve(logical);if(!error_.isEmpty())return false;
            // Refreshes reuse the earlier original relocation.
            if(QFileInfo(t.source).absolutePath()==QDir(directory).absolutePath())continue;
            t.target=freePath(directory,QFileInfo(logical).fileName());t.temporary=QDir(directory).filePath(".suresort-original-"+t.id+".part");
            if(t.target.isEmpty()||!ProcessingPipeline::fingerprint(t.source,t.sourceDigest,t.sourceIdentity,error_)||!save(t))return false;
        }
        if(!run(t,cancel))return false;
    }return true;
}
bool SourceMover::restoreOriginals(const QString& operation,const CancelToken& cancel){
    if(!error_.isEmpty())return false;
    if(!exec("UPDATE file_moves SET undo_requested=1 WHERE operation=? AND restore_of='' AND restored=0",{operation}))return false;
    const auto originals=transfers("operation=? AND restore_of='' AND restored=0 ORDER BY rowid DESC",{operation});
    for(auto original:originals){
        if(cancelled(cancel)){error_="Restoring originals paused.";return false;}
        const auto reverse=transfers("restore_of=? ORDER BY rowid DESC LIMIT 1",{original.id});if(!error_.isEmpty())return false;
        if(!reverse.isEmpty()){auto t=reverse.first();if(!run(t,cancel))return false;continue;}
        QByteArray sourceHash,sourceId;QString sourceError;
        const bool sourceExists=QFileInfo::exists(original.source);
        const bool sourceUnchanged=sourceExists&&ProcessingPipeline::fingerprint(original.source,sourceHash,sourceId,sourceError)&&sourceHash==original.sourceDigest&&sourceId==original.sourceIdentity;
        if(original.state=="planned"){
            if(!sourceUnchanged){error_="Original path changed while undoing; preserved: "+original.source;return false;}
            original.restored=true;if(!save(original))return false;continue;
        }
        // Recover a rename that finished just before the prepared-state commit.
        if(original.state=="prepared"&&QFileInfo::exists(original.target)){
            ProcessingTask output;output.outputPath=original.target;output.temporaryPath=original.temporary;output.digest=original.digest;output.identity=original.identity;
            auto recovered=ProcessingPipeline::recover(output,cancel);
            if(recovered.status=="completed"){original.identity=recovered.identity;original.state="published";if(!save(original))return false;}
            else if(!sourceUnchanged){error_=recovered.error;return false;}
        }
        if(sourceUnchanged){
            if(original.state=="published"||original.state=="complete"){
                if(!ProcessingPipeline::removeWithBackup(original.target,original.digest,original.identity,original.source,original.sourceDigest,original.sourceIdentity,error_))return false;
            }else if(original.state=="prepared"&&!ProcessingPipeline::removeOwned(original.temporary,original.digest,original.identity,error_))return false;
            original.restored=true;if(!save(original)||!exec("INSERT OR REPLACE INTO source_locations VALUES(?,?,?,?)",{original.logical,original.source,original.sourceDigest,original.sourceIdentity}))return false;continue;
        }
        if(sourceExists||QFileInfo(original.source).isSymLink()){error_="Undo destination is occupied or changed; originals preserved: "+original.source;return false;}
        QByteArray targetHash,targetId;if(!ProcessingPipeline::fingerprint(original.target,targetHash,targetId,error_)||targetHash!=original.digest||targetId!=original.identity){error_="Moved original changed or became unavailable; preserved: "+original.target;return false;}
        if(!QDir().mkpath(QFileInfo(original.source).absolutePath())){error_="Cannot restore the original folder.";return false;}
        Transfer restore;restore.id=newId();restore.operation="undo:"+operation;restore.restoreOf=original.id;restore.logical=original.logical;restore.source=original.target;restore.target=original.source;restore.sourceDigest=original.digest;restore.sourceIdentity=original.identity;restore.temporary=QDir(QFileInfo(restore.target).absolutePath()).filePath(".suresort-original-"+restore.id+".part");
        if(!save(restore)||!run(restore,cancel))return false;
    }return error_.isEmpty();
}
}
