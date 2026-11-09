#include "sorting_controller.h"
#include "source_mover.h"
#include <QDir>
#include <QFileInfo>
#include <QRunnable>
#include <QTimer>
#include <algorithm>
#include <QCollator>
#include <QDateTime>
#include <limits>

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
SortingController::SortingController(QObject* parent,PrepareFunction prepare,int retryDelayMs) : QObject(parent),prepare_(std::move(prepare)),retryDelayMs_(qMax(1,retryDelayMs)) {
    retryTimer_.setSingleShot(true);connect(&retryTimer_,&QTimer::timeout,this,&SortingController::dispatchRetries);
    pool_.setMaxThreadCount(1); pool_.setExpiryTimeout(10000); pool_.setThreadPriority(QThread::LowPriority);
    watermarkSaveTimer_.setSingleShot(true);watermarkSaveTimer_.setInterval(150);
    connect(&watermarkSaveTimer_,&QTimer::timeout,this,&SortingController::saveWatermark);
    editSaveTimer_.setSingleShot(true);editSaveTimer_.setInterval(350);
    connect(&editSaveTimer_,&QTimer::timeout,this,[this]{flushEdits();});
}
SortingController::~SortingController() { shutdown(); }
ProcessingTask SortingController::cancellationCleanup(const ProcessingTask& task) const {
    auto cleanup=cancelledCleanup(task);
    if(!task.predecessorPath.isEmpty())for(const auto& old:tasks_)
        if(old.id!=task.id&&old.operationId==task.operationId&&old.outputReady){cleanup.action="copy";break;}
    return cleanup;
}
bool SortingController::initialize(const QString& path) {
    QString error; SessionSnapshot snapshot;
    if (!store_.open(path,error) || !store_.load(snapshot,error)) { emit problem("Sorting unavailable: "+error); return false; }
    databasePath_=path;
    config_=snapshot.config; assignments_=snapshot.assignments; undo_=snapshot.undo;
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
        if (t.status=="retired"||(t.status=="preserved"&&t.action=="move")) work_.push_back({t,Operation::Cleanup});
        else if (assignments_.value(assignmentKey(t.photo.id,t.categoryId))==t.id) {
            if (t.status=="moving") work_.push_back({t,Operation::MoveOriginals});
            else if (t.status=="prepared") work_.push_back({t,Operation::Recover});
            else if (t.status=="queued" || t.status=="processing") {
                // An interrupted pre-journal temp cannot be proven owned. Preserve it and use a new one.
                if (QFileInfo::exists(t.temporaryPath)) t.temporaryPath=QDir(t.directory).filePath(".photocull-"+newId()+".part");
                t.status="queued"; t.outputPath.clear(); t.digest.clear(); t.identity.clear(); t.error.clear();
                if (!persist(t)) return false;
                work_.push_back({t,Operation::Prepare});
            }
        }
    }
    // Finish requested undo operations in reverse physical move order before
    // resuming jobs that may read the same original files.
    std::stable_sort(work_.begin(),work_.end(),[this](const Work& a,const Work& b){
        if((a.operation==Operation::Cleanup)!=(b.operation==Operation::Cleanup))return a.operation==Operation::Cleanup;
        if(a.operation==Operation::Cleanup&&b.operation==Operation::Cleanup)return store_.moveSequence(a.task.operationId)>store_.moveSequence(b.task.operationId);
        return false;
    });
    for(auto& work:work_)if(work.operation==Operation::Cleanup)for(const auto& id:assignments_)
        if(tasks_.value(id).operationId==work.task.operationId)work.task.action="copy";
    armRetryTimer();
    emit configChanged(); emit progressChanged();emit watermarkChanged(watermark_);
    if(watermark_.settings.enabled&&!watermark_.asset)emit problem("Saved watermark unavailable. Select a PNG or disable watermarking.");
    QTimer::singleShot(0,this,[this]{pump();});
    if(!pendingEdits_.isEmpty())editSaveTimer_.start();
    return true;
}
bool SortingController::configure(const DestinationConfig& config) {
    if (!ready_ || stopped_) return false;
    QString error=validateDestinationConfig(config);
    if (!error.isEmpty() || !store_.saveConfig(config,error)) { emit problem(error); return false; }
    config_=config; emit configChanged(); return true;
}
bool SortingController::renameDestination(const QString& id,const QString& name,QString& error){
    if(!ready_||stopped_){error="Sorting is unavailable.";return false;}
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
    t.id=newId();t.operationId=t.id; t.photo=photo; t.categoryId=categoryId; t.categoryName=categoryName; t.directory=directory;
    t.temporaryPath=QDir(directory).filePath(".photocull-"+t.id+".part");
    for(const auto& folder:config_.folders)if(folder.id==categoryId)t.action=folder.action;
    t.watermark=watermark_;
    t.edits=edits_.value(photo.id);t.output=output_;
    return t;
}
QString SortingController::assign(const Photo& photo,const QString& destinationId) {
    if (!ready_ || stopped_) return {};
    if(!flushEdits(photo.id))return {};
    const auto watermarkError=validateWatermark(watermark_);if(!watermarkError.isEmpty()){emit problem(watermarkError);return {};}
    for (const auto& d:config_.folders) if (d.id==destinationId) {
        const auto current=assignment(photo.id,d.id);
        const auto directory=QDir(config_.outputRoot).filePath(d.name);
        if (current.categoryId==d.id && current.directory==directory && current.status!="cancelled" && current.status!="failed" && current.watermark==watermark_ && current.edits==edits(photo.id) && current.output==output_ && current.action==d.action) {
            emit advanceRequested(); return current.id; // Same assignment is idempotent.
        }
        auto t=makeTask(photo,d.id,d.name,directory);
        if(current.status=="completed"&&current.categoryId==d.id&&current.directory==directory){
            // Pressing the same folder key before the edit debounce is also a
            // refresh: keep the completed image until the new one is prepared.
            t.predecessorPath=current.outputPath;t.predecessorDigest=current.digest;t.predecessorIdentity=current.identity;
        }
        if (!change(photo,t,true,false)) return {};
        emit advanceRequested(); return t.id;
    }
    emit problem("Configure a destination folder first."); return {};
}
bool SortingController::change(const Photo& photo,std::optional<ProcessingTask> created,bool pushUndo,bool popUndo,const QString& targetCategory) {
    const auto category=!targetCategory.isEmpty()?targetCategory:created?created->categoryId:assignment(photo.id).categoryId;
    const auto key=assignmentKey(photo.id,category),previous=assignments_.value(key);
    const auto previousTask=tasks_.value(previous);QString error;
    const auto actionId=created?created->operationId:previousTask.operationId;
    if(!store_.changeAssignment(photo,created?created->id:QString(),created?&*created:nullptr,previous,pushUndo,previous,popUndo,error,category,actionId)){
        emit problem("Action was not saved: "+error);return false;
    }
    if(pushUndo)undo_.append({photo,previous,actionId,category});
    if(popUndo&&!undo_.isEmpty())undo_.removeLast();
    const bool sameOperation=created&&created->operationId==previousTask.operationId;
    retire(previous,created&&!created->predecessorPath.isEmpty(),!sameOperation);
    assignments_.remove(key);
    photoAssignments_[photo.id].remove(category);
    if(photoAssignments_[photo.id].isEmpty())photoAssignments_.remove(photo.id);
    if(created){
        tasks_.insert(created->id,*created);taskOrder_.append(created->id);assignments_[assignmentKey(photo.id,created->categoryId)]=created->id;
        photoAssignments_[photo.id][created->categoryId]=created->id;
        if(pushUndo||latest_.value(photo.id)==previous||!latest_.contains(photo.id))latest_[photo.id]=created->id;
        work_.push_back({*created,Operation::Prepare});
    }else if(latest_.value(photo.id)==previous){
        latest_.remove(photo.id);for(auto it=taskOrder_.crbegin();it!=taskOrder_.crend();++it){const auto t=tasks_.value(*it);if(t.photo.id==photo.id&&assignments_.value(assignmentKey(photo.id,t.categoryId))==t.id){latest_[photo.id]=t.id;break;}}
    }
    emit assignmentChanged(photo.id);emit progressChanged();pump();return true;
}
void SortingController::retire(const QString& id,bool preserveOutput,bool restoreSources) {
    if(id.isEmpty())return;
    retryDue_.remove(id);
    auto& t=tasks_[id];t.automaticRetries=0;t.retryOperation=0;t.status=preserveOutput?"superseded":"retired";
    work_.erase(std::remove_if(work_.begin(),work_.end(),[&](const Work& w){return w.task.id==id;}),work_.end());
    if(active_&&active_->task.id==id&&active_->operation!=Operation::Cleanup)activeCancel_->store(true);
    else {auto cleanup=t;if(preserveOutput){cleanup.outputPath.clear();cleanup.predecessorPath.clear();}if(preserveOutput||!restoreSources)cleanup.action="copy";
        const auto before=std::find_if(work_.begin(),work_.end(),[](const Work& item){return item.operation!=Operation::Cleanup;});work_.insert(before,{cleanup,Operation::Cleanup});}
}
void SortingController::clearAssignment(const Photo& photo) {
    if(!flushEdits(photo.id))return;
    if (ready_ && !stopped_ && !assignment(photo.id).id.isEmpty()) change(photo,std::nullopt,true,false);
}
void SortingController::undo() {
    if (!ready_ || stopped_ || undo_.isEmpty()) return;
    if(!flushEdits())return;
    const auto entry=undo_.last();
    std::optional<ProcessingTask> restore;
    if (!entry.previousTask.isEmpty()) {
        const auto old=tasks_.value(entry.previousTask);
        restore=makeTask(entry.photo,old.categoryId,old.categoryName,old.directory);
        restore->watermark=old.watermark;
        restore->edits=old.edits;restore->output=old.output;restore->action=old.action;restore->operationId=old.operationId;
        QString error;if(!store_.saveEdits(entry.photo.id,old.edits,error)){emit problem(error);return;}
        edits_[entry.photo.id]=old.edits;emit editsChanged(entry.photo.id,old.edits);
    }
    if (change(entry.photo,restore,false,true,entry.categoryId)) emit focusRequested(entry.photo);
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
    if (!ready_ || stopped_) return;
    QVector<ProcessingTask> restores;for(const auto& t:tasks_)if(t.status=="preserved"&&t.action=="move")restores.append(t);
    std::stable_sort(restores.begin(),restores.end(),[this](const ProcessingTask& a,const ProcessingTask& b){return store_.moveSequence(a.operationId)>store_.moveSequence(b.operationId);});
    for(auto task:restores){retryDue_.remove(task.id);task.retryOperation=0;task.automaticRetries=0;if(!persist(task))return;const auto before=std::find_if(work_.begin(),work_.end(),[](const Work& item){return item.operation!=Operation::Cleanup;});work_.insert(before,{task,Operation::Cleanup});}
    const auto current=assignments_.values();
    for (const auto& id:current) {
        auto t=tasks_.value(id);
        retryDue_.remove(id);t.retryOperation=0;t.automaticRetries=0;
        if(t.status=="failed"&&t.action=="move"&&t.outputReady){
            t.status="moving";t.error.clear();if(persist(t))work_.push_back({t,Operation::MoveOriginals});continue;
        }
        if (t.status=="failed" || t.status=="cancelled"){
            auto retry=makeTask(t.photo,t.categoryId,t.categoryName,t.directory);retry.watermark=t.watermark;retry.edits=t.edits;retry.output=t.output;retry.action=t.action;retry.operationId=t.operationId;
            retry.predecessorPath=t.predecessorPath;retry.predecessorDigest=t.predecessorDigest;retry.predecessorIdentity=t.predecessorIdentity;
            change(t.photo,retry,false,false);
        }
    }
    pump();
}
void SortingController::cancelPending() {
    if (!ready_ || stopped_) return;
    const auto current=assignments_.values();
    for (const auto& id:current) {
        auto t=tasks_.value(id);
        if (!t.retryOperation && t.status!="queued" && t.status!="processing" && t.status!="prepared" && t.status!="moving") continue;
        retryDue_.remove(id);t.retryOperation=0;
        const bool savedMove=t.action=="move"&&t.outputReady&&!t.predecessorPath.isEmpty();
        t.status=savedMove?"failed":"cancelled";
        if(!savedMove)t.error.clear();
        if(savedMove)t.error="Original move paused after saving the processed image. Retry completes the move; Undo restores originals.";
        if (!persist(t)) break;
        work_.erase(std::remove_if(work_.begin(),work_.end(),[&](const Work& w){return w.task.id==id;}),work_.end());
        if (active_ && active_->task.id==id) activeCancel_->store(true);
        else if (!savedMove&&!t.digest.isEmpty()) work_.push_front({cancellationCleanup(t),Operation::Cleanup});
        emit assignmentChanged(t.photo.id);
    }
    emit progressChanged(); pump();
}
void SortingController::savePosition(const QString& folder,const QString& photoId) {
    if (!ready_ || stopped_) return;
    QString error;
    if (!store_.savePosition(folder,photoId,error)) emit problem("Cannot save browsing position: "+error);
}
ProcessingTask SortingController::assignment(const QString& photoId,const QString& categoryId) const {return tasks_.value(categoryId.isEmpty()?latest_.value(photoId):assignments_.value(assignmentKey(photoId,categoryId)));}
QVector<ProcessingTask> SortingController::assignments(const QString& photoId) const {QVector<ProcessingTask> result;for(const auto& id:photoAssignments_.value(photoId))result.append(tasks_.value(id));return result;}
Photo SortingController::resolvedPhoto(const Photo& photo) const {auto result=photo;result.sources=store_.resolveSources(photo.sources);return result;}
Catalog SortingController::mergeMovedPhotos(Catalog catalog,const QString& folder) const {
    const QDir root(folder);
    for(const auto& id:taskOrder_){const auto& t=tasks_[id];
        bool belongs=false;
        for(const auto& source:t.photo.sources){
            const auto relative=root.relativeFilePath(QFileInfo(source).absoluteFilePath());
            if(!QDir::isAbsolutePath(relative)&&relative!=".."&&!relative.startsWith("../")){belongs=true;break;}
        }
        if(!belongs||resolvedPhoto(t.photo).sources==t.photo.sources)continue;
        auto found=std::find_if(catalog.begin(),catalog.end(),[&](const Photo& photo){return photo.id==t.photo.id;});
        if(found==catalog.end())catalog.append(t.photo);
        else for(const auto& source:t.photo.sources)if(!found->sources.contains(source))found->sources.append(source);
    }
    QCollator collator(QLocale(QLocale::English,QLocale::UnitedStates));collator.setNumericMode(true);collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::stable_sort(catalog.begin(),catalog.end(),[&](const Photo& a,const Photo& b){const int order=collator.compare(a.name(),b.name());return order?order<0:a.displayPath()<b.displayPath();});return catalog;
}
int SortingController::remaining() const {
    int count=0;
    for (const auto& id:assignments_) { const auto s=tasks_.value(id).status; if(tasks_.value(id).retryOperation||s=="queued"||s=="processing"||s=="prepared"||s=="moving") ++count; }
    return count;
}
int SortingController::failed() const {
    int count=0;
    for (const auto& id:assignments_) if (tasks_.value(id).status=="failed"&&!tasks_.value(id).retryOperation) ++count;
    return count;
}
bool SortingController::setOutput(const OutputSettings& settings){
    if(!ready_||stopped_)return false;
    QString error=validateOutput(settings);
    if(!error.isEmpty()){emit problem(error);return false;}
    if(!store_.saveOutput(settings,error)){
        ready_=false;emit problem("Processing paused: output settings could not be saved: "+error);emit progressChanged();return false;
    }
    output_=settings;return true;
}
QString SortingController::processingStatus() const {
    int complete=0,processing=0,pending=0,failedCount=0;
    for(const auto& id:assignments_){const auto task=tasks_.value(id);const auto status=task.status;
        if(task.retryOperation)++pending;else if(status=="completed")++complete;else if(status=="processing"||status=="prepared"||status=="moving")++processing;
        else if(status=="queued")++pending;else if(status=="failed")++failedCount;
    }
    return QString("Completed: %1 | Processing: %2 | Pending: %3 | Failed: %4").arg(complete).arg(processing).arg(pending).arg(failedCount);
}
QStringList SortingController::issues() const {
    QStringList result;
    for (const auto& t:tasks_) if (!t.retryOperation && !t.error.isEmpty() && (t.status=="failed" || t.status=="preserved"||t.status=="completed"||t.status=="cancelled")) result.append(t.photo.name()+": "+t.error);
    return result;
}
bool SortingController::scheduleRetry(ProcessingTask& task,Operation operation) {
    if(task.automaticRetries>=2){task.retryOperation=0;task.error="Automatic retries exhausted (3 attempts): "+task.error;return false;}
    ++task.automaticRetries;
    task.retryOperation=operation==Operation::Cleanup?4:operation==Operation::MoveOriginals?3:operation==Operation::Prepare?1:2;
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
    std::stable_sort(due.begin(),due.end(),[this](const ProcessingTask& a,const ProcessingTask& b){
        if((a.retryOperation==4)!=(b.retryOperation==4))return a.retryOperation==4;
        return a.retryOperation==4&&store_.moveSequence(a.operationId)>store_.moveSequence(b.operationId);
    });
    for(auto task:due){
        const int retry=task.retryOperation;if(!retry)continue;
        const bool current=assignments_.value(assignmentKey(task.photo.id,task.categoryId))==task.id;
        if(retry!=4&&!current)continue;
        task.retryOperation=0;task.error.clear();Operation operation=Operation::Prepare;
        if(retry==4){operation=Operation::Cleanup;task.retryOperation=4;}
        else if(retry==3){operation=Operation::MoveOriginals;task.status="moving";}
        else if(retry==2){operation=Operation::Recover;task.status="prepared";}
        else{
            if(QFileInfo::exists(task.temporaryPath))task.temporaryPath=QDir(task.directory).filePath(".photocull-"+newId()+".part");
            task.status="queued";task.outputPath.clear();task.digest.clear();task.identity.clear();
        }
        if(!persist(task))return;
        if(operation==Operation::Cleanup){
            if(current&&task.status=="cancelled")task=cancellationCleanup(task);
            for(const auto& id:assignments_)if(tasks_.value(id).operationId==task.operationId&&id!=task.id)task.action="copy";
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
    work.task.readSources=store_.resolveSources(work.task.photo.sources);
    active_=work; activeCancel_=std::make_shared<std::atomic_bool>(false);
    const auto cancel=activeCancel_;
    pool_.start(QRunnable::create([this,work,cancel] {
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
            case Operation::MoveOriginals:{SourceMover mover(databasePath_);if(mover.moveOriginals(work.task,cancel)){result.status="completed";}else{result.status=cancel->load()?"cancelled":"failed";result.error=mover.error();}break;}
            case Operation::Cleanup:{
                bool restored=true;if(work.task.action=="move"){SourceMover mover(databasePath_);restored=mover.restoreOriginals(work.task.operationId,cancel);if(!restored){result.status="preserved";result.error="Original restore requires attention: "+mover.error();}}
                if(restored)result=ProcessingPipeline::cleanup(work.task);
                break;
            }
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
    result.action=expected.action;
    if(expected.status=="failed"&&expected.action=="move"&&expected.outputReady&&result.status=="cancelled"){
        result.status="failed";result.error=expected.error;
    }
    const bool current=assignments_.value(assignmentKey(result.photo.id,result.categoryId))==result.id;
    if(current&&expected.status=="cancelled"&&!result.predecessorPath.isEmpty()&&result.status=="completed"&&(result.action!="move"||result.outputReady)){
        // Publication already finished before cancellation reached the worker.
        if(!persist(result))return;
        emit assignmentChanged(result.photo.id);emit progressChanged();pump();return;
    }
    if (work.operation==Operation::Cleanup) {
        const bool fullCleanupPending=std::any_of(work_.begin(),work_.end(),[&](const Work& item){return item.operation==Operation::Cleanup&&item.task.id==result.id;});
        if(expected.status=="retired"&&fullCleanupPending)result=expected; // Preserve the durable full-undo request.
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
        for(const auto& id:assignments_)if(tasks_.value(id).operationId==result.operationId&&id!=result.id)cleanup.action="copy";
        work_.push_front({cleanup,Operation::Cleanup});
    } else {
        // Explicit cancellation is never restarted automatically.
        if(result.status=="failed"&&expected.status!="failed"&& !result.error.isEmpty())scheduleRetry(result,work.operation);
        else if(result.status=="failed"&&expected.status=="failed"&&work.operation==Operation::Prepare&&!result.error.isEmpty())scheduleRetry(result,work.operation);
        if(result.status=="completed"&&result.action=="move"&&!result.outputReady){
            result.outputReady=true;result.status="moving";if(!persist(result))return;work_.push_front({result,Operation::MoveOriginals});
        }else if(!persist(result))return;
        if (result.status=="prepared") work_.push_front({result,Operation::Publish}); // journal committed BEFORE publish
        if(result.status=="completed"&&edits_.contains(result.photo.id)&&result.edits!=edits(result.photo.id)){pendingEdits_[result.photo.id]=result.photo;editSaveTimer_.start();}
        if (!result.retryOperation&&!result.error.isEmpty()) emit problem(result.photo.name()+": "+result.error);
    }
    emit sourcesChanged(result.photo.id);emit assignmentChanged(result.photo.id); emit progressChanged(); pump();
}
void SortingController::shutdown() {
    if (stopped_) return;
    flushEdits();editSaveTimer_.stop();
    if(watermarkSaveTimer_.isActive()){watermarkSaveTimer_.stop();saveWatermark();}
    stopped_=true;retryTimer_.stop();retryDue_.clear();
    if (activeCancel_) activeCancel_->store(true);
    work_.clear(); pool_.waitForDone(); // Only after GUI event loop exits; journal recovers unfinished tasks.
}
bool SortingController::setWatermark(const WatermarkSnapshot& snapshot) {
    if(!ready_||stopped_)return false;
    const auto error=validateWatermark(snapshot);if(!error.isEmpty()){emit problem(error);return false;}
    watermark_=snapshot;emit watermarkChanged(watermark_);watermarkSaveTimer_.start();return true;
}
void SortingController::saveWatermark() {
    if(!ready_||stopped_)return;
    QString error;if(!store_.saveWatermark(watermark_,error)){ready_=false;emit problem("Could not save watermark settings: "+error);emit progressChanged();}
}
bool SortingController::setEdits(const Photo& photo,const PhotoEdits& parameters) {
    if(!ready_||stopped_||photo.id.isEmpty())return false;
    const auto error=validateEdits(parameters);if(!error.isEmpty()){emit problem(error);return false;}
    if(edits(photo.id)==parameters)return true;
    edits_[photo.id]=parameters;pendingEdits_[photo.id]=photo;editSaveTimer_.start();emit editsChanged(photo.id,parameters);return true;
}
bool SortingController::flushEdits(const QString& assigningPhoto) {
    if(pendingEdits_.isEmpty())return true;
    if(!ready_||stopped_)return false;
    editSaveTimer_.stop();const auto pending=pendingEdits_;
    QString error;
    for(auto it=pending.cbegin();it!=pending.cend();++it)if(!store_.saveEdits(it.key(),edits(it.key()),error)){
        ready_=false;emit problem("Edits could not be saved: "+error);return false;
    }
    pendingEdits_.clear();
    for(auto it=pending.cbegin();it!=pending.cend();++it)for(const auto& previous:assignments(it.key())){
        if(it.key()==assigningPhoto||previous.retryOperation||previous.status=="cancelled"||previous.status=="queued"||previous.status=="processing"||previous.status=="prepared"||previous.status=="moving"||previous.edits==edits(it.key()))continue;
        auto update=makeTask(it.value(),previous.categoryId,previous.categoryName,previous.directory);update.action=previous.action;update.operationId=previous.operationId;
        if(previous.status=="completed"||previous.outputReady){update.predecessorPath=previous.outputPath;update.predecessorDigest=previous.digest;update.predecessorIdentity=previous.identity;}
        else {update.predecessorPath=previous.predecessorPath;update.predecessorDigest=previous.predecessorDigest;update.predecessorIdentity=previous.predecessorIdentity;}
        if(!change(it.value(),update,false,false))return false;
    }
    return true;
}
bool SortingController::saveEditBindings(const QJsonArray& bindings) {
    if(!ready_||stopped_)return false;
    QString error;
    if(!store_.saveEditBindings(bindings,error)){emit problem(error);return false;}editBindings_=bindings;return true;
}
bool SortingController::saveUiPreferences(const QJsonObject& preferences) {
    if(!ready_||stopped_)return false;
    QString error;
    if(!store_.saveUiPreferences(preferences,error)){emit problem(error);return false;}
    uiPreferences_=preferences;return true;
}
}
