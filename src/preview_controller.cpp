#include "preview_controller.h"
#include <QRunnable>
#include <QSet>

namespace photocull {
PreviewController::PreviewController(QObject* parent, Decoder decoder)
    : QObject(parent), decoder_(std::move(decoder)) {
    pool_.setMaxThreadCount(3); // Two neighbors, with one slot reserved for foreground/detail.
    pool_.setExpiryTimeout(10000);
}
PreviewController::~PreviewController() { shutdown(); }
int PreviewController::pendingCount() const {
    return int(foreground_.has_value()) + int(detail_.has_value()) + int(prefetch_.size());
}
void PreviewController::cancelAll() {
    for (auto& job : running_) job.cancel->store(true);
    foreground_.reset(); detail_.reset(); prefetch_.clear();
}
void PreviewController::clear() {
    ++epoch_; ++generation_;
    cancelAll(); cache_.clear(); photos_.clear(); index_ = -1;
    detailWanted_ = previewLoaded_ = false;
    emit loadingChanged(false);
}
void PreviewController::shutdown() {
    if (stopped_) return;
    stopped_ = true;
    cancelAll();
    pool_.waitForDone(); // Only at destruction / after the app event loop has ended.
}
void PreviewController::setCatalog(Catalog photos) { clear(); photos_ = std::move(photos); }
PreviewController::Job PreviewController::makeJob(int index, Kind kind) {
    Job job;
    job.serial = ++serial_; job.epoch = epoch_; job.generation = generation_; job.kind = kind;
    job.request = {photos_[index], target_, kind == Kind::Detail, kind == Kind::Preview};
    job.cancel = std::make_shared<std::atomic_bool>(false);
    return job;
}
bool PreviewController::hasRunning(const CacheKey& key, Kind kind) const {
    for (const auto& job : running_)
        if (job.epoch == epoch_ && !job.cancel->load() && job.request.photo.id == key.photoId &&
            job.request.target == key.target && (job.kind == Kind::Detail) == (kind == Kind::Detail)) return true;
    return false;
}
void PreviewController::select(int index, QSize target) {
    if (stopped_ || index < 0 || index >= photos_.size() || !target.isValid()) return;
    ++generation_;
    if (index_ >= 0 && index != index_) direction_ = index > index_ ? 1 : -1;
    index_ = index; target_ = target;
    detailWanted_ = previewLoaded_ = false;
    foreground_.reset(); detail_.reset(); prefetch_.clear();
    const CacheKey key{photos_[index_].id, target_};
    QSet<QString> wanted;
    for (int neighbor : neighbors()) wanted.insert(photos_[neighbor].id);
    bool adopted = false;
    for (auto& job : running_) {
        if (job.epoch == epoch_ && !job.cancel->load() && job.kind != Kind::Detail &&
            job.request.photo.id == key.photoId && job.request.target == target_) {
            // Explicitly reauthorize this useful in-flight request for the newest generation.
            job.generation = generation_; job.kind = Kind::Preview; adopted = true;
        } else if (job.epoch == epoch_ && !job.cancel->load() && job.kind != Kind::Detail &&
                   job.request.target == target_ && wanted.contains(job.request.photo.id)) {
            job.kind = Kind::Prefetch; // Keep useful work across consecutive selections.
        } else job.cancel->store(true);
    }
    DecodedImage cached;
    if (cache_.get(key, cached)) {
        previewLoaded_ = true;
        emit loadingChanged(false);
        emit previewReady(generation_, key.photoId, cached);
        prefetch();
    } else {
        emit loadingChanged(true);
        if (!adopted) foreground_ = makeJob(index_, Kind::Preview);
    }
    pump();
}
void PreviewController::requestDetail(bool enabled) {
    detailWanted_ = enabled;
    detail_.reset();
    for (auto& job : running_) if (job.kind == Kind::Detail) job.cancel->store(true);
    if (enabled && index_ >= 0) {
        // Free the second slot promptly if LibRaw is working on an obsolete neighbor.
        for (auto& job : running_) if (job.kind == Kind::Prefetch) job.cancel->store(true);
        detail_ = makeJob(index_, Kind::Detail);
        pump();
    }
}
QVector<int> PreviewController::neighbors() const {
    QVector<int> result;
    const qint64 estimate = qMax(1LL, qint64(target_.width()) * target_.height() * 4);
    const int count = int(qBound(0LL, cache_.budget() / estimate - 1, 6LL));
    for (int offset : {1, 2, 3, 4, -1, -2}) {
        const int next = index_ + offset * direction_;
        if (next >= 0 && next < photos_.size() && result.size() < count) result.append(next);
    }
    return result;
}
void PreviewController::prefetch() {
    if (!previewLoaded_ || index_ < 0 || cache_.budget() == 0) return;
    prefetch_.clear();
    for (int next : neighbors()) {
        const CacheKey key{photos_[next].id, target_};
        if (!cache_.contains(key) && !hasRunning(key, Kind::Prefetch)) prefetch_.push_back(makeJob(next, Kind::Prefetch));
    }
}
void PreviewController::pump() {
    if (stopped_) return;
    while (running_.size() < 3) {
        std::optional<Job> next;
        if (foreground_) { next = std::move(foreground_); foreground_.reset(); }
        else if (detail_) { next = std::move(detail_); detail_.reset(); }
        else if (previewLoaded_ && !prefetch_.empty()) {
            int backgroundActive = 0;
            for (const auto& job : running_) if (job.kind == Kind::Prefetch) ++backgroundActive;
            if (backgroundActive >= 2 || running_.size() >= 2) break;
            next = std::move(prefetch_.front()); prefetch_.pop_front();
        } else break;
        const auto job = *next;
        running_.insert(job.serial, job);
        pool_.start(QRunnable::create([this, job] {
            DecodedImage result;
            if (!job.cancel->load()) {
                try { result = decoder_(job.request, job.cancel); }
                catch (...) { result.error = "This photo could not be decoded."; }
            }
            QMetaObject::invokeMethod(this, [this, serial = job.serial, result = std::move(result)]() mutable {
                complete(serial, std::move(result));
            }, Qt::QueuedConnection);
        }), job.kind == Kind::Prefetch ? -1 : 1);
    }
}
void PreviewController::complete(quint64 serial, DecodedImage result) {
    const auto job = running_.take(serial);
    if (stopped_) return;
    if (job.epoch == epoch_ && !job.cancel->load()) {
        if (job.kind != Kind::Detail) cache_.insert({job.request.photo.id, job.request.target}, result);
        if (job.generation == generation_ && index_ >= 0 && job.request.photo.id == photos_[index_].id) {
            if (job.kind == Kind::Preview) {
                // A prefetch promoted to foreground did not permit expensive fallback.
                if (result.image.isNull() && !job.request.allowRawFallback) foreground_ = makeJob(index_, Kind::Preview);
                else {
                    previewLoaded_ = true;
                    emit loadingChanged(false);
                    emit previewReady(generation_, job.request.photo.id, result);
                    prefetch();
                }
            } else if (job.kind == Kind::Detail && detailWanted_)
                emit detailReady(generation_, job.request.photo.id, result);
        }
    }
    pump();
}
}
