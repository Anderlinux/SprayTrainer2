#pragma once

#include "core/weapon.h"

#include <vector>

namespace st {

// Converte uma rajada desenhada com o mouse num padrão de recuo: é o inverso de IdealPath().
// `path[i]` é o movimento acumulado do mouse (graus) no instante do tiro i, com y > 0 = para baixo.
// `smoothing` (0 a 3) aplica uma média móvel no caminho para tirar o tremido da mão.
// Pontos além do caminho gravado (soltou antes, ou pente menor que 31) mantêm os valores de `base`.
Pattern PatternFromPath(const std::vector<Vec2>& path, const Pattern& base, int smoothing);

}  // namespace st
