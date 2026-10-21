#include "ui/pages.h"
#include "ui/spray_plot.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace st::pages {

namespace {

using icons::Icon;

constexpr float kLabelColumn = 190.0f;

// Seletor de arma com ícone + nome (o Combo padrão não desenha imagem no preview).
void WeaponSelector(AppState& state, float width) {
    Weapon w = state.weapon;
    if (widgets::WeaponDropdown("##weaponSel", state.weaponIcons, state.weaponIconW, state.weaponIconH, &w, width, 1.0f, state.session.Running())) {
        SelectWeapon(state, w);
    }
}

void RightAlignedCaption(const char* text, float rightX) {
    ImGui::PushFont(theme::Fonts().caption);
    const float w = ImGui::CalcTextSize(text).x;
    ImGui::SetCursorPosX(rightX - w);
    ImGui::TextColored(theme::kText, "%s", text);
    ImGui::PopFont();
}

void StatusCard(AppState& state) {
    widgets::BeginCard("##status", ImVec2(0.0f, 172.0f));
    const float w = ImGui::GetContentRegionAvail().x;
    const float startX = ImGui::GetCursorPosX();
    const float startY = ImGui::GetCursorPosY();
    const float splitX = startX + w * 0.58f;

    // Coluna esquerda: status, mouse e interruptor.
    ImGui::PushFont(theme::Fonts().title);
    if (state.repetitionRunning) {
        // Replay em andamento: o status passa a mostrar o estado da repetição.
        ImGui::TextColored(theme::kCyan, "REPLAY ATIVO");
    } else if (state.trainingEnabled) {
        ImGui::TextColored(theme::kCyan, "TREINO ATIVO");
    } else {
        ImGui::TextColored(theme::Accent(), "TREINO INATIVO");
    }
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0.0f, 2.0f));
    widgets::Caption(state.mouseDetected ? "MOUSE: DETECTADO" : "MOUSE: NÃO DETECTADO",
                     state.mouseDetected ? theme::kCyan : theme::kPink);
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    widgets::Toggle("##enabled", &state.trainingEnabled);

    // Divisória
    const ImVec2 wp = ImGui::GetWindowPos();
    ImGui::GetWindowDrawList()->AddLine(ImVec2(wp.x + splitX, wp.y + 20.0f),
                                        ImVec2(wp.x + splitX, wp.y + ImGui::GetWindowHeight() - 20.0f),
                                        theme::U32(theme::kBorder), 1.0f);

    // Coluna direita: botão de disparo e meta, alinhados à direita como na referência.
    const float rightX = startX + w;
    ImGui::SetCursorPos(ImVec2(rightX, startY));
    RightAlignedCaption("BOTÃO DE DISPARO", rightX);

    char fireLabel[64];
    std::snprintf(fireLabel, sizeof(fireLabel), "Disparo: %s##fire", FireButtonName(state.fireButton));
    const Icon mouseIcon = Icon::Mouse;
    const float fireW = 250.0f;
    ImGui::SetCursorPosX(rightX - fireW);
    if (widgets::OutlineButton(fireLabel, &mouseIcon, ImVec2(fireW, 40.0f))) {
        ToggleFireButton(state);
    }

    ImGui::Dummy(ImVec2(0.0f, 2.0f));
    RightAlignedCaption("META DE NOTA (OPCIONAL)", rightX);
    char goalLabel[48];
    if (state.scoreGoal == 0) {
        std::snprintf(goalLabel, sizeof(goalLabel), "Meta: Nenhuma##goal");
    } else {
        std::snprintf(goalLabel, sizeof(goalLabel), "Meta: %d pontos##goal", state.scoreGoal);
    }
    const Icon targetIcon = Icon::Target;
    const float goalW = 200.0f;
    ImGui::SetCursorPosX(rightX - goalW - 46.0f);
    if (widgets::OutlineButton(goalLabel, &targetIcon, ImVec2(goalW, 40.0f))) {
        CycleScoreGoal(state);
    }
    ImGui::SameLine(0.0f, 6.0f);
    if (widgets::IconButton("##clearGoal", Icon::Trash, 40.0f, false)) {
        state.scoreGoal = 0;
    }
    widgets::EndCard();
}

void ConfigCard(AppState& state) {
    ImGui::Dummy(ImVec2(0.0f, 2.0f));
    ImGui::TextUnformatted("Configuração de Treino");
    widgets::BeginCard("##config", ImVec2(0.0f, 136.0f));
    const float fieldW = ImGui::GetContentRegionAvail().x - kLabelColumn - 52.0f;

    ImGui::PushFont(theme::Fonts().bold);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10.0f);
    ImGui::TextUnformatted("Perfil de Arma:");
    ImGui::PopFont();
    ImGui::SameLine(kLabelColumn);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 10.0f);
    WeaponSelector(state, fieldW);
    ImGui::SameLine(0.0f, 12.0f);
    if (widgets::IconButton("##editPattern", Icon::Pencil, 40.0f)) {
        state.page = Page::Ajustes;
        state.editorWeapon = state.weapon;
        state.settingsTabRequest = 0;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Editar o padrão desta arma");
    }

    ImGui::PushFont(theme::Fonts().bold);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Sensibilidade no Jogo:");
    ImGui::PopFont();
    ImGui::SameLine(kLabelColumn);
    ImGui::SetNextItemWidth(fieldW);
    ImGui::BeginDisabled(state.session.Running());
    if (ImGui::InputFloat("##sens", &state.sensitivity, 0.0f, 0.0f, "%.2f")) {
        state.sensitivity = std::clamp(state.sensitivity, 0.05f, 20.0f);
    }
    ImGui::EndDisabled();
    widgets::EndCard();
}

// Legenda no canto superior esquerdo da área do gráfico, perto das linhas que ela explica.
void AreaLegend(const AppState& state, ImVec2 areaMin) {
    ImGui::SetCursorScreenPos(ImVec2(areaMin.x + 4.0f, areaMin.y + 2.0f));
    ImGui::BeginGroup();
    PlotLegend(state.visual);
    ImGui::EndGroup();
}

void TrainingArea(AppState& state, double nowMs, ImVec2 size) {
    TrainingSession& session = state.session;
    widgets::BeginCard("##area", size);
    const ImVec2 a = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImGuiButtonFlags flags =
        state.fireButton == FireButton::Left ? ImGuiButtonFlags_MouseButtonLeft : ImGuiButtonFlags_MouseButtonRight;
    ImGui::InvisibleButton("##train", avail, flags);
    const ImVec2 b(a.x + avail.x, a.y + avail.y);

    if (ImGui::IsItemActivated() && state.trainingEnabled) {
        session.Start(state.weapon, state.patterns.Get(state.weapon), state.sensitivity, nowMs,
                      state.scoreTolerance);
        state.lastResult.reset();
    }
    if (ImGui::IsItemDeactivated() && session.Running()) {
        session.Stop(nowMs);
    }
    state.captureMouse = session.Running();
    if (state.captureMouse) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    PlotOptions options = PlotOptionsFrom(state.visual);
    if (session.Running()) {
        // Guia conforme a aba Visual: inteira, só os próximos tiros, ou escondida (desafio).
        switch (state.visual.guide) {
            case GuideMode::Next:
                options.guideCount = session.ShotsFired() + static_cast<std::size_t>(state.visual.guideAhead);
                break;
            case GuideMode::Hidden:
                options.guideCount = 0;
                break;
            case GuideMode::Always:
                break;
        }
        const Vec2 live = session.LivePlayer();
        DrawSprayPlot(dl, a, b, session.IdealPath(), session.Shots(), &live, options);
        AreaLegend(state, a);
        return widgets::EndCard();
    }
    if (state.lastResult) {
        DrawSprayPlot(dl, a, b, state.lastResult->idealPath, state.lastResult->shots, nullptr, options);
        AreaLegend(state, a);
        return widgets::EndCard();
    }

    const auto magazine = static_cast<std::size_t>(GetWeaponInfo(state.weapon).magazine);
    DrawSprayPlot(dl, a, b, IdealPath(state.patterns.Get(state.weapon), magazine), {}, nullptr, options);
    const char* button = state.fireButton == FireButton::Left ? "esquerdo" : "direito";
    char hint[128];
    if (state.trainingEnabled && state.visual.guide == GuideMode::Hidden) {
        std::snprintf(hint, sizeof(hint), "Modo desafio: memorize a linha; ela some quando você segurar o botão %s",
                      button);
    } else if (state.trainingEnabled) {
        std::snprintf(hint, sizeof(hint), "Segure o botão %s aqui e puxe o mouse seguindo a linha roxa", button);
    } else {
        std::snprintf(hint, sizeof(hint), "Ligue o interruptor acima para começar a treinar");
    }
    const ImVec2 ts = ImGui::CalcTextSize(hint);
    dl->AddText(ImVec2(a.x + (avail.x - ts.x) * 0.5f, b.y - ts.y), theme::U32(theme::kTextDim), hint);
    AreaLegend(state, a);
    widgets::EndCard();
}

void ResultsCard(const AppState& state, ImVec2 size) {
    widgets::BeginCard("##results", size);
    widgets::Caption("RESULTADO", theme::kTextDim);
    ImGui::Spacing();
    const TrainingSession& session = state.session;
    if (session.Running()) {
        ImGui::PushFont(theme::Fonts().title);
        ImGui::Text("%zu / %zu", session.ShotsFired(), session.ShotsTotal());
        ImGui::PopFont();
        ImGui::TextColored(theme::kTextDim, "tiros");
    } else if (state.lastResult && !state.lastResult->shots.empty()) {
        const SessionResult& r = *state.lastResult;
        const bool good = state.scoreGoal > 0 ? r.score >= static_cast<float>(state.scoreGoal) : r.score >= 70.0f;
        ImGui::PushFont(theme::Fonts().title);
        ImGui::TextColored(good ? theme::kCyan : theme::kPink, "%.0f", static_cast<double>(r.score));
        ImGui::PopFont();
        ImGui::TextColored(theme::kTextDim, "nota (0-100)");
        ImGui::Spacing();
        ImGui::Text("Tiros: %zu", r.shots.size());
        ImGui::Text("Erro médio: %.2f°", static_cast<double>(r.meanError));
        if (state.scoreGoal > 0) {
            ImGui::Spacing();
            if (good) {
                ImGui::TextColored(theme::kCyan, "Meta batida!");
            } else {
                ImGui::TextColored(theme::kPink, "Faltaram %.0f pontos",
                                   static_cast<double>(static_cast<float>(state.scoreGoal) - r.score));
            }
        }
    } else {
        ImGui::TextWrapped("Nenhuma rajada ainda.");
    }

    widgets::EndCard();
}

void BottomBar(AppState& state, double nowMs) {
    const float h = 48.0f;
    const float leftW = 330.0f;
    ImDrawList* dl = nullptr;
    const float y0 = ImGui::GetCursorPosY();

    widgets::BeginCard("##statsPill", ImVec2(leftW, h));
    {
        dl = ImGui::GetWindowDrawList();
        const ImVec2 wp = ImGui::GetWindowPos();
        icons::Draw(dl, Icon::Refresh, ImVec2(wp.x + 26.0f, wp.y + h * 0.5f), 16.0f, theme::U32(theme::kText));
        char text[96];
        if (state.bursts == 0) {
            std::snprintf(text, sizeof(text), "Rajadas: 0");
        } else {
            std::snprintf(text, sizeof(text), "Rajadas: %d  ·  Melhor nota: %.0f", state.bursts,
                          static_cast<double>(state.bestScore));
        }
        ImGui::PushFont(theme::Fonts().bold);
        dl->AddText(ImVec2(wp.x + 46.0f, wp.y + (h - ImGui::GetFontSize()) * 0.5f), theme::U32(theme::kText), text);
        ImGui::PopFont();
    }
    widgets::EndCard();

    ImGui::SameLine(0.0f, 8.0f);
    ImGui::SetCursorPosY(y0 + 4.0f);
    if (widgets::IconButton("##resetStats", Icon::Trash, 40.0f, false)) {
        state.bursts = 0;
        state.bestScore = 0.0f;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Zerar estatísticas da sessão");
    }

    ImGui::SameLine(0.0f, 8.0f);
    ImGui::SetCursorPosY(y0);
    widgets::BeginCard("##sessionPill", ImVec2(0.0f, h));
    {
        dl = ImGui::GetWindowDrawList();
        const ImVec2 wp = ImGui::GetWindowPos();
        const float ww = ImGui::GetWindowWidth();
        icons::Draw(dl, Icon::Clock, ImVec2(wp.x + 26.0f, wp.y + h * 0.5f), 16.0f, theme::U32(theme::kText));
        const int totalMin = static_cast<int>((nowMs - state.appStartMs) / 60000.0);
        char text[64];
        std::snprintf(text, sizeof(text), "Sessão: %dh %02dm", totalMin / 60, totalMin % 60);
        dl->AddText(ImVec2(wp.x + 46.0f, wp.y + (h - ImGui::GetFontSize()) * 0.5f), theme::U32(theme::kText), text);
        icons::Draw(dl, Icon::Target, ImVec2(wp.x + ww - 28.0f, wp.y + h * 0.5f), 18.0f,
                    theme::U32(state.trainingEnabled ? theme::kCyan : theme::kTextDim));
    }
    widgets::EndCard();
}

}  // namespace

void Principal(AppState& state, double nowMs) {
    StatusCard(state);
    ConfigCard(state);

    const float bottomH = 48.0f;
    const float spacing = ImGui::GetStyle().ItemSpacing.y;
    const float rowH = std::max(ImGui::GetContentRegionAvail().y - bottomH - spacing, 160.0f);
    const float resultsW = 240.0f;
    TrainingArea(state, nowMs, ImVec2(ImGui::GetContentRegionAvail().x - resultsW - 10.0f, rowH));
    ImGui::SameLine(0.0f, 10.0f);
    ResultsCard(state, ImVec2(resultsW, rowH));
    BottomBar(state, nowMs);
}

}  // namespace st::pages
