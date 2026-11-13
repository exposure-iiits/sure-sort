#include "sorting_controller.h"
#include <QDir>
#include <QFileInfo>
#include <QRunnable>
#include <QTimer>
#include <algorithm>
#include <QDateTime>
#include <limits>
#include <QJsonDocument>

namespace photocull {
namespace {
ProcessingTask cancelledCleanup(ProcessingTask task){
    // Cancelling an edit refresh keeps the last completed photograph.
    if(!task.predecessorPath.isEmpty()){
        if(task.outputPath==task.predecessorPath)task.outputPath.clear();
        task.predecessorPath.clear();task.predecessorDigest.clear();task.predecessorIdentity.clear();
    }
    return task;
}
}
SortingController::SortingController(QObject* parent,PrepareFunction prepare,int retryDelayMs) : QObject(parent),pool_(std::make_shared<QThreadPool>()),prepare_(std::move(prepare)),retryDelayMs_(qMax(1,retryDelayMs)) {
    retryTimer_.setSingleShot(true);connect(&retryTimer_,&QTimer::timeout,this,&SortingController::dispatchRetries);
    pool_->setMaxThreadCount(1); pool_->setExpiryTimeout(10000); pool_->setThreadPriority(QThread::LowPriority);
    watermarkSaveTimer_.setSingleShot(true);watermarkSaveTimer_.setInterval(150);
    connect(&watermarkSaveTimer_,&QTimer::timeout,this,&SortingController::saveWatermark);
    editSaveTimer_.setSingleShot(true);editSaveTimer_.setInterval(350);
    connect(&editSaveTimer_,&QTimer::timeout,this,[this]{flushEdits();});
}
SortingController::~SortingController() { shutdown(); }
ProcessingTask SortingController::cancellationCleanup(const ProcessingTask& task) const {
    return cancelledCleanup(task);
}
bool SortingController::initialize(const QString& path) {
    QString error; SessionSnapshot snapshot;
    if (!store_.open(path,error) || !store_.load(snapshot,error)) { emit problem("Sorting unavailable: "+error); return false; }
    databasePath_=path;
    config_=snapshot.config; assignments_=snapshot.assignments;
    restoredFolder_=snapshot.inputFolder; restoredPhoto_=snapshot.currentPhoto;
    watermark_=snapshot.watermark;output_=snapshot.output;
    edits_=snapshot.edits;editBindings_=snapshot.editBindings;
    uiPreferences_=snapshot.uiPreferences;
    for (const auto& t:snapshot.tasks){tasks_.insert(t.id,t);taskOrder_.append(t.id);
        if(assignments_.value(assignmentKey(t.photo.id,t.categoryId))==t.id){latest_[t.photo.id]=t.id;photoAssignments_[t.photo.id][t.categoryId]=t.id;}}
    ready_=true;
    for (auto t:snapshot.tasks) {
        if(t.retryOperation){retryDue_[t.id]=QDateTime::currentMSecsSinceEpoch()+retryDelayMs_;continue;}
        if(assignments_.value(assignmentKey(t.photo.id,t.categoryId))==t.id&&t.status=="completed"&&edits_.contains(t.photo.id)&&t.edits!=edits(t.photo.id))pendingEdits_[t.photo.id]=t.photo;
        if(t.removalRequested)work_.push_back({t,Operation::Cleanup});
        else if (t.status=="retired") work_.push_back({t,Operation::Cleanup});
        else if (assignments_.value(assignmentKey(t.photo.id,t.categoryId))==t.id) {
            if (t.status=="prepared") work_.push_back({t,Operation::Recover});
            else if (t.status=="queued" || t.status=="processing") {
                // An interrupted pre-journal temp cannot be proven owned. Preserve it and use a new one.
                if (QFileInfo::exists(t.temporaryPath)) t.temporaryPath=QDir(t.directory).filePath(".photocull-"+newId()+".part");
                t.status="queued"; t.outputPath.clear(); t.digest.clear(); t.identity.clear(); t.error.clear();
                if (!persist(t)) return false;
                work_.push_back({t,Operation::Prepare});
            }
        }
    }
    armRetryTimer();
    emit configChanged(); emit progressChanged();emit watermarkChanged(watermark_);
    if(watermark_.settings.enabled&&!watermark_.asset)emit problem("Saved watermark unavailable. Select a PNG or disable watermarking.");
    QTimer::singleShot(0,this,[this]{pump();});
    if(!pendingEdits_.isEmpty())editSaveTimer_.start();
    if(sourceRoot_.isEmpty()){const auto lastFolder=restoredFolder_,lastPhoto=restoredPhoto_;const auto folders=QJsonDocument::fromJson(store_.metadata("folder_sessions").toUtf8()).array();for(const auto& folder:folders)if(QFileInfo(folder.toString()).isDir())openFolderSession(folder.toString());folderContext_=nullptr;restoredFolder_=lastFolder;restoredPhoto_=lastPhoto;QString ignored;store_.savePosition(lastFolder,lastPhoto,ignored);}
    return true;
}
bool SortingController::configure(const DestinationConfig& config) {
    if(folderContext_){if(!folderContext_->configure(config))return false;config_=config;QString error;if(!store_.saveConfig(config,error))emit problem(error);return true;}
    if (!ready() || stopped_) return false;
    QString error=validateDestinationConfig(config);
    if(error.isEmpty()&&!sourceRoot_.isEmpty())for(const auto& destination:config.folders){const auto relative=QDir(QDir(config.outputRoot).filePath(destination.name)).relativeFilePath(sourceRoot_);if(!QDir::isAbsolutePath(relative)&&relative!=".."&&!relative.startsWith("../")){error="A sorting destination cannot contain the original photo folder.";break;}}
    if (!error.isEmpty() || !store_.saveConfig(config,error)) { emit problem(error); return false; }
    config_=config; emit configChanged(); return true;
}
bool SortingController::renameDestination(const QString& id,const QString& name,QString& error){
    if(folderContext_)return folderContext_->renameDestination(id,name,error);
    if(!ready()||stopped_){error="Sorting is unavailable.";return false;}
    if(active_||!work_.empty()||!retryDue_.isEmpty()||!pendingEdits_.isEmpty()){
        error="Wait for saving and automatic retries to finish before renaming a folder.";return false;
    }
    auto config=config_;bool found=false;for(auto& folder:config.folders)if(folder.id==id){folder.name=name;found=true;}
    error=found?validateDestinationConfig(config):"This sorting folder no longer exists.";if(!error.isEmpty())return false;
    if(!store_.renameDestination(id,name,error)){
        // A directory may already have moved if the final journal write failed.
        // Pause until startup reconciles the durable intent and reloads paths.
        if(store_.destinationRenamePending()){ready_=false;emit problem(error+" Restart Sure Sort to finish the folder rename.");emit progressChanged();}
        return false;
    }
    SessionSnapshot snapshot;if(!store_.load(snapshot,error)){ready_=false;emit problem(error);return false;}
    config_=snapshot.config;for(const auto& task:snapshot.tasks)tasks_[task.id]=task;
    emit configChanged();emit assignmentChanged({});emit progressChanged();return true;
}
ProcessingTask SortingController::makeTask(const Photo& photo,const QString& categoryId,const QString& categoryName,const QString& directory) {
    ProcessingTask t;
    t.id=newId(); t.photo=photo; t.categoryId=categoryId; t.categoryName=categoryName; t.directory=directory;
    t.temporaryPath=QDir(directory).filePath(".photocull-"+t.id+".part");
    t.watermark=watermark_;
    t.edits=edits_.value(photo.id);t.output=output_;
    return t;
}
QString SortingController::assign(const Photo& photo,const QString& destinationId) {
    if(folderContext_)return folderContext_->assign(photo,destinationId);
    if (!ready() || stopped_) return {};
    if(!flushEdits(photo.id))return {};
    const auto watermarkError=validateWatermark(watermark_);if(!watermarkError.isEmpty()){emit problem(watermarkError);return {};}
    for (const auto& d:config_.folders) if (d.id==destinationId) {
        const auto current=assignment(photo.id,d.id);
        if(current.removalRequested){emit problem("Wait for this copy to finish removing.");return {};}
        const auto directory=QDir(config_.outputRoot).filePath(d.name);
        if (current.categoryId==d.id && current.directory==directory && current.status!="cancelled" && current.status!="failed" && current.status!="missing" && !current.removalRequested && current.watermark==watermark_ && current.edits==edits(photo.id) && current.output==output_) {
            emit advanceRequested(); return current.id; // Same assignment is idempotent.
        }
        auto t=makeTask(photo,d.id,d.name,directory);
        if(current.status=="completed"&&current.categoryId==d.id&&current.directory==directory){
            // Pressing the same folder key before the edit debounce is also a
            // refresh: keep the completed image until the new one is prepared.
            t.predecessorPath=current.outputPath;t.predecessorDigest=current.digest;t.predecessorIdentity=current.identity;
        }
        if (!change(photo,t)) return {};
        latest_[photo.id]=t.id;
        emit advanceRequested(); return t.id;
    }
    emit problem("Configure a destination folder first."); return {};
}
bool SortingController::change(const Photo& photo,std::optional<ProcessingTask> created,const QString& targetCategory) {
    const auto category=!targetCategory.isEmpty()?targetCategory:created?created->categoryId:assignment(photo.id).categoryId;
    const auto key=assignmentKey(photo.id,category),previous=assignments_.value(key);
    QString error;
    if(!store_.changeAssignment(photo,created?created->id:QString(),created?&*created:nullptr,previous,error,category)){
        emit problem("Action was not saved: "+error);return false;
    }
    retire(previous,created&&!created->predecessorPath.isEmpty());
    assignments_.remove(key);
    photoAssignments_[photo.id].remove(category);
    if(photoAssignments_[photo.id].isEmpty())photoAssignments_.remove(photo.id);
    if(created){
        tasks_.insert(created->id,*created);taskOrder_.append(created->id);assignments_[assignmentKey(photo.id,created->categoryId)]=created->id;
        photoAssignments_[photo.id][created->categoryId]=created->id;
        if(latest_.value(photo.id)==previous||!latest_.contains(photo.id))latest_[photo.id]=created->id;
        work_.push_back({*created,Operation::Prepare});
    }else if(latest_.value(photo.id)==previous){
        latest_.remove(photo.id);for(auto it=taskOrder_.crbegin();it!=taskOrder_.crend();++it){const auto t=tasks_.value(*it);if(t.photo.id==photo.id&&assignments_.value(assignmentKey(photo.id,t.categoryId))==t.id){latest_[photo.id]=t.id;break;}}
    }
    emit assignmentChanged(photo.id);emit progressChanged();pump();return true;
}
void SortingController::retire(const QString& id,bool preserveOutput) {
    if(id.isEmpty())return;
    retryDue_.remove(id);
    auto& t=tasks_[id];t.removalRequested=false;t.automaticRetries=0;t.retryOperation=0;t.status=preserveOutput?"superseded":"retired";
    work_.erase(std::remove_if(work_.begin(),work_.end(),[&](const Work& w){return w.task.id==id;}),work_.end());
    if(active_&&active_->task.id==id&&active_->operation!=Operation::Cleanup)activeCancel_->store(true);
    else {auto cleanup=t;if(preserveOutput){cleanup.outputPath.clear();cleanup.predecessorPath.clear();}
        const auto before=std::find_if(work_.begin(),work_.end(),[](const Work& item){return item.operation!=Operation::Cleanup;});work_.insert(before,{cleanup,Operation::Cleanup});}
}
void SortingController::clearAssignment(const Photo& photo) {
    if(folderContext_)return folderContext_->clearAssignment(photo);
    removeAssignment(photo,assignment(photo.id).categoryId);
}
bool SortingController::persist(ProcessingTask& t) {
    QString error;
    if (!store_.saveTask(t,error)) {
        ready_=false; emit problem("Processing paused: cannot save session journal: "+error); return false;
    }
    auto stored=t;
    if(t.status=="completed"||t.status=="retired-clean"||t.status=="preserved"||t.status=="failed"||t.status=="cancelled")stored.watermark.asset.reset();
    tasks_.insert(t.id,stored); return true;
}
void SortingController::retryFailed() {
    if(folderContext_)return folderContext_->retryFailed();
    if (!ready() || stopped_) return;
    const auto current=assignments_.values();
    for (const auto& id:current) {
        auto t=tasks_.value(id);
        retryDue_.remove(id);t.retryOperation=0;t.automaticRetries=0;
        if(t.removalRequested){removeAssignment(t.photo,t.categoryId);continue;}
        if (t.status=="failed" || t.status=="cancelled"){
            auto retry=makeTask(t.photo,t.categoryId,t.categoryName,t.directory);retry.watermark=t.watermark;retry.edits=t.edits;retry.output=t.output;
            retry.predecessorPath=t.predecessorPath;retry.predecessorDigest=t.predecessorDigest;retry.predecessorIdentity=t.predecessorIdentity;
            change(t.photo,retry);
        }
    }
    pump();
}
void SortingController::cancelPending() {
    if(folderContext_)return folderContext_->cancelPending();
    if (!ready() || stopped_) return;
    const auto current=assignments_.values();
    for (const auto& id:current) {
        auto t=tasks_.value(id);
        if(t.removalRequested)continue;
        if (!t.retryOperation && t.status!="queued" && t.status!="processing" && t.status!="prepared") continue;
        retryDue_.remove(id);t.retryOperation=0;
        t.status="cancelled";t.error.clear();
        if (!persist(t)) break;
        work_.erase(std::remove_if(work_.begin(),work_.end(),[&](const Work& w){return w.task.id==id;}),work_.end());
        if (active_ && active_->task.id==id) activeCancel_->store(true);
        else if (!t.digest.isEmpty()) work_.push_front({cancellationCleanup(t),Operation::Cleanup});
        emit assignmentChanged(t.photo.id);
    }
    emit progressChanged(); pump();
}
void SortingController::savePosition(const QString& folder,const QString& photoId) {
    if(folderContext_)folderContext_->savePosition(folder,photoId);
    if (!ready() || stopped_) return;
    restoredFolder_=folder;restoredPhoto_=photoId;
    QString error;
    if (!store_.savePosition(folder,photoId,error)) emit problem("Cannot save browsing position: "+error);
}
ProcessingTask SortingController::assignment(const QString& photoId,const QString& categoryId) const {
    if(folderContext_)return folderContext_->assignment(photoId,categoryId);
    return tasks_.value(categoryId.isEmpty()?latest_.value(photoId):assignments_.value(assignmentKey(photoId,categoryId)));}
QVector<ProcessingTask> SortingController::assignments(const QString& photoId) const {
    if(folderContext_)return folderContext_->assignments(photoId);
    QVector<ProcessingTask> result;for(const auto& id:photoAssignments_.value(photoId))result.append(tasks_.value(id));return result;}
int SortingController::remaining() const {
    int count=0;
    for(const auto& context:folderContexts_)count+=context->remaining();
    for (const auto& id:assignments_) { const auto s=tasks_.value(id).status; if(tasks_.value(id).retryOperation||s=="removing"||s=="queued"||s=="processing"||s=="prepared") ++count; }
    return count;
}
int SortingController::failed() const {
    int count=0;for(const auto& context:folderContexts_)count+=context->failed();
    for (const auto& id:assignments_) if ((tasks_.value(id).status=="failed"||(tasks_.value(id).status=="preserved"&&tasks_.value(id).removalRequested))&&!tasks_.value(id).retryOperation) ++count;
    return count;
}
bool SortingController::setOutput(const OutputSettings& settings){
    if(folderContext_){if(!folderContext_->setOutput(settings))return false;output_=settings;QString error;if(!store_.saveOutput(settings,error))emit problem(error);return true;}
    if(!ready()||stopped_)return false;
    QString error=validateOutput(settings);
    if(!error.isEmpty()){emit problem(error);return false;}
    if(!store_.saveOutput(settings,error)){
        ready_=false;emit problem("Processing paused: output settings could not be saved: "+error);emit progressChanged();return false;
    }
    output_=settings;return true;
}
QString SortingController::processingStatus() const {
    if(folderContext_)return folderContext_->processingStatus();
    int complete=0,processing=0,pending=0,failedCount=0;
    for(const auto& id:assignments_){const auto task=tasks_.value(id);const auto status=task.status;
        if(task.retryOperation)++pending;else if(status=="completed")++complete;else if(status=="processing"||status=="prepared")++processing;
        else if(status=="queued")++pending;else if(status=="failed")++failedCount;
    }
    return QString("Completed: %1 | Processing: %2 | Pending: %3 | Failed: %4").arg(complete).arg(processing).arg(pending).arg(failedCount);
}
QStringList SortingController::issues() const {
    QStringList result;for(const auto& context:folderContexts_)result.append(context->issues());
    for (const auto& t:tasks_) if (!t.retryOperation && !t.error.isEmpty() && (t.status=="failed" || t.status=="preserved"||t.status=="completed"||t.status=="cancelled")) result.append(t.photo.name()+": "+t.error);
    return result;
}
bool SortingController::scheduleRetry(ProcessingTask& task,Operation operation) {
    if(task.automaticRetries>=2){task.retryOperation=0;task.error="Automatic retries exhausted (3 attempts): "+task.error;return false;}
    ++task.automaticRetries;
    task.retryOperation=operation==Operation::Cleanup?4:operation==Operation::Prepare?1:2;
    retryDue_[task.id]=QDateTime::currentMSecsSinceEpoch()+qint64(retryDelayMs_)*(task.automaticRetries==1?1:3);
    armRetryTimer();return true;
}
void SortingController::armRetryTimer() {
    if(stopped_||retryDue_.isEmpty()){retryTimer_.stop();return;}
    qint64 due=std::numeric_limits<qint64>::max();for(auto value:retryDue_)due=qMin(due,value);
    retryTimer_.start(int(qBound<qint64>(qint64(1),due-QDateTime::currentMSecsSinceEpoch(),qint64(60000))));
}
void SortingController::dispatchRetries() {
    if(stopped_||!ready_)return;
    const auto now=QDateTime::currentMSecsSinceEpoch();QVector<ProcessingTask> due;
    for(auto it=retryDue_.begin();it!=retryDue_.end();)if(it.value()<=now){due.append(tasks_.value(it.key()));it=retryDue_.erase(it);}else ++it;
    for(auto task:due){
        const int retry=task.retryOperation;if(!retry)continue;
        const bool current=assignments_.value(assignmentKey(task.photo.id,task.categoryId))==task.id;
        if(retry!=4&&!current)continue;
        task.retryOperation=0;task.error.clear();Operation operation=Operation::Prepare;
        if(retry==4){operation=Operation::Cleanup;task.retryOperation=4;}
        else if(retry==2){operation=Operation::Recover;task.status="prepared";}
        else{
            if(QFileInfo::exists(task.temporaryPath))task.temporaryPath=QDir(task.directory).filePath(".photocull-"+newId()+".part");
            task.status="queued";task.outputPath.clear();task.digest.clear();task.identity.clear();
        }
        if(!persist(task))return;
        if(operation==Operation::Cleanup){
            if(current&&task.status=="cancelled")task=cancellationCleanup(task);
            const auto before=std::find_if(work_.begin(),work_.end(),[](const Work& w){return w.operation!=Operation::Cleanup;});work_.insert(before,{task,operation});
        }else work_.push_back({task,operation});
    }
    armRetryTimer();emit progressChanged();pump();
}
void SortingController::pump() {
    if (stopped_ || !ready_ || active_ || work_.empty()) return;
    auto work=work_.front(); work_.pop_front();
    if (work.operation==Operation::Prepare) {
        if(work.task.watermark.settings.enabled&&!work.task.watermark.asset){QString error;store_.hydrateWatermark(work.task.watermark,error);}
        work.task.status="processing";
        if (!persist(work.task)) return;
    }
    active_=work; activeCancel_=std::make_shared<std::atomic_bool>(false);
    const auto cancel=activeCancel_;
    pool_->start(QRunnable::create([this,work,cancel] {
        ProcessingTask result=work.task;
        try {
            switch(work.operation) {
            case Operation::Prepare: result=prepare_(work.task,cancel); break;
            case Operation::Publish: result=ProcessingPipeline::publish(work.task,cancel); break;
            case Operation::Recover:
                result=ProcessingPipeline::recover(work.task,cancel);
                // Retry a publication collision without decoding the source again.
                if(result.status=="failed"&&work.task.automaticRetries>0&&work.task.outputPath!=work.task.predecessorPath&&QFileInfo::exists(work.task.outputPath)){
                    QByteArray digest,identity;QString error;
                    if(ProcessingPipeline::fingerprint(work.task.temporaryPath,digest,identity,error)&&digest==work.task.digest&&identity==work.task.identity){
                        auto retry=work.task;const QFileInfo path(retry.outputPath);
                        for(int n=2;n<10000;++n){const auto candidate=path.dir().filePath(path.completeBaseName()+"_"+QString::number(n)+"."+path.suffix());if(!QFileInfo::exists(candidate)){retry.outputPath=candidate;break;}}
                        // Journal the new path on the GUI thread before publishing it.
                        if(retry.outputPath!=work.task.outputPath){result=retry;result.status="prepared";result.error.clear();}
                    }
                }
                break;
            case Operation::Cleanup: result=ProcessingPipeline::cleanup(work.task);break;
            }
        } catch (...) { result.status="failed"; result.error="Processing task failed unexpectedly."; }
        QMetaObject::invokeMethod(this,[this,work,result=std::move(result)]() mutable {finished(work,std::move(result));},Qt::QueuedConnection);
    }));
    emit progressChanged();
}
void SortingController::finished(Work work,ProcessingTask result) {
    active_.reset(); activeCancel_.reset();
    if (stopped_) return;
    const auto expected=tasks_.value(result.id);
    result.retryOperation=0; // A completed attempt clears its durable in-flight cleanup marker.
    const bool current=assignments_.value(assignmentKey(result.photo.id,result.categoryId))==result.id;
    if(expected.removalRequested&&current){
        result.removalRequested=true;
        if(work.operation==Operation::Cleanup){
            if(std::any_of(work_.begin(),work_.end(),[&](const Work& next){return next.task.id==result.id&&next.operation==Operation::Cleanup;})){pump();return;}
            if(!result.error.isEmpty())scheduleRetry(result,Operation::Cleanup);
            if(!persist(result))return;
            finishRemoval(result);
        }else{result.status="removing";result.retryOperation=0;result.error.clear();if(!persist(result))return;work_.push_front({result,Operation::Cleanup});}
        emit assignmentChanged(result.photo.id);emit progressChanged();pump();return;
    }
    if(current&&expected.status=="cancelled"&&!result.predecessorPath.isEmpty()&&result.status=="completed"){
        // Publication already finished before cancellation reached the worker.
        if(!persist(result))return;
        emit assignmentChanged(result.photo.id);emit progressChanged();pump();return;
    }
    if (work.operation==Operation::Cleanup) {
        const bool fullCleanupPending=std::any_of(work_.begin(),work_.end(),[&](const Work& item){return item.operation==Operation::Cleanup&&item.task.id==result.id;});
        if(expected.status=="retired"&&fullCleanupPending)result=expected; // Preserve the durable full removal request.
        else if (current && expected.status=="cancelled"){
            result.status="cancelled";result.predecessorPath=expected.predecessorPath;
            result.predecessorDigest=expected.predecessorDigest;result.predecessorIdentity=expected.predecessorIdentity;
        }
        if(!fullCleanupPending&&!result.error.isEmpty())scheduleRetry(result,Operation::Cleanup);
        if (!persist(result)) return;
        if (!result.retryOperation&&!result.error.isEmpty()) emit problem(result.photo.name()+": "+result.error);
    } else if (!current || expected.status=="retired" || expected.status=="cancelled") {
        result.status=current?"cancelled":"retired";result.retryOperation=0;result.automaticRetries=0;
        if (!persist(result)) return;
        auto cleanup=current?cancellationCleanup(result):result;
        work_.push_front({cleanup,Operation::Cleanup});
    } else {
        // Explicit cancellation is never restarted automatically.
        if(result.status=="failed"&&expected.status!="failed"&& !result.error.isEmpty())scheduleRetry(result,work.operation);
        else if(result.status=="failed"&&expected.status=="failed"&&work.operation==Operation::Prepare&&!result.error.isEmpty())scheduleRetry(result,work.operation);
        if(!persist(result))return;
        if (result.status=="prepared") work_.push_front({result,Operation::Publish}); // journal committed BEFORE publish
        if(result.status=="completed"&&edits_.contains(result.photo.id)&&result.edits!=edits(result.photo.id)){pendingEdits_[result.photo.id]=result.photo;editSaveTimer_.start();}
        if (!result.retryOperation&&!result.error.isEmpty()) emit problem(result.photo.name()+": "+result.error);
    }
    if(current&&result.status=="completed"&&expected.status!="completed")emit copyCompleted(result.photo.name(),result.categoryName);
    emit assignmentChanged(result.photo.id); emit progressChanged(); pump();
}
void SortingController::shutdown() {
    if (stopped_) return;
    saveSessionIndex();
    for(auto& context:folderContexts_)context->shutdown();
    flushEdits();editSaveTimer_.stop();
    if(watermarkSaveTimer_.isActive()){watermarkSaveTimer_.stop();saveWatermark();}
    stopped_=true;retryTimer_.stop();retryDue_.clear();
    if (activeCancel_) activeCancel_->store(true);
    work_.clear(); pool_->waitForDone(); // Only after GUI event loop exits; journal recovers unfinished tasks.
}
bool SortingController::setWatermark(const WatermarkSnapshot& snapshot) {
    if(folderContext_){if(!folderContext_->setWatermark(snapshot))return false;watermark_=snapshot;watermarkSaveTimer_.start();return true;}
    if(!ready()||stopped_)return false;
    const auto error=validateWatermark(snapshot);if(!error.isEmpty()){emit problem(error);return false;}
    watermark_=snapshot;emit watermarkChanged(watermark_);watermarkSaveTimer_.start();return true;
}
void SortingController::saveWatermark() {
    if(!ready_||stopped_)return;
    QString error;if(!store_.saveWatermark(watermark_,error)){ready_=false;emit problem("Could not save watermark settings: "+error);emit progressChanged();}
}
bool SortingController::setEdits(const Photo& photo,const PhotoEdits& parameters) {
    if(folderContext_)return folderContext_->setEdits(photo,parameters);
    if(!ready()||stopped_||photo.id.isEmpty())return false;
    QString photoError;if(!store_.savePhoto(photo,photoError)){emit problem(photoError);return false;}
    const auto error=validateEdits(parameters);if(!error.isEmpty()){emit problem(error);return false;}
    if(edits(photo.id)==parameters)return true;
    edits_[photo.id]=parameters;pendingEdits_[photo.id]=photo;editSaveTimer_.start();emit editsChanged(photo.id,parameters);return true;
}
bool SortingController::flushEdits(const QString& assigningPhoto) {
    if(pendingEdits_.isEmpty())return true;
    if(!ready()||stopped_)return false;
    editSaveTimer_.stop();const auto pending=pendingEdits_;
    QString error;
    for(auto it=pending.cbegin();it!=pending.cend();++it)if(!store_.saveEdits(it.key(),edits(it.key()),error)){
        ready_=false;emit problem("Edits could not be saved: "+error);return false;
    }
    pendingEdits_.clear();
    for(auto it=pending.cbegin();it!=pending.cend();++it)for(const auto& previous:assignments(it.key())){
        if(it.key()==assigningPhoto||previous.retryOperation||previous.status=="cancelled"||previous.status=="queued"||previous.status=="processing"||previous.status=="prepared"||previous.edits==edits(it.key()))continue;
        auto update=makeTask(it.value(),previous.categoryId,previous.categoryName,previous.directory);
        if(previous.status=="completed"){update.predecessorPath=previous.outputPath;update.predecessorDigest=previous.digest;update.predecessorIdentity=previous.identity;}
        else {update.predecessorPath=previous.predecessorPath;update.predecessorDigest=previous.predecessorDigest;update.predecessorIdentity=previous.predecessorIdentity;}
        if(!change(it.value(),update))return false;
    }
    return true;
}
bool SortingController::saveEditBindings(const QJsonArray& bindings) {
    if(folderContext_){if(!folderContext_->saveEditBindings(bindings))return false;editBindings_=bindings;QString error;if(!store_.saveEditBindings(bindings,error))emit problem(error);return true;}
    if(!ready()||stopped_)return false;
    QString error;
    if(!store_.saveEditBindings(bindings,error)){emit problem(error);return false;}editBindings_=bindings;return true;
}
bool SortingController::saveUiPreferences(const QJsonObject& preferences) {
    if(!ready()||stopped_)return false;
    QString error;
    if(!store_.saveUiPreferences(preferences,error)){emit problem(error);return false;}
    uiPreferences_=preferences;for(auto& context:folderContexts_)context->uiPreferences_=preferences;return true;
}
}
