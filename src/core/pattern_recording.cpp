#include "core/pattern_recording.h"

#include <algorithm>

namespace st {

namespace {

constexpr float kMaxDegrees = 10.0f;  // mesmo limite do editor e dos perfis

}  // namespace

Pattern PatternFromPath(const std::vector<Vec2>& path, const Pattern& base, int smoothing) {
    const std::size_t n = std::min(path.size(), kPatternPoints);
    if (n == 0) {
        return base;
    }

    // Posição acumulada do recuo que o desenho anula: mouse para a esquerda = kick para a direita.
    std::vector<Vec2> cum(n);
    for (std::size_t i = 0; i < n; ++i) {
        cum[i] = {-path[i].x, path[i].y};
    }

    // Média móvel centrada; o primeiro ponto fica fixo (é a mira inicial).
    const int r = std::clamp(smoothing, 0, 3);
    if (r > 0 && n > 2) {
        std::vector<Vec2> smoothed = cum;
        for (std::size_t i = 1; i < n; ++i) {
            const std::size_t lo = i >= static_cast<std::size_t>(r) ? i - static_cast<std::size_t>(r) : 1;
            const std::size_t hi = std::min(n - 1, i + static_cast<std::size_t>(r));
            Vec2 sum{};
            for (std::size_t k = lo; k <= hi; ++k) {
                sum.x += cum[k].x;
                sum.y += cum[k].y;
            }
            const auto count = static_cast<float>(hi - lo + 1);
            smoothed[i] = {sum.x / count, sum.y / count};
        }
        cum = std::move(smoothed);
    }

    Pattern out = base;
    Vec2 prev{};
    for (std::size_t i = 0; i < n; ++i) {
        // "+ 0.0f" troca -0 por 0 (a tabela mostraria "-0.000").
        out[i] = {std::clamp(cum[i].x - prev.x, -kMaxDegrees, kMaxDegrees) + 0.0f,
                  std::clamp(cum[i].y - prev.y, -kMaxDegrees, kMaxDegrees) + 0.0f};
        prev = cum[i];
    }
    return out;
}

}  // namespace st
