#include "destination_counter.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRunnable>
#include <QSet>

namespace photocull {
DestinationCounter::DestinationCounter(QObject* parent) : QObject(parent) {
    pool_.setMaxThreadCount(1);
    timer_.setSingleShot(true); timer_.setInterval(150);
    connect(&timer_, &QTimer::timeout, this, &DestinationCounter::scan);
    connect(&watcher_, &QFileSystemWatcher::directoryChanged, this, [this] { refresh(); });
}
DestinationCounter::~DestinationCounter() { shutdown(); }
void DestinationCounter::shutdown() {
    stopped_ = true; timer_.stop();
    if (cancel_) cancel_->store(true);
    pool_.clear(); pool_.waitForDone();
}
void DestinationCounter::setFolders(const QHash<QString, QString>& folders, const QString& outputRoot) {
    if (folders_ == folders && outputRoot_ == outputRoot) { refresh(); return; }
    ++generation_;
    if (cancel_) cancel_->store(true);
    folders_ = folders; outputRoot_ = outputRoot;
    watchFolders(); refresh();
}
void DestinationCounter::refresh() {
    if (!stopped_ && !timer_.isActive()) timer_.start();
}
void DestinationCounter::watchFolders() {
    QSet<QString> wanted;
    auto watchAncestor = [&wanted](QString path) {
        while (!path.isEmpty()) {
            const QFileInfo info(path);
            if (info.isDir()) { wanted.insert(info.absoluteFilePath()); break; }
            const auto parent = info.absolutePath(); if (parent == path) break;
            path = parent;
        }
    };
    if (!outputRoot_.isEmpty()) watchAncestor(outputRoot_);
    for (const auto& path : folders_) watchAncestor(path);
    const auto current = watcher_.directories();
    QStringList remove, add;
    for (const auto& path : current) if (!wanted.contains(path)) remove.append(path);
    for (const auto& path : wanted) if (!current.contains(path)) add.append(path);
    if (!remove.isEmpty()) watcher_.removePaths(remove);
    if (!add.isEmpty()) watcher_.addPaths(add);
}
void DestinationCounter::scan() {
    if (stopped_) return;
    if (running_) { pending_ = true; return; }
    running_ = true; pending_ = false;
    cancel_ = std::make_shared<std::atomic_bool>(false);
    const auto token = cancel_; const auto generation = generation_; const auto folders = folders_;
    pool_.start(QRunnable::create([this, token, generation, folders] {
        QHash<QString, int> counts;
        for (auto folder = folders.cbegin(); folder != folders.cend() && !token->load(); ++folder) {
            const QFileInfo info(folder.value());
            if (!info.exists()) { counts[folder.key()] = 0; continue; }
            if (!info.isDir() || !info.isReadable()) { counts[folder.key()] = -1; continue; }
            QSet<QString> photos;
            QDirIterator files(folder.value(), QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot);
            while (!token->load() && files.hasNext()) {
                files.next(); const auto file = files.fileInfo(); const auto extension = file.suffix().toLower();
                if (isSupportedExtension(extension))
                    photos.insert((extension == "png" ? "png:" : "photo:") + file.completeBaseName().toCaseFolded());
            }
            counts[folder.key()] = int(photos.size());
        }
        QMetaObject::invokeMethod(this, [this, token, generation, counts] {
            running_ = false;
            if (stopped_) return;
            if (!token->load() && generation == generation_) { watchFolders(); emit countsChanged(counts); }
            if (pending_ || generation != generation_) refresh();
        }, Qt::QueuedConnection);
    }));
}
}
