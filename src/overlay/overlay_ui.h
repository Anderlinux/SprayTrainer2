#pragma once

#include "ui/app_state.h"

#include <imgui.h>

namespace st {

// Tamanhos base (escala 1.0) do overlay.
inline constexpr ImVec2 kOverlayCompactSize(300.0f, 84.0f);
inline constexpr ImVec2 kOverlayExpandedSize(380.0f, 680.0f);

struct OverlayUiResult {
    // Área clicável no modo compacto (coordenadas do cliente). Fora dela o clique atravessa.
    ImVec2 hotMin{};
    ImVec2 hotMax{};
    bool toggleExpanded = false;
    // O overlay pediu para religar/desligar o replay pelo botão principal:
    // limpa a trava deixada por uma tecla de desativar (App trata no loop).
    bool resetReplayLockout = false;
};

// Desenha o conteúdo do overlay (compacto ou expandido) no contexto ImGui atual.
OverlayUiResult RenderOverlayUi(AppState& state, float scale);

// true se o VK é um botão do mouse (exportado de app.cpp; usado pelo overlay).
bool IsMouseButtonVk(int vk);

}  // namespace st
