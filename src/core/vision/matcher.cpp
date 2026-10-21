#include "core/vision/matcher.h"

#include <algorithm>
#include <cmath>

namespace st {

namespace {

// Maior potência de 2 que reduz `side` para um valor PAR <= maxSide. O divisor tem
// de ser par: a região capturada pode ter lado ímpar (ex.: 97) e o tamanho dela é
// derivado como refW*factor — só com factor par esse produto reproduz o original.
int DownscaleFactor(int side, int maxSide) {
    int factor = 2;
    while (side / factor > maxSide && side / (factor * 2) > 0) {
        factor *= 2;
    }
    return factor;
}

// Reduz por blocos quadrados de `factor` x `factor` tirando a média.
ImageRgb BoxDownscale(const ImageRgb& in, int factor) {
    if (in.Empty()) {
        return ImageRgb{};
    }
    if (factor <= 1) {
        return in;
    }
    ImageRgb out;
    out.width = in.width / factor;
    out.height = in.height / factor;
    if (out.width <= 0 || out.height <= 0) {
        return ImageRgb{};
    }
    out.rgb.resize(out.SizeBytes());
    const std::size_t n = static_cast<std::size_t>(factor) * static_cast<std::size_t>(factor);
    for (int y = 0; y < out.height; ++y) {
        for (int x = 0; x < out.width; ++x) {
            unsigned sum[3] = {0, 0, 0};
            for (int dy = 0; dy < factor; ++dy) {
                const std::size_t rowBase = static_cast<std::size_t>(y * factor + dy) *
                                            static_cast<std::size_t>(in.width) * 3u;
                for (int dx = 0; dx < factor; ++dx) {
                    const std::size_t p = rowBase + static_cast<std::size_t>(x * factor + dx) * 3u;
                    sum[0] += in.rgb[p];
                    sum[1] += in.rgb[p + 1u];
                    sum[2] += in.rgb[p + 2u];
                }
            }
            const std::size_t d = (static_cast<std::size_t>(y) * static_cast<std::size_t>(out.width) +
                                   static_cast<std::size_t>(x)) * 3u;
            for (std::size_t c = 0; c < 3u; ++c) {
                // Truncamento simples (sem "+ n/2"): template e região passam pelo
                // mesmo caminho, então o viés cancela na comparação — ver image.cpp.
                out.rgb[d + c] = static_cast<std::uint8_t>(sum[c] / n);
            }
        }
    }
    return out;
}

}  // namespace

void TemplateMatcher::Build(const ImageRgb& referenceSize, const std::vector<WeaponTemplate>& templates) {
    templates_.clear();
    factor_ = 1;
    refW_ = 0;
    refH_ = 0;
    if (referenceSize.Empty()) {
        return;
    }
    factor_ = DownscaleFactor(std::max(referenceSize.width, referenceSize.height), kMaxSide);
    const int rw = referenceSize.width / factor_;
    const int rh = referenceSize.height / factor_;
    if (rw <= 0 || rh <= 0) {
        return;
    }
    refW_ = rw;
    refH_ = rh;

    for (const WeaponTemplate& t : templates) {
        if (t.weaponIndex < 0 || t.image.Empty()) {
            continue;
        }
        const ImageRgb scaled = BoxDownscale(t.image, factor_);
        if (scaled.Empty() || scaled.width > refW_ || scaled.height > refH_) {
            continue;  // template maior que a região: impossível de casar
        }
        templates_.push_back({t.weaponIndex, scaled});
    }
}

float TemplateMatcher::Confidence(float sad) {
    // Ver o comentário no header: curva logarítmica ancorada em sad=32 -> 0.5.
    const double s = sad < 0.0 ? 0.0 : static_cast<double>(sad);
    const double c = 1.0 / (1.0 + s / 32.0);
    return static_cast<float>(c < 0.0 ? 0.0 : (c > 1.0 ? 1.0 : c));
}

std::size_t TemplateMatcher::PixelDiffCount(const ImageRgb& a, const ImageRgb& b) {
    if (a.width != b.width || a.height != b.height || a.Empty() || b.Empty()) {
        return 0;
    }
    std::size_t diff = 0;
    for (std::size_t i = 0; i < a.rgb.size(); i += 3u) {
        if (a.rgb[i] != b.rgb[i] || a.rgb[i + 1u] != b.rgb[i + 1u] || a.rgb[i + 2u] != b.rgb[i + 2u]) {
            ++diff;
        }
    }
    return diff;
}

TemplateMatcher::Hit TemplateMatcher::Match(int weaponIndex, const ImageRgb& region) const {
    Hit miss;
    if (refW_ <= 0 || templates_.empty() || region.Empty()) {
        return miss;
    }
    const Entry* entry = nullptr;
    for (const Entry& e : templates_) {
        if (e.weaponIndex == weaponIndex) {
            entry = &e;
            break;
        }
    }
    if (entry == nullptr) {
        return miss;
    }
    return MatchImpl(*entry, region);
}

TemplateMatcher::Hit TemplateMatcher::MatchBySlot(int slot, const ImageRgb& region) const {
    if (slot < 0 || static_cast<std::size_t>(slot) >= templates_.size()) {
        return Hit{};
    }
    return MatchImpl(templates_[static_cast<std::size_t>(slot)], region);
}

// Núcleo comum: desloca `entry.image` sobre a região reduzida e devolve a
// melhor batida. Separado de Match/MatchBySlot para não duplicar o laço SAD.
TemplateMatcher::Hit TemplateMatcher::MatchImpl(const Entry& entry, const ImageRgb& region) const {
    Hit miss;
    if (refW_ <= 0 || region.Empty()) {
        return miss;
    }
    if (region.width != refW_ * factor_ || region.height != refH_ * factor_) {
        return miss;  // tamanho diferente do cadastrado: precisa reconfigurar/regenerar
    }
    const ImageRgb scaled = BoxDownscale(region, factor_);
    if (scaled.Empty()) {
        return miss;
    }
    const int tw = entry.image.width;
    const int th = entry.image.height;
    if (tw > scaled.width || th > scaled.height) {
        return miss;
    }
    const std::size_t rowBytes = static_cast<std::size_t>(tw) * 3u;  // bytes por linha do template
    const double denom = static_cast<double>(rowBytes) * static_cast<double>(th);  // pixels*canais
    double bestSad = 255.0;
    int bestX = 0;
    int bestY = 0;
    bool found = false;
    // Nota de performance: sem early-exit por limiar — como o SAD é uma MÉDIA, um
    // único pixel fora do template derrubaria a soma parcial por baixo e faria o
    // corte antecipado descartar a batida correta. Para regiões de HUD (<=96 px na
    // escala de trabalho) o custo total segue mínimo; além disso, uma batida
    // praticamente idêntica encerra a busca imediatamente.
    for (int y = 0; y + th <= scaled.height && !found; ++y) {
        for (int x = 0; x + tw <= scaled.width; ++x) {
            unsigned long long sum = 0;
            const std::size_t sBase = static_cast<std::size_t>(y) * static_cast<std::size_t>(scaled.width) * 3u +
                                      static_cast<std::size_t>(x) * 3u;
            for (int ty = 0; ty < th; ++ty) {
                // stride da REGIÃO reduzida entre linhas; offset do TEMPLATE é ty*rowBytes
                const std::size_t sOff = sBase + static_cast<std::size_t>(ty) *
                                                      static_cast<std::size_t>(scaled.width) * 3u;
                const std::size_t tOff = static_cast<std::size_t>(ty) * rowBytes;
                for (std::size_t p = 0; p < rowBytes; ++p) {
                    sum += static_cast<unsigned long long>(
                        std::abs(static_cast<int>(scaled.rgb[sOff + p]) - static_cast<int>(entry.image.rgb[tOff + p])));
                }
            }
            const double sad = static_cast<double>(sum) / denom;
            if (sad < bestSad) {
                bestSad = sad;
                bestX = x;
                bestY = y;
                if (sad < 0.01) {
                    found = true;  // idêntica na escala de trabalho: não vale procurar mais
                    break;
                }
            }
        }
    }
    Hit hit;
    hit.sad = static_cast<float>(bestSad);
    hit.x = bestX * factor_;  // de volta à escala original da região
    hit.y = bestY * factor_;
    hit.templateWidth = tw * factor_;
    hit.templateHeight = th * factor_;
    return hit;
}

}  // namespace st
