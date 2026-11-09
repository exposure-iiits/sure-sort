#pragma once
#include "photo.h"
#include <QObject>
#include <QThreadPool>
#include <atomic>
#include <memory>

namespace photocull {
using CancelToken = std::shared_ptr<std::atomic_bool>;
struct ScanResult { Catalog photos; QString error; };
ScanResult scanFolder(const QString& folder, const CancelToken& cancel = {});

class FolderScanner : public QObject {
    Q_OBJECT
public:
    explicit FolderScanner(QObject* parent = nullptr);
    ~FolderScanner() override;
    void start(const QString& folder, quint64 epoch);
    void cancel();
    void shutdown();
signals:
    void finished(quint64 epoch, photocull::Catalog photos, QString error);
private:
    QThreadPool pool_;
    CancelToken cancel_;
};
}
