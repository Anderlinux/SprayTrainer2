#pragma once

#include <imgui.h>

namespace st::icons {

enum class Icon {
    Home, Sliders, Palette, History, Folder, Trash, Pencil, Mouse, Target, Refresh, Clock,
    Eye, EyeOff, ChevronUp, ChevronDown, Window
};

// Desenha o ícone (vetorial) centralizado em `center`, dentro de um quadrado de lado `size`.
void Draw(ImDrawList* dl, Icon icon, ImVec2 center, float size, ImU32 color);

}  // namespace st::icons
