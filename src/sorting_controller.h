#pragma once
#include "processing_pipeline.h"
#include "session_store.h"
#include <QThreadPool>
#include <deque>
#include <optional>
#include <functional>
#include <QTimer>

namespace photocull {
class SortingController : public QObject {
    Q_OBJECT
public:
    using PrepareFunction = std::function<ProcessingTask(ProcessingTask,const CancelToken&)>;
    explicit SortingController(QObject* parent=nullptr, PrepareFunction prepare=ProcessingPipeline::prepare, int retryDelayMs=1000);
    ~SortingController() override;
    bool initialize(const QString& databasePath);
    bool configure(const DestinationConfig& config);
    bool renameDestination(const QString& id,const QString& name,QString& error);
    QString assign(const Photo& photo, const QString& destinationId);
    void clearAssignment(const Photo& photo);
    void undo();
    void retryFailed();
    void cancelPending();
    void savePosition(const QString& folder,const QString& photoId);
    void shutdown();
    bool setWatermark(const WatermarkSnapshot& snapshot);
    bool setOutput(const OutputSettings&);
    OutputSettings output() const {return output_;}
    QString processingStatus() const;
    const WatermarkSnapshot& watermark() const {return watermark_;}
    PhotoEdits edits(const QString& photoId) const {return edits_.value(photoId);}
    bool setEdits(const Photo& photo,const PhotoEdits& edits);
    QJsonArray editBindings() const {return editBindings_;}
    bool saveEditBindings(const QJsonArray& bindings);
    QJsonObject uiPreferences() const {return uiPreferences_;}
    bool saveUiPreferences(const QJsonObject& preferences);
    int editingStep() const {return qBound(1,uiPreferences_.value("editingStep").toInt(5),20);}
    bool ready() const { return ready_; }
    const DestinationConfig& config() const { return config_; }
    ProcessingTask assignment(const QString& photoId,const QString& categoryId={}) const;
    QVector<ProcessingTask> assignments(const QString& photoId) const;
    Photo resolvedPhoto(const Photo&) const;
    Catalog mergeMovedPhotos(Catalog,const QString& folder) const;
    bool canUndo() const {return !undo_.isEmpty();}
    int remaining() const;
    int failed() const;
    QStringList issues() const;
    QString restoredFolder() const { return restoredFolder_; }
    QString restoredPhoto() const { return restoredPhoto_; }
signals:
    void configChanged();
    void assignmentChanged(QString photoId);
    void progressChanged();
    void advanceRequested();
    void focusRequested(photocull::Photo photo);
    void problem(QString message);
    void watermarkChanged(photocull::WatermarkSnapshot snapshot);
    void editsChanged(QString photoId,photocull::PhotoEdits edits);
    void sourcesChanged(QString photoId);
private:
    enum class Operation { Prepare, Publish, Recover, Cleanup, MoveOriginals };
    struct Work { ProcessingTask task; Operation operation; };
    ProcessingTask makeTask(const Photo&,const QString& categoryId,const QString& categoryName,const QString& directory);
    bool change(const Photo&,std::optional<ProcessingTask> task,bool pushUndo,bool popUndo,const QString& categoryId={});
    void retire(const QString& id,bool preserveOutput=false,bool restoreSources=true);
    bool persist(ProcessingTask& task);
    ProcessingTask cancellationCleanup(const ProcessingTask&) const;
    void pump();
    bool scheduleRetry(ProcessingTask&,Operation);
    void dispatchRetries();
    void armRetryTimer();
    void finished(Work work,ProcessingTask result);
    void saveWatermark();
    bool flushEdits(const QString& assigningPhoto={});
    SessionStore store_;
    DestinationConfig config_;
    QHash<QString,ProcessingTask> tasks_;
    QHash<QString,QString> assignments_;
    QHash<QString,QHash<QString,QString>> photoAssignments_;
    QHash<QString,QString> latest_;
    QVector<QString> taskOrder_;
    QString databasePath_;
    QVector<UndoEntry> undo_;
    QString restoredFolder_,restoredPhoto_;
    QThreadPool pool_;
    PrepareFunction prepare_;
    std::deque<Work> work_;
    std::optional<Work> active_;
    CancelToken activeCancel_;
    bool ready_=false,stopped_=false;
    WatermarkSnapshot watermark_;
    QTimer watermarkSaveTimer_;
    QHash<QString,PhotoEdits> edits_;
    QHash<QString,Photo> pendingEdits_;
    QJsonArray editBindings_;
    QJsonObject uiPreferences_;
    QTimer editSaveTimer_;
    OutputSettings output_;
    QTimer retryTimer_;
    QHash<QString,qint64> retryDue_;
    int retryDelayMs_;
};
}
