#include "overlay/overlay_ui.h"

#include "ui/icons.h"
#include "ui/pattern_editor.h"
#include "ui/profiles.h"
#include "ui/spray_plot.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#include <algorithm>
#include <cstdio>

namespace st {

namespace {

using icons::Icon;

const char* StatusText(const AppState& s) {
    // O replay é a função principal do overlay: ele manda no status sempre que
    // está habilitado — em andamento ("ATIVO") ou pronto para disparar ("PRONTO").
    // O estado do treino manual só aparece quando o replay está desabilitado.
    if (s.repetition.enabled) {
        return s.repetitionRunning ? "REPLAY ATIVO" : "REPLAY PRONTO";
    }
    return s.trainingEnabled ? "TREINO ATIVO" : "TREINO INATIVO";
}

ImVec4 StatusColor(const AppState& s) {
    if (s.repetition.enabled) {
        return s.repetitionRunning ? theme::kCyan : theme::Accent();
    }
    return s.trainingEnabled ? theme::kCyan : theme::kPink;
}

void DrawStatusDot(ImDrawList* dl, ImVec2 c, float r, const ImVec4& col) {
    dl->AddCircleFilled(c, r * 2.0f, theme::U32(col, 0.18f));
    dl->AddCircleFilled(c, r, theme::U32(col));
}

// Botão desenhado à mão, com área exata (usado para o hot rect do modo compacto).
bool FlatButton(const char* id, ImVec2 pos, ImVec2 size, Icon icon, float iconSize) {
    ImGui::SetCursorScreenPos(pos);
    const bool pressed = ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 end(pos.x + size.x, pos.y + size.y);
    dl->AddRectFilled(pos, end, theme::U32(hovered ? theme::Accent() : theme::kPanelAlt, hovered ? 0.35f : 1.0f),
                      8.0f);
    icons::Draw(dl, icon, ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f), iconSize, theme::U32(theme::kText));
    return pressed;
}

// ------------------------------------------------------------------ compacto

OverlayUiResult Compact(AppState& state, float k) {
    OverlayUiResult result;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 o = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();

    const ImVec2 iconBox(70.0f * k, 22.0f * k);
    widgets::WeaponIcon(dl, state.weaponIcons[Index(state.weapon)], ImVec2(o.x + 14.0f * k, o.y + 14.0f * k), iconBox);

    ImGui::PushFont(theme::Fonts().bold);
    const float fs = ImGui::GetFontSize();
    dl->AddText(ImGui::GetFont(), fs, ImVec2(o.x + 94.0f * k, o.y + 14.0f * k + (iconBox.y - fs) * 0.5f),
                theme::U32(theme::kText), GetWeaponInfo(state.weapon).name.data());
    ImGui::PopFont();

    const float rowY = o.y + 60.0f * k;
    DrawStatusDot(dl, ImVec2(o.x + 20.0f * k, rowY), 4.0f * k, StatusColor(state));
    ImGui::PushFont(theme::Fonts().caption);
    const float cs = ImGui::GetFontSize();
    dl->AddText(ImGui::GetFont(), cs, ImVec2(o.x + 32.0f * k, rowY - cs * 0.5f), theme::U32(StatusColor(state)),
                StatusText(state));

    char info[64];
    if (state.repetitionRunning) {
        std::snprintf(info, sizeof(info), "Replay %zu/%d", static_cast<std::size_t>(state.repetitionShot),
                      state.repetitionShotsTotal);
    } else if (state.session.Running()) {
        std::snprintf(info, sizeof(info), "Tiro %zu/%zu", state.session.ShotsFired(), state.session.ShotsTotal());
    } else if (state.lastResult && !state.lastResult->shots.empty()) {
        std::snprintf(info, sizeof(info), "Última: %.0f", static_cast<double>(state.lastResult->score));
    } else {
        std::snprintf(info, sizeof(info), "Sem rajadas");
    }
    const float infoW = ImGui::CalcTextSize(info).x;
    dl->AddText(ImGui::GetFont(), cs, ImVec2(o.x + size.x - 56.0f * k - infoW, rowY - cs * 0.5f),
                theme::U32(theme::kTextDim), info);
    ImGui::PopFont();

    // Botão de expandir: única área que recebe clique no modo compacto.
    const ImVec2 btnPos(o.x + size.x - 44.0f * k, o.y + 10.0f * k);
    const ImVec2 btnSize(34.0f * k, size.y - 20.0f * k);
    if (FlatButton("##expand", btnPos, btnSize, Icon::ChevronUp, 16.0f * k)) {
        result.toggleExpanded = true;
    }
    result.hotMin = ImVec2(btnPos.x - o.x, btnPos.y - o.y);
    result.hotMax = ImVec2(result.hotMin.x + btnSize.x, result.hotMin.y + btnSize.y);
    return result;
}

// ------------------------------------------------------------------ expandido

// Rodapé: perfil atual (clique abre a lista para carregar) + Salvar.
void ProfileFooter(AppState& state, ImVec2 pos, ImVec2 size, float k) {
    if (state.profiles.store == nullptr) {
        return;
    }
    const float saveW = 92.0f * k;
    const float gap = 8.0f * k;
    const bool dirty = profiles::Dirty(state);
    const std::string& current = state.profiles.current;
    const std::string label = "Perfil: " + (current.empty() ? std::string("não salvo") : current) + "##ovProfile";

    ImGui::SetCursorScreenPos(pos);
    const ImVec2 btnSize(size.x - saveW - gap, size.y);
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
    if (ImGui::Button(label.c_str(), btnSize)) {
        ImGui::OpenPopup("##ovProfiles");
    }
    ImGui::PopStyleVar();
    if (dirty) {
        ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(pos.x + btnSize.x - 14.0f * k, pos.y + size.y * 0.5f),
                                                    4.0f * k, theme::U32(theme::kPink));
    }
    ImGui::SameLine(0.0f, gap);
    ImGui::BeginDisabled(!dirty);
    if (widgets::OutlineButton("Salvar##ovSave", nullptr, ImVec2(saveW, size.y))) {
        profiles::SaveCurrent(state);
    }
    ImGui::EndDisabled();

    const std::vector<std::string>& names = profiles::List(state);
    const float popupH = std::min(static_cast<float>(names.size()) * 34.0f * k + 16.0f * k, 260.0f * k);
    ImGui::SetNextWindowPos(ImVec2(pos.x, pos.y - 6.0f * k), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
    ImGui::SetNextWindowSize(ImVec2(btnSize.x, std::max(popupH, 44.0f * k)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f * k, 8.0f * k));
    const bool open = ImGui::BeginPopup("##ovProfiles");
    ImGui::PopStyleVar();
    if (open) {
        if (names.empty()) {
            ImGui::TextColored(theme::kTextDim, "Nenhum perfil salvo");
        }
        for (const std::string& name : names) {
            if (ImGui::Selectable(name.c_str(), name == current, 0, ImVec2(0.0f, 26.0f * k))) {
                profiles::Load(state, name);
            }
        }
        ImGui::EndPopup();
    }
}

OverlayUiResult Expanded(AppState& state, float k) {
    OverlayUiResult result;
    const ImVec2 o = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float pad = 16.0f * k;

    // Cabeçalho
    widgets::WeaponIcon(dl, state.weaponIcons[Index(state.weapon)], ImVec2(o.x + pad, o.y + pad),
                        ImVec2(70.0f * k, 22.0f * k));
    ImGui::PushFont(theme::Fonts().bold);
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(o.x + pad + 80.0f * k, o.y + pad + 2.0f * k),
                theme::U32(theme::kText), GetWeaponInfo(state.weapon).name.data());
    ImGui::PopFont();
    DrawStatusDot(dl, ImVec2(o.x + pad + 4.0f * k, o.y + pad + 38.0f * k), 4.0f * k, StatusColor(state));
    ImGui::PushFont(theme::Fonts().caption);
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(o.x + pad + 16.0f * k, o.y + pad + 31.0f * k),
                theme::U32(StatusColor(state)), StatusText(state));
    ImGui::PopFont();

    const ImVec2 btn(32.0f * k, 32.0f * k);
    if (FlatButton("##openMain", ImVec2(o.x + size.x - pad - 2.0f * btn.x - 6.0f * k, o.y + pad), btn, Icon::Window,
                   16.0f * k)) {
        state.requestShowMain = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Abrir o Spray Trainer");
    }
    if (FlatButton("##collapse", ImVec2(o.x + size.x - pad - btn.x, o.y + pad), btn, Icon::ChevronDown, 16.0f * k)) {
        result.toggleExpanded = true;
    }

    const float fullW = size.x - 2.0f * pad;
    const float rowH = 36.0f * k;
    ImGui::SetCursorScreenPos(ImVec2(o.x + pad, o.y + pad + 54.0f * k));
    // O replay é a função principal: a aba dele vem primeiro e é a padrão.
    const char* tabs[] = {"Replay", "Padrão"};
    int tab = state.overlayShowEditor ? 1 : 0;
    if (widgets::Segmented("##ovTabs", tabs, 2, &tab, ImVec2(fullW, 32.0f * k))) {
        state.overlayShowEditor = tab == 1;
    }

    ImGui::SetCursorScreenPos(ImVec2(o.x + pad, o.y + pad + 96.0f * k));
    widgets::Caption("ARMA", theme::kTextDim);
    // A janela do overlay não tem padding: cada bloco começa na margem manualmente.
    ImGui::SetCursorScreenPos(ImVec2(o.x + pad, ImGui::GetCursorScreenPos().y));
    Weapon w = state.weapon;
    if (widgets::WeaponDropdown("##ovWeapon", state.weaponIcons, &w, fullW, k, state.session.Running())) {
        SelectWeapon(state, w);
    }

    if (state.overlayShowEditor) {
        // Mesmo editor da aba Ajustes, empilhado. Overlay não recebe teclado: edite arrastando.
        ImGui::SetCursorScreenPos(ImVec2(o.x + pad, ImGui::GetCursorScreenPos().y + 4.0f * k));
        ImGui::BeginChild("##ovEditor", ImVec2(fullW, o.y + size.y - pad - ImGui::GetCursorScreenPos().y),
                          ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
        PatternEditorLayout layout;
        layout.stacked = true;
        layout.plotHeight = 150.0f * k;
        layout.scale = k;
        PatternEditor(state, state.weapon, layout);
        ImGui::EndChild();
        result.hotMin = ImVec2(0.0f, 0.0f);
        result.hotMax = size;
        return result;
    }

    ImGui::SetCursorScreenPos(ImVec2(o.x + pad, ImGui::GetCursorScreenPos().y));
    ImGui::BeginGroup();
    ImGui::PushItemWidth(fullW);

    ImGui::Spacing();
    // Bloco principal do overlay: o REPLAY (repetição automática do padrão).
    widgets::Caption("REPLAY", theme::kTextDim);
    {
        char repLabel[48];
        std::snprintf(repLabel, sizeof(repLabel), "%s##ovRepToggle",
                      state.repetition.enabled ? "Desligar replay" : "Ligar replay");
        if (widgets::OutlineButton(repLabel, nullptr, ImVec2(fullW, rowH))) {
            // Botão "Ligar/Desligar": religa/desliga o replay e rearma o clique de
            // atirar. A trava da tecla de desativar é limpa pelo App via
            // resetReplayLockout (o overlay não conhece as flags internas).
            state.repetition.enabled = !state.repetition.enabled;
            state.repeatToggledOn = false;
            state.repeatArmed = !IsMouseButtonVk(state.repetition.repeatKeyVk);
            result.resetReplayLockout = true;
        }
    }

    widgets::Caption("TREINO MANUAL", theme::kTextDim);
    widgets::Toggle("##ovEnabled", &state.trainingEnabled, 30.0f * k);
    ImGui::SameLine(0.0f, 12.0f * k);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f * k);
    ImGui::TextColored(theme::kTextDim, "%s", state.trainingEnabled ? "Ligado" : "Desligado");

    widgets::Caption("BOTÃO DE DISPARO", theme::kTextDim);
    char fireLabel[64];
    std::snprintf(fireLabel, sizeof(fireLabel), "%s##ovFire", FireButtonName(state.fireButton));
    const Icon mouseIcon = Icon::Mouse;
    if (widgets::OutlineButton(fireLabel, &mouseIcon, ImVec2(fullW, rowH))) {
        ToggleFireButton(state);
    }

    widgets::Caption("META DE NOTA (OPCIONAL)", theme::kTextDim);
    char goalLabel[48];
    if (state.scoreGoal == 0) {
        std::snprintf(goalLabel, sizeof(goalLabel), "Nenhuma##ovGoal");
    } else {
        std::snprintf(goalLabel, sizeof(goalLabel), "%d pontos##ovGoal", state.scoreGoal);
    }
    const Icon targetIcon = Icon::Target;
    if (widgets::OutlineButton(goalLabel, &targetIcon, ImVec2(fullW - rowH - 8.0f * k, rowH))) {
        CycleScoreGoal(state);
    }
    ImGui::SameLine(0.0f, 8.0f * k);
    if (widgets::IconButton("##ovClearGoal", Icon::Trash, rowH, false)) {
        state.scoreGoal = 0;
    }

    widgets::Caption("SENSIBILIDADE NO JOGO", theme::kTextDim);
    ImGui::BeginDisabled(state.session.Running());
    if (ImGui::DragFloat("##ovSens", &state.sensitivity, 0.005f, 0.05f, 20.0f, "%.2f",
                         ImGuiSliderFlags_AlwaysClamp)) {
        state.lastResult.reset();
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    widgets::Caption("ÚLTIMA RAJADA", theme::kTextDim);
    const ImVec2 plotA = ImGui::GetCursorScreenPos();
    const float footerH = 36.0f * k;
    const float plotH = std::max(o.y + size.y - pad - footerH - 12.0f * k - plotA.y, 60.0f * k);
    const ImVec2 plotB(plotA.x + fullW, plotA.y + plotH);
    dl->AddRectFilled(plotA, plotB, theme::U32(theme::kBg), 8.0f);
    dl->AddRect(plotA, plotB, theme::U32(theme::kBorder), 8.0f);
    if (state.lastResult && !state.lastResult->shots.empty()) {
        DrawSprayPlot(dl, plotA, plotB, state.lastResult->idealPath, state.lastResult->shots, nullptr,
                      PlotOptionsFrom(state.visual));
        char score[32];
        std::snprintf(score, sizeof(score), "%.0f", static_cast<double>(state.lastResult->score));
        ImGui::PushFont(theme::Fonts().title);
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(plotA.x + 10.0f * k, plotA.y + 6.0f * k),
                    theme::U32(state.lastResult->score >= 70.0f ? theme::kCyan : theme::kPink), score);
        ImGui::PopFont();
    } else {
        const char* empty = "Faça uma rajada no app para ver aqui";
        const ImVec2 ts = ImGui::CalcTextSize(empty);
        dl->AddText(ImVec2(plotA.x + (fullW - ts.x) * 0.5f, plotA.y + (plotH - ts.y) * 0.5f),
                    theme::U32(theme::kTextDim), empty);
    }
    ImGui::Dummy(ImVec2(fullW, plotH));
    ImGui::PopItemWidth();
    ImGui::EndGroup();

    ProfileFooter(state, ImVec2(o.x + pad, o.y + size.y - pad - footerH), ImVec2(fullW, footerH), k);

    // Expandido: o painel inteiro recebe clique.
    result.hotMin = ImVec2(0.0f, 0.0f);
    result.hotMax = size;
    return result;
}

}  // namespace

OverlayUiResult RenderOverlayUi(AppState& state, float scale) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, theme::kPanel);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(theme::Accent().x, theme::Accent().y, theme::Accent().z, 0.6f));
    ImGui::Begin("##overlay", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    const OverlayUiResult result = state.overlayExpanded ? Expanded(state, scale) : Compact(state, scale);
    ImGui::End();
    return result;
}

}  // namespace st
