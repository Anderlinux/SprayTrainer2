#pragma once

#include "core/training_session.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace st {

// Uma rajada concluída, guardada no histórico.
struct BurstRecord {
    std::int64_t timestamp = 0;  // segundos desde 1970 (UTC)
    Weapon weapon = Weapon::AK47;
    float score = 0.0f;
    float meanError = 0.0f;
    int goal = 0;  // meta ativa na hora (0 = nenhuma)
    std::vector<Vec2> ideal;   // um ponto por tiro disparado
    std::vector<Vec2> player;  // idem
};

BurstRecord MakeBurstRecord(const SessionResult& result, std::int64_t timestamp, int goal);

struct HistoryStats {
    int count = 0;
    float mean = 0.0f;
    float best = 0.0f;
    float recentMean = 0.0f;  // média das últimas `recent` rajadas
};

// Histórico das rajadas, mais antiga primeiro. Guarda no máximo kMaxEntries.
class History {
public:
    static constexpr std::size_t kMaxEntries = 300;

    void Add(BurstRecord record);
    void Clear();
    void Set(std::vector<BurstRecord> entries);  // ao carregar do disco

    const std::vector<BurstRecord>& Entries() const { return entries_; }

    // Índices (em Entries) das rajadas da arma, ou de todas.
    std::vector<std::size_t> Filter(std::optional<Weapon> weapon) const;
    HistoryStats Stats(std::optional<Weapon> weapon, std::size_t recent = 10) const;

private:
    std::vector<BurstRecord> entries_;
};

}  // namespace st
