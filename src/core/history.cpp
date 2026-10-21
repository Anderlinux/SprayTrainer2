#include "core/history.h"

#include <algorithm>

namespace st {

BurstRecord MakeBurstRecord(const SessionResult& result, std::int64_t timestamp, int goal) {
    BurstRecord r;
    r.timestamp = timestamp;
    r.weapon = result.weapon;
    r.score = result.score;
    r.meanError = result.meanError;
    r.goal = goal;
    r.ideal.reserve(result.shots.size());
    r.player.reserve(result.shots.size());
    for (const ShotResult& s : result.shots) {
        r.ideal.push_back(s.ideal);
        r.player.push_back(s.player);
    }
    return r;
}

void History::Add(BurstRecord record) {
    entries_.push_back(std::move(record));
    if (entries_.size() > kMaxEntries) {
        entries_.erase(entries_.begin(), entries_.begin() + static_cast<std::ptrdiff_t>(entries_.size() - kMaxEntries));
    }
}

void History::Clear() {
    entries_.clear();
}

void History::Set(std::vector<BurstRecord> entries) {
    entries_ = std::move(entries);
    if (entries_.size() > kMaxEntries) {
        entries_.erase(entries_.begin(), entries_.begin() + static_cast<std::ptrdiff_t>(entries_.size() - kMaxEntries));
    }
}

std::vector<std::size_t> History::Filter(std::optional<Weapon> weapon) const {
    std::vector<std::size_t> out;
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        if (!weapon || entries_[i].weapon == *weapon) {
            out.push_back(i);
        }
    }
    return out;
}

HistoryStats History::Stats(std::optional<Weapon> weapon, std::size_t recent) const {
    const std::vector<std::size_t> idx = Filter(weapon);
    HistoryStats s;
    s.count = static_cast<int>(idx.size());
    if (idx.empty()) {
        return s;
    }
    float sum = 0.0f;
    for (std::size_t i : idx) {
        sum += entries_[i].score;
        s.best = std::max(s.best, entries_[i].score);
    }
    s.mean = sum / static_cast<float>(idx.size());

    const std::size_t n = std::min(recent, idx.size());
    float recentSum = 0.0f;
    for (std::size_t k = idx.size() - n; k < idx.size(); ++k) {
        recentSum += entries_[idx[k]].score;
    }
    s.recentMean = recentSum / static_cast<float>(n);
    return s;
}

}  // namespace st
