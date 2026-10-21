#pragma once

#include "core/weapon.h"

#include <cstddef>
#include <vector>

namespace st {

// Movimento lido do mouse (contagens brutas) com o instante em que chegou.
struct MouseSample {
    int dx = 0;
    int dy = 0;  // > 0 = mouse puxado para baixo
    double timeMs = 0.0;
};

struct ShotResult {
    Vec2 ideal;   // compensação ideal acumulada até este tiro (graus)
    Vec2 player;  // movimento acumulado do jogador até este tiro (graus)
    float error;  // distância entre os dois (graus)
};

struct SessionResult {
    Weapon weapon = Weapon::AK47;
    std::vector<ShotResult> shots;
    std::vector<Vec2> idealPath;
    float meanError = 0.0f;
    float score = 0.0f;  // 0..100
};

// Movimento ideal do mouse (acumulado, em graus) para anular os `count` primeiros tiros:
// kick para a direita -> mouse para a esquerda; kick para cima -> mouse para baixo.
std::vector<Vec2> IdealPath(const Pattern& pattern, std::size_t count);

// Uma rajada de treino. Só lê movimentos do mouse e compara com o padrão;
// não gera nenhuma entrada.
class TrainingSession {
public:
    enum class State { Idle, Running, Done };

    // Graus de rotação por contagem de mouse com sensibilidade 1.0 (m_yaw do CS).
    static constexpr float kDegreesPerCount = 0.022f;
    // Erro médio padrão (graus) a partir do qual a nota é 0.
    static constexpr float kScoreTolerance = 2.0f;

    void Start(Weapon weapon, const Pattern& pattern, float sensitivity, double nowMs,
               float scoreTolerance = kScoreTolerance);
    void AddMovement(int dx, int dy);
    void Tick(double nowMs);  // registra os tiros cujo instante já passou
    void Stop(double nowMs);
    void Reset();

    State GetState() const { return state_; }
    bool Running() const { return state_ == State::Running; }
    double StartMs() const { return startMs_; }
    std::size_t ShotsFired() const { return shots_.size(); }
    std::size_t ShotsTotal() const { return idealPath_.size(); }
    const std::vector<Vec2>& IdealPath() const { return idealPath_; }
    const std::vector<ShotResult>& Shots() const { return shots_; }
    Vec2 LivePlayer() const;
    SessionResult Result() const;

private:
    void RecordShot();

    State state_ = State::Idle;
    Weapon weapon_ = Weapon::AK47;
    float degPerCount_ = kDegreesPerCount;
    float scoreTolerance_ = kScoreTolerance;
    double startMs_ = 0.0;
    double intervalMs_ = 100.0;
    long long cumDx_ = 0;
    long long cumDy_ = 0;
    std::vector<Vec2> idealPath_;
    std::vector<ShotResult> shots_;
};

}  // namespace st
