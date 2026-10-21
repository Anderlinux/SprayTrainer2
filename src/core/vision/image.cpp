#include "core/vision/image.h"

namespace st {

bool DownscaleHalf(const ImageRgb& in, ImageRgb& out) {
    if (in.Empty()) {
        return false;
    }
    const int w = in.width / 2;
    const int h = in.height / 2;
    if (w <= 0 || h <= 0) {
        return false;
    }
    ImageRgb res;
    res.width = w;
    res.height = h;
    res.rgb.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3u);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::size_t rowW = static_cast<std::size_t>(in.width) * 3u;
            const std::size_t srcBase = (static_cast<std::size_t>(y * 2) * static_cast<std::size_t>(in.width) +
                                         static_cast<std::size_t>(x * 2)) * 3u;
            const std::size_t dstBase = (static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                                         static_cast<std::size_t>(x)) * 3u;
            for (std::size_t c = 0; c < 3u; ++c) {
                // Média 2x2 com arredondamento para o inteiro mais próximo. Os
                // quatro pixels do bloco estão em srcBase (x,y), srcBase+3 (x+1,y),
                // srcBase+rowW (x,y+1) e srcBase+rowW+3 (x+1,y+1).
                const unsigned sum = in.rgb[srcBase + c] + in.rgb[srcBase + 3u + c] +
                                     in.rgb[srcBase + rowW + c] + in.rgb[srcBase + rowW + 3u + c];
                res.rgb[dstBase + c] = static_cast<std::uint8_t>((sum + 2u) / 4u);
            }
        }
    }
    out = std::move(res);
    return true;
}

void DownscaleToFit(const ImageRgb& in, int maxSide, ImageRgb& out) {
    if (in.Empty()) {
        out = ImageRgb{};
        return;
    }
    ImageRgb cur = in;
    while (cur.width > maxSide || cur.height > maxSide) {
        ImageRgb next;
        if (!DownscaleHalf(cur, next)) {
            break;  // 1x1 ou degenerada: para de reduzir
        }
        cur = std::move(next);
    }
    out = std::move(cur);
}

}  // namespace st
