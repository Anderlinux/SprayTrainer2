#include "core/training_session.h"

#include <algorithm>
#include <cmath>

namespace st {

std::vector<Vec2> IdealPath(const Pattern& pattern, std::size_t count) {
    count = std::min(count, kPatternPoints);
    std::vector<Vec2> path;
    path.reserve(count);
    Vec2 acc{};
    for (std::size_t i = 0; i < count; ++i) {
        acc.x += pattern[i].x;
        acc.y += pattern[i].y;
        path.push_back({-acc.x, acc.y});
    }
    return path;
}

void TrainingSession::Start(Weapon weapon, const Pattern& pattern, float sensitivity, double nowMs,
                            float scoreTolerance) {
    const WeaponInfo& info = GetWeaponInfo(weapon);
    weapon_ = weapon;
    degPerCount_ = std::max(sensitivity, 0.01f) * kDegreesPerCount;
    scoreTolerance_ = std::max(scoreTolerance, 0.1f);
    startMs_ = nowMs;
    intervalMs_ = static_cast<double>(info.fireIntervalMs);
    cumDx_ = 0;
    cumDy_ = 0;
    shots_.clear();

    idealPath_ = st::IdealPath(pattern, static_cast<std::size_t>(info.magazine));
    shots_.reserve(idealPath_.size());
    state_ = State::Running;
}

void TrainingSession::AddMovement(int dx, int dy) {
    if (state_ != State::Running) {
        return;
    }
    cumDx_ += dx;
    cumDy_ += dy;
}

void TrainingSession::Tick(double nowMs) {
    if (state_ != State::Running) {
        return;
    }
    while (shots_.size() < idealPath_.size() &&
           nowMs >= startMs_ + static_cast<double>(shots_.size()) * intervalMs_) {
        RecordShot();
    }
    if (shots_.size() == idealPath_.size()) {
        state_ = State::Done;
    }
}

void TrainingSession::Stop(double nowMs) {
    Tick(nowMs);
    if (state_ == State::Running) {
        state_ = State::Done;
    }
}

void TrainingSession::Reset() {
    state_ = State::Idle;
    shots_.clear();
    idealPath_.clear();
    cumDx_ = 0;
    cumDy_ = 0;
}

Vec2 TrainingSession::LivePlayer() const {
    return {static_cast<float>(cumDx_) * degPerCount_, static_cast<float>(cumDy_) * degPerCount_};
}

void TrainingSession::RecordShot() {
    const Vec2 ideal = idealPath_[shots_.size()];
    const Vec2 player = LivePlayer();
    const float error = std::hypot(player.x - ideal.x, player.y - ideal.y);
    shots_.push_back({ideal, player, error});
}

SessionResult TrainingSession::Result() const {
    SessionResult r;
    r.weapon = weapon_;
    r.shots = shots_;
    r.idealPath = idealPath_;
    if (!shots_.empty()) {
        float sum = 0.0f;
        for (const ShotResult& s : shots_) {
            sum += s.error;
        }
        r.meanError = sum / static_cast<float>(shots_.size());
        r.score = std::clamp(100.0f * (1.0f - r.meanError / scoreTolerance_), 0.0f, 100.0f);
    }
    return r;
}

}  // namespace st
