#pragma once
#include "sorting_types.h"
#include "folder_scanner.h"

namespace photocull {
// Original loading/development and encoding stay separate from preview decoding.
// Tone, rotation, crop and watermark run after development, before encoding.
class ProcessingPipeline {
public:
    static ProcessingTask prepare(ProcessingTask task, const CancelToken& cancel);
    static ProcessingTask publish(ProcessingTask task, const CancelToken& cancel);
    static ProcessingTask recover(ProcessingTask task, const CancelToken& cancel);
    static ProcessingTask cleanup(ProcessingTask task);
    static bool fingerprint(const QString& path, QByteArray& digest, QByteArray& identity, QString& error);
    static bool removeOwned(const QString& path, const QByteArray& digest, const QByteArray& identity, QString& error);
    static bool removeWithBackup(const QString& source,const QByteArray& sourceDigest,const QByteArray& sourceIdentity,
                                 const QString& backup,const QByteArray& backupDigest,const QByteArray& backupIdentity,QString& error);
};
}
