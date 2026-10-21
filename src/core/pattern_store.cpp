#include "core/pattern_store.h"

namespace st {

PatternStore::PatternStore() {
    for (std::size_t i = 0; i < kWeaponCount; ++i) {
        patterns_[i] = DefaultPattern(static_cast<Weapon>(i));
    }
}

Pattern PatternStore::Get(Weapon w) const {
    std::lock_guard lock(mutex_);
    return patterns_[Index(w)];
}

void PatternStore::Set(Weapon w, const Pattern& p) {
    {
        std::lock_guard lock(mutex_);
        patterns_[Index(w)] = p;
    }
    version_.fetch_add(1, std::memory_order_release);
}

void PatternStore::SetPoint(Weapon w, std::size_t index, Vec2 value) {
    if (index >= kPatternPoints) {
        return;
    }
    {
        std::lock_guard lock(mutex_);
        patterns_[Index(w)][index] = value;
    }
    version_.fetch_add(1, std::memory_order_release);
}

void PatternStore::ResetToDefault(Weapon w) {
    Set(w, DefaultPattern(w));
}

}  // namespace st
