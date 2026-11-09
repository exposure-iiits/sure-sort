#pragma once
#include "processing_pipeline.h"
#include <QSqlDatabase>
namespace photocull {
// Worker-only byte transfers, with a separate short-lived SQLite connection.
// Every destructive step follows a durable journal entry and a verified backup.
class SourceMover {
public:
    explicit SourceMover(const QString& databasePath);
    ~SourceMover();
    bool moveOriginals(const ProcessingTask&,const CancelToken&);
    bool restoreOriginals(const QString& operation,const CancelToken&);
    QString error() const {return error_;}
private:
    struct Transfer {
        QString id,operation,logical,source,target,temporary;
        QByteArray sourceDigest,sourceIdentity,digest,identity;
        QString state="planned",restoreOf;
        bool undoRequested=false,restored=false;
    };
    bool exec(const QString&,const QVariantList& args={});
    bool save(const Transfer&);
    QVector<Transfer> transfers(const QString&,const QVariantList&);
    bool run(Transfer&,const CancelToken&);
    bool complete(Transfer&);
    QString resolve(const QString&);
    QSqlDatabase db_;
    QString connection_,error_;
};
}
