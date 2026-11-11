#pragma once
#include "image_cache.h"
#include <QThreadPool>
#include <functional>
#include <optional>
#include <deque>

namespace photocull {
class PreviewController : public QObject {
    Q_OBJECT
public:
    using Decoder = std::function<DecodedImage(const DecodeRequest&, const CancelToken&)>;
    explicit PreviewController(QObject* parent = nullptr, Decoder decoder = ImageDecoder::decode);
    ~PreviewController() override;
    void setCatalog(Catalog photos);
    void select(int index, QSize target);
    void requestDetail(bool enabled);
    void clear();
    void shutdown();
    void setCacheBudget(qint64 bytes) { cache_.setBudget(bytes); }
    const Catalog& catalog() const { return photos_; }
    int index() const { return index_; }
    quint64 generation() const { return generation_; }
    qint64 cacheBytes() const { return cache_.bytes(); }
    int runningCount() const { return int(running_.size()); }
    int pendingCount() const;
signals:
    void previewReady(quint64 generation, QString photoId, photocull::DecodedImage result);
    void detailReady(quint64 generation, QString photoId, photocull::DecodedImage result);
    void loadingChanged(bool loading);
private:
    enum class Kind { Preview, Detail, Prefetch };
    struct Job {
        quint64 serial = 0, epoch = 0, generation = 0;
        DecodeRequest request;
        Kind kind = Kind::Preview;
        CancelToken cancel;
    };
    Job makeJob(int index, Kind kind);
    void complete(quint64 serial, DecodedImage result);
    void pump();
    void prefetch();
    QVector<int> neighbors() const;
    bool hasRunning(const CacheKey& key, Kind kind) const;
    void cancelAll();
    Catalog photos_;
    int index_ = -1;
    int direction_ = 1;
    QSize target_;
    quint64 epoch_ = 0, generation_ = 0, serial_ = 0;
    bool detailWanted_ = false, previewLoaded_ = false, stopped_ = false;
    QThreadPool pool_;
    Decoder decoder_;
    ImageCache cache_;
    std::optional<Job> foreground_, detail_;
    std::deque<Job> prefetch_;
    QHash<quint64, Job> running_;
};
}
