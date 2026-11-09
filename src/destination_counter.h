#pragma once
#include "folder_scanner.h"
#include <QFileSystemWatcher>
#include <QHash>
#include <QThreadPool>
#include <QTimer>

namespace photocull {
// Counts real photos off the GUI thread; Originals and other subfolders are excluded.
class DestinationCounter : public QObject {
    Q_OBJECT
public:
    explicit DestinationCounter(QObject* parent = nullptr);
    ~DestinationCounter() override;
    void setFolders(const QHash<QString, QString>& folders, const QString& outputRoot);
    void refresh();
    void shutdown();
signals:
    void countsChanged(QHash<QString, int> counts);
private:
    void scan();
    void watchFolders();
    QHash<QString, QString> folders_;
    QString outputRoot_;
    QFileSystemWatcher watcher_;
    QThreadPool pool_;
    QTimer timer_;
    CancelToken cancel_;
    quint64 generation_ = 0;
    bool running_ = false, pending_ = false, stopped_ = false;
};
}
