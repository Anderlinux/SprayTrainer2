#include "config/profile_store.h"
#include "ui/pages.h"
#include "ui/profiles.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#include <imgui.h>

#include <algorithm>

namespace st::pages {

namespace {

using icons::Icon;

constexpr float kListWidth = 360.0f;

void ProfileList(AppState& state) {
    ProfileUiState& p = state.profiles;
    widgets::BeginCard("##profileList", ImVec2(kListWidth, 0.0f));
    widgets::Caption("PERFIS SALVOS", theme::kTextDim);
    ImGui::Spacing();

    const std::vector<std::string>& names = profiles::List(state);
    if (names.empty()) {
        ImGui::TextWrapped("Nenhum perfil salvo ainda.");
        ImGui::TextColored(theme::kTextDim, "Use \"Salvar como\" para criar o primeiro.");
    }

    const bool dirty = profiles::Dirty(state);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (const std::string& name : names) {
        ImGui::PushID(name.c_str());
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float w = ImGui::GetContentRegionAvail().x;
        const bool isCurrent = name == p.current;
        if (ImGui::Selectable("##row", p.selected == name, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(w, 44.0f))) {
            p.selected = name;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                profiles::Load(state, name);
            }
        }
        ImGui::PushFont(theme::Fonts().bold);
        const float fs = ImGui::GetFontSize();
        dl->AddText(ImGui::GetFont(), fs, ImVec2(pos.x + 12.0f, pos.y + (44.0f - fs) * 0.5f),
                    theme::U32(isCurrent ? theme::Accent() : theme::kText), name.c_str());
        ImGui::PopFont();
        if (isCurrent) {
            ImGui::PushFont(theme::Fonts().caption);
            const char* badge = dirty ? "ATUAL · NÃO SALVO" : "ATUAL";
            const ImVec2 ts = ImGui::CalcTextSize(badge);
            const ImVec2 b0(pos.x + w - ts.x - 22.0f, pos.y + 12.0f);
            const ImVec2 b1(pos.x + w - 8.0f, pos.y + 32.0f);
            const ImVec4 col = dirty ? theme::kPink : theme::Accent();
            dl->AddRectFilled(b0, b1, theme::U32(col, 0.18f), 6.0f);
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(b0.x + 7.0f, b0.y + (20.0f - ts.y) * 0.5f),
                        theme::U32(col), badge);
            ImGui::PopFont();
        }
        ImGui::PopID();
    }
    widgets::EndCard();
}

bool ActionButton(const char* label, Icon icon, float width, bool enabled = true) {
    ImGui::BeginDisabled(!enabled);
    const bool pressed = widgets::OutlineButton(label, &icon, ImVec2(width, 40.0f));
    ImGui::EndDisabled();
    return pressed && enabled;
}

void Actions(AppState& state) {
    ProfileUiState& p = state.profiles;
    widgets::BeginCard("##profileActions", ImVec2(0.0f, 0.0f));
    const float w = ImGui::GetContentRegionAvail().x;
    const float half = (w - 10.0f) * 0.5f;
    const bool dirty = profiles::Dirty(state);
    const bool hasSelection = !p.selected.empty() && p.store->Exists(p.selected);

    // Perfil carregado agora
    widgets::Caption("PERFIL ATUAL", theme::kTextDim);
    ImGui::PushFont(theme::Fonts().title);
    if (p.current.empty()) {
        ImGui::TextColored(theme::kTextDim, "Nenhum (não salvo)");
    } else {
        ImGui::TextColored(theme::Accent(), "%s", p.current.c_str());
    }
    ImGui::PopFont();
    ImGui::TextColored(dirty ? theme::kPink : theme::kTextDim, "%s",
                       dirty ? "Há alterações não salvas." : "Tudo salvo.");
    ImGui::Spacing();
    if (ActionButton("Salvar perfil  (Ctrl+S)##save", Icon::Folder, half)) {
        profiles::SaveCurrent(state);
    }
    ImGui::SameLine(0.0f, 10.0f);
    if (ActionButton("Salvar como...##saveas", Icon::Pencil, half)) {
        profiles::OpenPrompt(state, NamePrompt::SaveAs);
    }
    if (ActionButton("Novo perfil (padrões de fábrica)##new", Icon::Refresh, w)) {
        profiles::OpenPrompt(state, NamePrompt::New);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Perfil selecionado na lista
    widgets::Caption("PERFIL SELECIONADO", theme::kTextDim);
    ImGui::PushFont(theme::Fonts().bold);
    ImGui::TextUnformatted(hasSelection ? p.selected.c_str() : "Selecione um perfil na lista");
    ImGui::PopFont();
    ImGui::Spacing();
    if (ActionButton("Carregar##load", Icon::Home, half, hasSelection && !state.session.Running())) {
        profiles::Load(state, p.selected);
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && hasSelection && dirty) {
        ImGui::SetTooltip("As alterações não salvas do perfil atual serão descartadas");
    }
    ImGui::SameLine(0.0f, 10.0f);
    if (ActionButton("Duplicar##dup", Icon::Folder, half, hasSelection)) {
        profiles::OpenPrompt(state, NamePrompt::Duplicate, p.selected);
    }
    if (ActionButton("Renomear##ren", Icon::Pencil, half, hasSelection)) {
        profiles::OpenPrompt(state, NamePrompt::Rename, p.selected);
    }
    ImGui::SameLine(0.0f, 10.0f);
    if (ActionButton("Excluir##del", Icon::Trash, half, hasSelection)) {
        p.confirmDelete = true;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    widgets::Caption("ARQUIVOS", theme::kTextDim);
    ImGui::Spacing();
    if (ActionButton("Importar perfil...##import", Icon::Folder, half, !state.session.Running())) {
        p.pendingFile = FileAction::Import;
    }
    ImGui::SameLine(0.0f, 10.0f);
    if (ActionButton("Exportar perfil atual...##export", Icon::Folder, half)) {
        p.pendingFile = FileAction::Export;
    }
    ImGui::Spacing();
    ImGui::TextColored(theme::kTextDim, "Pasta dos perfis:");
    const std::string dir = ProfileStore::PathToUtf8(p.store->ProfilesDir());
    ImGui::TextWrapped("%s", dir.c_str());
    if (ImGui::SmallButton("Abrir pasta")) {
        p.requestOpenFolder = true;
    }
    widgets::EndCard();
}

}  // namespace

void Presets(AppState& state) {
    if (state.profiles.store == nullptr) {
        widgets::BeginCard("##noStore", ImVec2(0.0f, 0.0f));
        ImGui::TextColored(theme::kPink, "Não foi possível acessar a pasta de dados; perfis indisponíveis.");
        widgets::EndCard();
        return;
    }
    ProfileList(state);
    ImGui::SameLine(0.0f, 12.0f);
    Actions(state);
}

}  // namespace st::pages
