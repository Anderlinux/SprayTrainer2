#include "ui/pages.h"
#include "ui/spray_plot.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#include <imgui.h>

#include <cfloat>
#include <cmath>

namespace st::pages {

namespace {

// Rajada de exemplo (padrão atual + um desvio suave) para a pré-visualização.
std::vector<ShotResult> SampleShots(const std::vector<Vec2>& ideal) {
    std::vector<ShotResult> shots;
    for (std::size_t i = 0; i < ideal.size(); ++i) {
        const float t = static_cast<float>(i);
        const Vec2 off{0.3f * std::sin(t * 0.45f), 0.2f * std::sin(t * 0.3f + 1.0f) - 0.0015f * t * t};
        const Vec2 p{ideal[i].x + off.x, ideal[i].y + off.y};
        shots.push_back({ideal[i], p, std::hypot(off.x, off.y)});
    }
    return shots;
}

void Settings(AppState& state) {
    VisualSettings& v = state.visual;
    widgets::BeginCard("##visualCard", ImVec2(ImGui::GetContentRegionAvail().x * 0.56f, 0.0f));
    if (widgets::BeginSettings("##visualRows", 210.0f)) {
        widgets::SettingLabel("Linha guia durante a rajada",
                              "\"Escondida\" é o modo desafio: você só vê o padrão antes e depois da rajada.");
        const char* modes[] = {"Sempre", "Próximos tiros", "Escondida"};
        int mode = static_cast<int>(v.guide);
        if (widgets::Segmented("##guide", modes, 3, &mode, ImVec2(ImGui::GetContentRegionAvail().x, 36.0f))) {
            v.guide = static_cast<GuideMode>(mode);
        }
        if (v.guide == GuideMode::Next) {
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SliderInt("##ahead", &v.guideAhead, 1, 10, "%d tiros à frente", ImGuiSliderFlags_AlwaysClamp);
        }

        widgets::SettingLabel("Cor da sua linha", "Opções testadas para daltonismo contra a linha ideal (roxa).");
        const float colorW = (ImGui::GetContentRegionAvail().x - 16.0f) / 3.0f;
        for (int i = 0; i < theme::kPlayerColorCount; ++i) {
            ImGui::PushID(i);
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            if (ImGui::InvisibleButton("##color", ImVec2(colorW, 34.0f))) {
                v.playerColor = i;
            }
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const bool selected = v.playerColor == i;
            dl->AddRectFilled(pos, ImVec2(pos.x + colorW, pos.y + 34.0f), theme::U32(theme::kPanelAlt), 8.0f);
            dl->AddRect(pos, ImVec2(pos.x + colorW, pos.y + 34.0f),
                        theme::U32(selected ? theme::kText : theme::kBorder), 8.0f, 0, selected ? 2.0f : 1.0f);
            dl->AddCircleFilled(ImVec2(pos.x + 18.0f, pos.y + 17.0f), 7.0f, theme::U32(theme::kPlayerColors[i].color));
            dl->AddText(ImVec2(pos.x + 32.0f, pos.y + (34.0f - ImGui::GetFontSize()) * 0.5f),
                        theme::U32(theme::kText), theme::kPlayerColors[i].name);
            ImGui::PopID();
            if (i + 1 < theme::kPlayerColorCount) {
                ImGui::SameLine(0.0f, 8.0f);
            }
        }

        widgets::SettingLabel("Espessura das linhas", "");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderFloat("##width", &v.lineWidth, 1.0f, 4.0f, "%.1f px", ImGuiSliderFlags_AlwaysClamp);

        widgets::SettingLabel("Grade", "Linhas a cada 1° (2° ou 5° quando o gráfico fica pequeno).");
        widgets::Toggle("##grid", &v.grid, 30.0f);

        widgets::SettingLabel("Números dos tiros", "Marca os tiros 1, 5, 10, 15... na linha ideal.");
        widgets::Toggle("##numbers", &v.shotNumbers, 30.0f);
        widgets::EndSettings();
    }
    widgets::EndCard();
}

void Preview(const AppState& state) {
    widgets::BeginCard("##visualPreview", ImVec2(0.0f, 0.0f));
    widgets::Caption("PRÉ-VISUALIZAÇÃO", theme::kTextDim);
    const auto magazine = static_cast<std::size_t>(GetWeaponInfo(state.weapon).magazine);
    const std::vector<Vec2> ideal = IdealPath(state.patterns.Get(state.weapon), magazine);
    const std::vector<ShotResult> shots = SampleShots(ideal);

    const float legendH = 2.0f * (ImGui::GetTextLineHeight() + ImGui::GetStyle().ItemSpacing.y) + 6.0f;
    const ImVec2 a = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 b(a.x + avail.x, a.y + avail.y - legendH);
    PlotOptions options = PlotOptionsFrom(state.visual);
    if (state.visual.guide == GuideMode::Next) {
        options.guideCount = 12 + static_cast<std::size_t>(state.visual.guideAhead);  // como no tiro 12
    }
    // No modo desafio a prévia mostra a guia (é como fica depois da rajada).
    DrawSprayPlot(ImGui::GetWindowDrawList(), a, b, ideal, shots, nullptr, options);
    ImGui::SetCursorScreenPos(ImVec2(a.x, b.y + 6.0f));
    ImGui::BeginGroup();
    PlotLegend(state.visual);
    ImGui::EndGroup();
    widgets::EndCard();
}

}  // namespace

void Visual(AppState& state) {
    Settings(state);
    ImGui::SameLine(0.0f, 12.0f);
    Preview(state);
}

}  // namespace st::pages
