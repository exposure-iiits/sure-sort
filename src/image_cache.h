#pragma once
#include "image_decoder.h"
#include <QHash>
#include <list>

namespace photocull {
struct CacheKey {
    QString photoId;
    QSize target;
    bool operator==(const CacheKey& other) const { return photoId == other.photoId && target == other.target; }
};
inline size_t qHash(const CacheKey& key, size_t seed = 0) noexcept {
    return qHashMulti(seed, key.photoId, key.target.width(), key.target.height());
}
class ImageCache {
public:
    explicit ImageCache(qint64 budget = 256LL * 1024 * 1024) : budget_(qMax(0LL, budget)) {}
    bool get(const CacheKey& key, DecodedImage& result);
    bool contains(const CacheKey& key) const { return entries_.contains(key); }
    void insert(const CacheKey& key, const DecodedImage& result);
    void clear();
    void setBudget(qint64 bytes);
    qint64 bytes() const { return bytes_; }
    qint64 budget() const { return budget_; }
    qsizetype size() const { return entries_.size(); }
private:
    struct Entry { DecodedImage value; qint64 bytes; std::list<CacheKey>::iterator position; };
    void trim();
    QHash<CacheKey, Entry> entries_;
    std::list<CacheKey> lru_;
    qint64 bytes_ = 0;
    qint64 budget_;
};
}
