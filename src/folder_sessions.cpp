#include "sorting_controller.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QSqlQuery>
#include <algorithm>

namespace photocull {
namespace {
QByteArray fileDigest(const QString& path){QFile file(path);if(!file.open(QIODevice::ReadOnly))return {};QCryptographicHash hash(QCryptographicHash::Sha256);if(!hash.addData(&file))return {};return hash.result().toHex();}
bool copyDatabase(const QString& from,const QString& to){
    // Reading through SQLite first recovers any hot rollback journal before taking a snapshot.
    const auto connection="snapshot-"+newId();bool healthy=false;
    {auto database=QSqlDatabase::addDatabase("QSQLITE",connection);database.setDatabaseName(from);
        if(database.open()){QSqlQuery check(database);healthy=check.exec("PRAGMA quick_check")&&check.next()&&check.value(0).toString()=="ok";}
        database.close();}
    QSqlDatabase::removeDatabase(connection);if(!healthy)return false;
    QFile input(from);QSaveFile output(to);if(!input.open(QIODevice::ReadOnly)||!output.open(QIODevice::WriteOnly))return false;
    while(!input.atEnd()){const auto bytes=input.read(1024*1024);if(bytes.isEmpty()&&input.error()!=QFileDevice::NoError)return false;if(output.write(bytes)!=bytes.size())return false;}
    return output.commit();
}
bool canWriteProgress(const QString& folder,const QString& path){
    if(QFileInfo::exists(path)){QFile file(path);if(!file.open(QIODevice::ReadWrite))return false;}
    QTemporaryFile probe(QDir(folder).filePath(".suresort-write-XXXXXX"));return probe.open();
}
}
void SortingController::saveSessionIndex(){
    if(!sourceRoot_.isEmpty()||!ready_)return;
    QJsonArray folders;for(auto it=folderContexts_.cbegin();it!=folderContexts_.cend();++it)if(it.value().get()==folderContext_||it.value()->remaining()>0)folders.append(it.key());
    QString error;if(!store_.setMetadata("folder_sessions",QString::fromUtf8(QJsonDocument(folders).toJson(QJsonDocument::Compact)),error))emit problem(error);
}
bool SortingController::openFolderSession(const QString& folder,const std::function<bool()>& chooseLocal){
    if(stopped_||!ready_||!sourceRoot_.isEmpty())return false;
    folderUnavailable_=true;
    auto* previous=folderContext_;folderContext_=nullptr;
    if(previous){previous->flushEdits();previous->editSaveTimer_.stop();if(previous->watermarkSaveTimer_.isActive()){previous->watermarkSaveTimer_.stop();previous->saveWatermark();}}
    const QFileInfo folderInfo(folder);const auto root=folderInfo.canonicalFilePath();
    if(root.isEmpty()||!folderInfo.isDir()){emit problem("Cannot open the photo folder.");return false;}
    if(auto existing=folderContexts_.value(root)){
        folderContext_=existing.get();folderUnavailable_=false;emit configChanged();emit watermarkChanged(watermark());emit progressChanged();return existing->ready();
    }
    const auto sidecar=QDir(root).filePath(".suresort-progress.sqlite");
    QString key=root;
#ifdef Q_OS_WIN
    key=key.toCaseFolded();
#endif
    const auto hash=QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(),QCryptographicHash::Sha256).toHex());
    const auto localDirectory=databasePath_+".sessions";
    const auto local=QDir(localDirectory).filePath(hash+".sqlite"),basePath=local+".base";
    bool localOnly=!canWriteProgress(root,sidecar);QString selected=sidecar;
    // Hold both locks while comparing/copying closed, rollback-journal databases.
    QLockFile sideLock(sidecar+".lock"),localLock(local+".lock");sideLock.setStaleLockTime(0);localLock.setStaleLockTime(0);
    if(!localOnly&&!sideLock.tryLock(0)){emit problem("This folder's progress is already open in another Sure Sort instance.");folderContext_=nullptr;return false;}
    if(QFileInfo::exists(local)||localOnly){
        if(!QDir().mkpath(localDirectory)||!localLock.tryLock(0)){emit problem("Local folder progress is unavailable or already open.");folderContext_=nullptr;return false;}
        if(QFileInfo::exists(local)&&!localOnly){
            QFile base(basePath);QByteArray previous;if(base.open(QIODevice::ReadOnly))previous=base.readAll();
            const auto current=fileDigest(sidecar);bool useLocal=previous==current;
            if(!useLocal){
                if(!chooseLocal){emit problem("Local and folder progress both changed. Open the folder in the viewer to choose which to resume.");folderContext_=nullptr;return false;}
                useLocal=chooseLocal();
                const auto retained=(useLocal?sidecar:local)+".conflict-"+newId()+".sqlite";
                if(!copyDatabase(useLocal?sidecar:local,retained)){emit problem("Cannot preserve conflicting progress. Both sessions were left unchanged.");folderContext_=nullptr;return false;}
            }
            if(useLocal&&!copyDatabase(local,sidecar)){localOnly=true;}
            if(!localOnly){QFile::remove(local);QFile::remove(basePath);}
        }
        if(localOnly){
            selected=local;
            if(!QFileInfo::exists(local)){
                if(QFileInfo::exists(sidecar)&&!copyDatabase(sidecar,local)){emit problem("Cannot read saved folder progress.");folderContext_=nullptr;return false;}
                const auto digest=fileDigest(sidecar);
                QSaveFile base(basePath);if(!base.open(QIODevice::WriteOnly)||base.write(digest)!=digest.size()||!base.commit()){emit problem("Cannot save local progress origin.");folderContext_=nullptr;return false;}
            }
        }
    }
    sideLock.unlock();localLock.unlock();
    auto context=std::make_shared<SortingController>(nullptr,prepare_,retryDelayMs_);context->pool_=pool_;context->sourceRoot_=root;context->store_.setSourceRoot(root);
    auto* child=context.get();
    connect(child,&SortingController::configChanged,this,[this,child]{if(folderContext_==child)emit configChanged();});
    connect(child,&SortingController::assignmentChanged,this,[this,child](const QString& id){if(folderContext_==child)emit assignmentChanged(id);});
    connect(child,&SortingController::progressChanged,this,&SortingController::progressChanged);
    connect(child,&SortingController::problem,this,&SortingController::problem);
    connect(child,&SortingController::watermarkChanged,this,[this,child](WatermarkSnapshot value){if(folderContext_==child)emit watermarkChanged(value);});
    connect(child,&SortingController::editsChanged,this,[this,child](const QString& id,PhotoEdits value){if(folderContext_==child)emit editsChanged(id,value);});
    connect(child,&SortingController::advanceRequested,this,[this,child]{if(folderContext_==child)emit advanceRequested();});
    connect(child,&SortingController::focusRequested,this,[this,child](Photo photo){if(folderContext_==child)emit focusRequested(photo);});
    const bool initialized=context->initialize(selected);
    folderContexts_.insert(root,context);folderContext_=child;folderUnavailable_=false;
    if(!initialized){emit configChanged();return false;}
    context->uiPreferences_=uiPreferences_;
    if(context->store_.metadata("folder_initialized").isEmpty()){
        if(!config_.outputRoot.isEmpty())context->configure(config_);
        context->setOutput(output_);context->setWatermark(watermark_);context->saveEditBindings(editBindings_);
        QString error;if(!context->store_.setMetadata("folder_initialized","1",error)){context->ready_=false;emit problem(error);return false;}
    }
    if(localOnly)context->progressNotice_="Progress saved on this computer only.";
    savePosition(root,context->restoredPhoto_);saveSessionIndex();
    emit configChanged();emit watermarkChanged(watermark());emit progressChanged();return true;
}
void SortingController::registerCatalog(const Catalog& photos){
    if(folderContext_)return folderContext_->registerCatalog(photos);
    if(!ready_||stopped_)return;
    QString error;if(!store_.saveCatalog(photos,error)){emit problem(error);return;}
    // Missing outputs lose their completed state without discarding saved settings or history.
    for(const auto& id:assignments_){auto task=tasks_.value(id);if(task.status=="completed"&&!QFileInfo::exists(task.outputPath)){task.status="missing";task.error="Copied file is missing: "+task.outputPath;if(!persist(task))return;}}
    emit assignmentChanged({});
}
QStringList SortingController::excludedFolders() const {QStringList paths;for(const auto& folder:config().folders)paths.append(QDir(config().outputRoot).filePath(folder.name));return paths;}
bool SortingController::hasCompletedCopy(const QString& photoId,const QString& destinationId) const {
    for(const auto& task:assignments(photoId)){
        if(!destinationId.isEmpty()&&task.categoryId!=destinationId)continue;
        if((task.status=="completed"||task.removalRequested)&&!task.outputPath.isEmpty()&&QFileInfo::exists(task.outputPath))return true;
        if(!task.predecessorPath.isEmpty()&&QFileInfo::exists(task.predecessorPath))return true;
    }return false;
}
SortingController::DestinationToggle SortingController::toggleAssignment(const Photo& photo,const QString& destinationId){
    if(folderContext_)return folderContext_->toggleAssignment(photo,destinationId);
    if(!ready()||stopped_)return DestinationToggle::None;
    const auto current=assignment(photo.id,destinationId);
    if(current.removalRequested&&(current.status=="removing"||current.retryOperation))return DestinationToggle::None;
    const bool pending=!current.id.isEmpty()&&(current.status=="queued"||current.status=="processing"||current.status=="prepared"||current.retryOperation);
    if(hasCompletedCopy(photo.id,destinationId)||pending||current.removalRequested){
        removeAssignment(photo,destinationId);
        return assignment(photo.id,destinationId).removalRequested?DestinationToggle::RemovalRequested:DestinationToggle::None;
    }
    return assign(photo,destinationId).isEmpty()?DestinationToggle::None:DestinationToggle::CopyQueued;
}
bool SortingController::canUndo() const {
    if(folderContext_)return folderContext_->canUndo();
    for(const auto& task:tasks_)if(task.removalRequested&&(task.status=="removing"||task.retryOperation))return false;
    return !undo_.isEmpty();
}
void SortingController::removeAssignment(const Photo& photo,const QString& destinationId){
    if(folderContext_)return folderContext_->removeAssignment(photo,destinationId);
    if(!ready_||stopped_||!flushEdits(photo.id))return;
    auto task=assignment(photo.id,destinationId);if(task.id.isEmpty()||task.status=="removing")return;
    retryDue_.remove(task.id);task.removalRequested=true;task.status="removing";task.error.clear();task.retryOperation=0;task.automaticRetries=0;
    if(!persist(task))return;
    work_.erase(std::remove_if(work_.begin(),work_.end(),[&](const Work& work){return work.task.id==task.id;}),work_.end());
    if(active_&&active_->task.id==task.id){activeCancel_->store(true);if(active_->operation==Operation::Cleanup)work_.push_front({task,Operation::Cleanup});}
    else work_.push_front({task,Operation::Cleanup});
    emit assignmentChanged(photo.id);emit progressChanged();pump();
}
void SortingController::finishRemoval(ProcessingTask& task){
    if(task.status=="retired-clean"){
        change(task.photo,std::nullopt,true,false,task.categoryId);
    }
}
}
