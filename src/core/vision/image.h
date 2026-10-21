#pragma once

#include <cstdint>
#include <vector>

namespace st {

// Imagem RGB de 8 bits por canal, linha a linha (3 bytes por pixel). Núcleo puro:
// sem dependências de plataforma — a captura (Win32) e a UI só entregam/recebem bytes.
struct ImageRgb {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgb;  // tamanho = width * height * 3

    std::size_t SizeBytes() const { return static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3u; }
    bool Empty() const { return width <= 0 || height <= 0 || rgb.size() != SizeBytes(); }
};

// Reduz pela metade (2x2 -> média). Devolve false se `out` já couber em `maxSide`
// ou se a imagem for degenerada; `out` recebe o resultado quando devolve true.
bool DownscaleHalf(const ImageRgb& in, ImageRgb& out);

// Reduz até que o maior lado seja <= maxSide (repete DownscaleHalf). Se já couber,
// copia `in` para `out` sem alterar.
void DownscaleToFit(const ImageRgb& in, int maxSide, ImageRgb& out);

}  // namespace st
