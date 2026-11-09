#include "image_cache.h"
namespace photocull {
bool ImageCache::get(const CacheKey& key, DecodedImage& result) {
    auto it = entries_.find(key);
    if (it == entries_.end()) return false;
    lru_.splice(lru_.begin(), lru_, it->position);
    result = it->value;
    return true;
}
void ImageCache::insert(const CacheKey& key, const DecodedImage& result) {
    if (result.image.isNull()) return;
    // Enforce target-sized admission even if a decoder is changed later.
    if (result.image.width() > key.target.width() || result.image.height() > key.target.height()) return;
    auto old = entries_.find(key);
    if (old != entries_.end()) {
        bytes_ -= old->bytes;
        lru_.erase(old->position);
        entries_.erase(old);
    }
    const qint64 cost = result.image.sizeInBytes();
    if (cost > budget_) return;
    lru_.push_front(key);
    entries_.insert(key, {result, cost, lru_.begin()});
    bytes_ += cost;
    trim();
}
void ImageCache::trim() {
    while (bytes_ > budget_ && !lru_.empty()) {
        auto it = entries_.find(lru_.back());
        bytes_ -= it->bytes;
        entries_.erase(it);
        lru_.pop_back();
    }
}
void ImageCache::clear() { entries_.clear(); lru_.clear(); bytes_ = 0; }
void ImageCache::setBudget(qint64 bytes) { budget_ = qMax(0LL, bytes); trim(); }
}
