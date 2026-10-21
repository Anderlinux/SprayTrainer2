#pragma once

#include <imgui.h>

namespace st::theme {

// Paleta da referência visual.
inline constexpr ImVec4 kBg{0.039f, 0.039f, 0.039f, 1.0f};        // #0A0A0A
inline constexpr ImVec4 kPanel{0.059f, 0.059f, 0.059f, 1.0f};     // #0F0F0F
inline constexpr ImVec4 kPanelAlt{0.102f, 0.102f, 0.102f, 1.0f};  // #1A1A1A
inline constexpr ImVec4 kBorder{0.165f, 0.165f, 0.165f, 1.0f};    // #2A2A2A
inline constexpr ImVec4 kDefaultAccent{157.0f / 255.0f, 78.0f / 255.0f, 221.0f / 255.0f, 1.0f};  // #9D4EDD
inline constexpr ImVec4 kCyan{0.0f, 0.737f, 0.831f, 1.0f};        // #00BCD4
inline constexpr ImVec4 kPink{1.0f, 0.090f, 0.267f, 1.0f};        // #FF1744
inline constexpr ImVec4 kText{0.93f, 0.93f, 0.93f, 1.0f};
inline constexpr ImVec4 kTextDim{0.54f, 0.54f, 0.54f, 1.0f};

// Paleta do gráfico (separada da cor de destaque da interface). Pares validados no tema
// escuro (#0A0A0A): faixa de luminosidade, croma, contraste e separação para daltonismo.
inline constexpr ImVec4 kChartIdeal{157.0f / 255.0f, 78.0f / 255.0f, 221.0f / 255.0f, 1.0f};  // #9D4EDD

struct PlayerColor {
    const char* name;
    ImVec4 color;
};
inline constexpr PlayerColor kPlayerColors[] = {
    {"Ciano", {0.0f, 151.0f / 255.0f, 167.0f / 255.0f, 1.0f}},              // #0097A7
    {"Laranja", {230.0f / 255.0f, 81.0f / 255.0f, 0.0f, 1.0f}},             // #E65100
    {"Verde", {67.0f / 255.0f, 160.0f / 255.0f, 71.0f / 255.0f, 1.0f}},     // #43A047
};
inline constexpr int kPlayerColorCount = 3;

inline constexpr float kTitleBarHeight = 44.0f;
inline constexpr float kSidebarWidth = 110.0f;

struct FontSet {
    ImFont* regular = nullptr;
    ImFont* bold = nullptr;
    ImFont* caption = nullptr;  // rótulos em caixa alta
    ImFont* title = nullptr;  // status grande
};

// Cor de destaque atual (configurável na aba Ajustes > Interface).
const ImVec4& Accent();
void SetAccent(const ImVec4& color);

// Aplica o estilo no contexto ImGui atual (chamar de novo após mudar o destaque).
void Apply();
// Segoe UI (normal e negrito) se existir, senão a fonte padrão do ImGui. `scale` multiplica
// os tamanhos (fontes rasterizadas já no tamanho final ficam nítidas).
void LoadFonts(float scale = 1.0f);
const FontSet& Fonts();

ImU32 U32(const ImVec4& c, float alphaMul = 1.0f);

}  // namespace st::theme
