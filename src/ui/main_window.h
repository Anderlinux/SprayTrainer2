#pragma once

#include "ui/app_state.h"

namespace st {

// Largura reservada aos botões da barra de título (fora da área de arrastar).
inline constexpr float kTitleButtonsWidth = 194.0f;

void RenderMainWindow(AppState& state, double nowMs);

}  // namespace st
