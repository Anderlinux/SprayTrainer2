#pragma once

#include "core/weapon.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>

namespace st {

// Padrões das 3 armas, compartilhados entre UI, overlay e sessão de treino.
// Leituras devolvem cópia; o mutex só é segurado durante a cópia.
class PatternStore {
public:
    PatternStore();

    Pattern Get(Weapon w) const;
    void Set(Weapon w, const Pattern& p);
    void SetPoint(Weapon w, std::size_t index, Vec2 value);
    void ResetToDefault(Weapon w);

    // Incrementa a cada alteração; a UI usa para saber quando refazer caches.
    std::uint32_t Version() const { return version_.load(std::memory_order_acquire); }

private:
    mutable std::mutex mutex_;
    std::array<Pattern, kWeaponCount> patterns_{};
    std::atomic<std::uint32_t> version_{0};
};

}  // namespace st
