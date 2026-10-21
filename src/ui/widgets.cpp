#include "ui/widgets.h"

#include "ui/theme.h"

#include <algorithm>
#include <cstring>

namespace st::widgets {

bool Toggle(const char* id, bool* value, float height) {
    const ImVec2 size(height * 2.0f, height);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(id, size);
    if (pressed) {
        *value = !*value;
    }

    // Posição animada da bolinha guardada no storage do ImGui.
    ImGuiStorage* storage = ImGui::GetStateStorage();
    const ImGuiID key = ImGui::GetItemID();
    float anim = storage->GetFloat(key, *value ? 1.0f : 0.0f);
    const float target = *value ? 1.0f : 0.0f;
    const float step = ImGui::GetIO().DeltaTime * 8.0f;
    anim = anim < target ? std::min(anim + step, target) : std::max(anim - step, target);
    storage->SetFloat(key, anim);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec4 off = theme::kPink;
    const ImVec4 on = theme::kCyan;
    const ImVec4 bg(off.x + (on.x - off.x) * anim, off.y + (on.y - off.y) * anim, off.z + (on.z - off.z) * anim, 1.0f);
    const float radius = size.y * 0.5f;
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), theme::U32(bg), radius);
    if (ImGui::IsItemHovered()) {
        dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), theme::U32(theme::kText, 0.25f), radius, 0, 2.0f);
    }
    const float knobX = pos.x + radius + anim * (size.x - 2.0f * radius);
    dl->AddCircleFilled(ImVec2(knobX, pos.y + radius), radius * 0.75f, IM_COL32_WHITE);
    return pressed;
}

bool OutlineButton(const char* label, const icons::Icon* icon, ImVec2 size) {
    // Fonte e medidas pelo tamanho efetivo (inclui io.FontGlobalScale do overlay).
    ImGui::PushFont(theme::Fonts().bold);
    ImFont* font = ImGui::GetFont();
    const float fontSize = ImGui::GetFontSize();
    const ImVec2 ts = ImGui::CalcTextSize(label, nullptr, true);
    ImGui::PopFont();

    if (size.y <= 0.0f) {
        size.y = 40.0f;
    }
    const float iconSize = size.y * 0.4f;
    const float iconW = icon != nullptr ? iconSize + 6.0f : 0.0f;
    if (size.x <= 0.0f) {
        size.x = ts.x + iconW + 28.0f;
    }
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(label, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 end(pos.x + size.x, pos.y + size.y);
    dl->AddRectFilled(pos, end, theme::U32(theme::Accent(), held ? 0.30f : (hovered ? 0.18f : 0.0f)), 8.0f);
    dl->AddRect(pos, end, theme::U32(theme::Accent()), 8.0f, 0, 2.0f);

    float x = pos.x + (size.x - (ts.x + iconW)) * 0.5f;
    const float cy = pos.y + size.y * 0.5f;
    if (icon != nullptr) {
        icons::Draw(dl, *icon, ImVec2(x + iconSize * 0.5f, cy), iconSize, theme::U32(theme::kText));
        x += iconW;
    }
    const char* hashes = std::strstr(label, "##");
    const char* labelEnd = hashes != nullptr ? hashes : label + std::strlen(label);
    dl->AddText(font, fontSize, ImVec2(x, cy - ts.y * 0.5f), theme::U32(theme::kText), label, labelEnd);
    return pressed;
}

bool IconButton(const char* id, icons::Icon icon, float size, bool framed) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(size, size));
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 end(pos.x + size, pos.y + size);
    if (framed) {
        dl->AddRectFilled(pos, end, theme::U32(hovered ? theme::kPanelAlt : theme::kBg), 8.0f);
        dl->AddRect(pos, end, theme::U32(theme::kBorder), 8.0f, 0, 1.0f);
    } else if (hovered) {
        dl->AddRectFilled(pos, end, theme::U32(theme::kPanelAlt), 8.0f);
    }
    icons::Draw(dl, icon, ImVec2(pos.x + size * 0.5f, pos.y + size * 0.5f), size * 0.45f,
                theme::U32(hovered ? theme::kText : ImVec4(0.85f, 0.85f, 0.85f, 1.0f)));
    return pressed;
}

void BeginCard(const char* id, ImVec2 size) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22.0f, 20.0f));
    ImGui::BeginChild(id, size, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
}

void EndCard() {
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void WeaponIcon(ImDrawList* dl, const Texture& tex, ImVec2 boxPos, ImVec2 boxSize) {
    if (!tex.Valid()) {
        return;
    }
    const float scale = std::min(boxSize.x / tex.width, boxSize.y / tex.height);
    const ImVec2 size(tex.width * scale, tex.height * scale);
    const ImVec2 pos(boxPos.x + (boxSize.x - size.x) * 0.5f, boxPos.y + (boxSize.y - size.y) * 0.5f);
    dl->AddImage(tex.Id(), pos, ImVec2(pos.x + size.x, pos.y + size.y));
}

bool Segmented(const char* id, const char* const* labels, int count, int* current, ImVec2 size) {
    ImGui::PushID(id);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float segW = size.x / static_cast<float>(count);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), theme::U32(theme::kPanelAlt), 8.0f);
    dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), theme::U32(theme::kBorder), 8.0f);
    bool changed = false;
    ImGui::PushFont(theme::Fonts().bold);
    for (int i = 0; i < count; ++i) {
        const ImVec2 a(pos.x + segW * static_cast<float>(i), pos.y);
        const ImVec2 b(a.x + segW, pos.y + size.y);
        ImGui::SetCursorScreenPos(a);
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##seg", ImVec2(segW, size.y)) && *current != i) {
            *current = i;
            changed = true;
        }
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();
        const bool active = *current == i;
        if (active) {
            dl->AddRectFilled(ImVec2(a.x + 3.0f, a.y + 3.0f), ImVec2(b.x - 3.0f, b.y - 3.0f),
                              theme::U32(theme::Accent(), 0.85f), 6.0f);
        } else if (hovered) {
            dl->AddRectFilled(ImVec2(a.x + 3.0f, a.y + 3.0f), ImVec2(b.x - 3.0f, b.y - 3.0f),
                              theme::U32(theme::kText, 0.06f), 6.0f);
        }
        const ImVec2 ts = ImGui::CalcTextSize(labels[i]);
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(a.x + (segW - ts.x) * 0.5f, a.y + (size.y - ts.y) * 0.5f),
                    theme::U32(active ? theme::kText : theme::kTextDim), labels[i]);
    }
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(pos);
    ImGui::Dummy(size);
    ImGui::PopID();
    return changed;
}

bool WeaponTiles(const char* id, const std::array<Texture, kWeaponCount>& icons, Weapon* current, float width,
                 float scale, bool disabled, int columns) {
    // Grade com `columns` colunas, posicionada a partir da origem.
    const std::size_t kColumns = static_cast<std::size_t>(std::max(columns, 1));
    const std::size_t rows = (kWeaponCount + kColumns - 1) / kColumns;
    const float k = scale;
    const float gap = 8.0f * k;
    const float w = (width - static_cast<float>(kColumns - 1) * gap) / static_cast<float>(kColumns);
    const float h = 58.0f * k;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool changed = false;
    ImGui::PushID(id);
    ImGui::BeginDisabled(disabled);
    for (std::size_t i = 0; i < kWeaponCount; ++i) {
        const auto weapon = static_cast<Weapon>(i);
        const auto col = static_cast<float>(i % kColumns);
        const auto row = static_cast<float>(i / kColumns);
        const ImVec2 pos(origin.x + col * (w + gap), origin.y + row * (h + gap));
        ImGui::SetCursorScreenPos(pos);
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::InvisibleButton("##tile", ImVec2(w, h)) && *current != weapon) {
            *current = weapon;
            changed = true;
        }
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();
        const bool active = *current == weapon;
        const ImVec2 end(pos.x + w, pos.y + h);
        dl->AddRectFilled(pos, end, theme::U32(hovered ? ImVec4(0.14f, 0.14f, 0.14f, 1.0f) : theme::kPanelAlt), 8.0f);
        dl->AddRect(pos, end, theme::U32(active ? theme::Accent() : theme::kBorder), 8.0f, 0, active ? 2.0f : 1.0f);
        WeaponIcon(dl, icons[i], ImVec2(pos.x + 8.0f * k, pos.y + 8.0f * k), ImVec2(w - 16.0f * k, 22.0f * k));
        ImGui::PushFont(theme::Fonts().caption);
        const char* name = GetWeaponInfo(weapon).name.data();
        const ImVec2 ts = ImGui::CalcTextSize(name);
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(pos.x + (w - ts.x) * 0.5f, pos.y + 36.0f * k),
                    theme::U32(active ? theme::Accent() : theme::kText), name);
        ImGui::PopFont();
    }
    ImGui::EndDisabled();
    ImGui::PopID();
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2(width, static_cast<float>(rows) * h + static_cast<float>(rows - 1) * gap));
    return changed;
}

bool WeaponDropdown(const char* id, const std::array<Texture, kWeaponCount>& icons, Weapon* current, float width,
                    float scale, bool disabled) {
    const float k = scale;
    const float height = 40.0f * k;
    const ImVec2 iconBox(76.0f * k, 24.0f * k);
    ImGui::PushID(id);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    if (ImGui::InvisibleButton("##sel", ImVec2(width, height)) && !disabled) {
        ImGui::OpenPopup("##weaponPopup");
    }
    const bool hovered = ImGui::IsItemHovered() && !disabled;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 end(pos.x + width, pos.y + height);
    dl->AddRectFilled(pos, end, theme::U32(hovered ? ImVec4(0.14f, 0.14f, 0.14f, 1.0f) : theme::kPanelAlt), 8.0f);
    dl->AddRect(pos, end, theme::U32(theme::kBorder), 8.0f);
    WeaponIcon(dl, icons[Index(*current)], ImVec2(pos.x + 12.0f * k, pos.y + (height - iconBox.y) * 0.5f), iconBox);
    ImGui::PushFont(theme::Fonts().bold);
    const float fs = ImGui::GetFontSize();
    const float textX = pos.x + 12.0f * k + iconBox.x + 14.0f * k;
    dl->AddText(ImGui::GetFont(), fs, ImVec2(textX, pos.y + (height - fs) * 0.5f),
                theme::U32(disabled ? theme::kTextDim : theme::kText), GetWeaponInfo(*current).name.data());
    const ImVec2 c(end.x - 22.0f * k, pos.y + height * 0.5f);
    dl->AddTriangleFilled(ImVec2(c.x - 5.0f * k, c.y - 2.0f * k), ImVec2(c.x + 5.0f * k, c.y - 2.0f * k),
                          ImVec2(c.x, c.y + 4.0f * k), theme::U32(theme::kText));

    // Abre para baixo se couber; senão para cima. Altura limitada (rola com 13 armas).
    const float rowH = 40.0f * k;
    const float listH = std::min(static_cast<float>(kWeaponCount) * rowH + 12.0f * k, 7.5f * rowH);
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const bool below = end.y + 4.0f + listH <= vp->Pos.y + vp->Size.y;
    ImGui::SetNextWindowPos(below ? ImVec2(pos.x, end.y + 4.0f) : ImVec2(pos.x, pos.y - 4.0f), ImGuiCond_Always,
                            below ? ImVec2(0.0f, 0.0f) : ImVec2(0.0f, 1.0f));
    ImGui::SetNextWindowSize(ImVec2(width, listH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f * k, 6.0f * k));
    bool changed = false;
    if (ImGui::BeginPopup("##weaponPopup")) {
        for (std::size_t i = 0; i < kWeaponCount; ++i) {
            const auto w = static_cast<Weapon>(i);
            ImGui::PushID(static_cast<int>(i));
            const ImVec2 rowPos = ImGui::GetCursorScreenPos();
            if (ImGui::Selectable("##w", w == *current, 0, ImVec2(0.0f, rowH)) && w != *current) {
                *current = w;
                changed = true;
            }
            if (ImGui::IsWindowAppearing() && w == *current) {
                ImGui::SetScrollHereY();
            }
            ImDrawList* pdl = ImGui::GetWindowDrawList();
            WeaponIcon(pdl, icons[i], ImVec2(rowPos.x + 6.0f * k, rowPos.y + (rowH - iconBox.y) * 0.5f), iconBox);
            pdl->AddText(ImGui::GetFont(), fs, ImVec2(rowPos.x + 6.0f * k + iconBox.x + 14.0f * k, rowPos.y + (rowH - fs) * 0.5f),
                         theme::U32(theme::kText), GetWeaponInfo(w).name.data());
            ImGui::PopID();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
    ImGui::PopFont();
    ImGui::PopID();
    return changed;
}

bool BeginSettings(const char* id, float labelWidth) {
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 12.0f));
    if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::PopStyleVar();
        return false;
    }
    ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, labelWidth);
    ImGui::TableSetupColumn("control", ImGuiTableColumnFlags_WidthStretch);
    return true;
}

void SettingLabel(const char* label, const char* help) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::PushFont(theme::Fonts().bold);
    ImGui::TextUnformatted(label);
    ImGui::PopFont();
    if (help != nullptr && help[0] != '\0') {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::kTextDim);
        ImGui::TextWrapped("%s", help);
        ImGui::PopStyleColor();
    }
    ImGui::TableSetColumnIndex(1);
}

void EndSettings() {
    ImGui::EndTable();
    ImGui::PopStyleVar();
}

void Caption(const char* text, const ImVec4& color) {
    ImGui::PushFont(theme::Fonts().caption);
    ImGui::TextColored(color, "%s", text);
    ImGui::PopFont();
}

}  // namespace st::widgets
