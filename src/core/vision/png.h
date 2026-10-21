#pragma once

#include "core/vision/image.h"

#include <vector>

namespace st {

// Decodificador PNG mínimo, só com a stdlib (sem zlib/stb — o build do núcleo não
// pode depender de rede nem de bibliotecas novas). Aceita o que o app produz e o
// que é comum em capturas de tela: truecolor 8 bits (RGB/RGBA), cinza 8/16,
// paleta (1/2/4/8 bits) e cinza+alfa, com todos os filtros de scanline.
// Devolve RGB compacto linha a linha; `out` fica vazio em caso de falha.
bool DecodePng(const std::vector<unsigned char>& bytes, ImageRgb& out);

}  // namespace st
