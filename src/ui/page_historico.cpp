#include "ui/pages.h"
#include "ui/profiles.h"
#include "ui/spray_plot.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <ctime>
#include <optional>

namespace st::pages {

namespace {

constexpr std::size_t kChartMaxPoints = 60;

std::string FormatTime(std::int64_t timestamp, const char* format) {
    const auto t = static_cast<std::time_t>(timestamp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), format, &tm);
    return buf;
}

std::optional<Weapon> FilterWeapon(const AppState& state) {
    if (state.historyFilter <= 0) {
        return std::nullopt;
    }
    return static_cast<Weapon>(state.historyFilter - 1);
}

void Toolbar(AppState& state) {
    // Lista (não botões): são 8 armas.
    ImGui::SetNextItemWidth(260.0f);
    const char* preview =
        state.historyFilter <= 0 ? "Todas as armas" : GetWeaponInfo(static_cast<Weapon>(state.historyFilter - 1)).name.data();
    if (ImGui::BeginCombo("##histFilter", preview)) {
        for (int i = 0; i <= static_cast<int>(kWeaponCount); ++i) {
            const char* label = i == 0 ? "Todas as armas" : GetWeaponInfo(static_cast<Weapon>(i - 1)).name.data();
            if (ImGui::Selectable(label, state.historyFilter == i)) {
                state.historyFilter = i;
                state.historySelected = -1;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 190.0f);
    const icons::Icon trash = icons::Icon::Trash;
    ImGui::BeginDisabled(state.history.Entries().empty());
    if (widgets::OutlineButton("Limpar histórico##clear", &trash, ImVec2(190.0f, 36.0f))) {
        state.confirmClearHistory = true;
    }
    ImGui::EndDisabled();
}

// Número em destaque (sem gráfico): rótulo pequeno + valor grande.
void StatTile(const char* id, const char* label, const char* value, float width) {
    widgets::BeginCard(id, ImVec2(width, 92.0f));
    widgets::Caption(label, theme::kTextDim);
    ImGui::PushFont(theme::Fonts().title);
    ImGui::TextUnformatted(value);
    ImGui::PopFont();
    widgets::EndCard();
}

void Stats(const AppState& state) {
    const HistoryStats s = state.history.Stats(FilterWeapon(state), 10);
    const float gap = 10.0f;
    const float w = (ImGui::GetContentRegionAvail().x - 3.0f * gap) / 4.0f;
    char buf[4][32] = {"-", "-", "-", "-"};
    std::snprintf(buf[0], sizeof(buf[0]), "%d", s.count);
    if (s.count > 0) {
        std::snprintf(buf[1], sizeof(buf[1]), "%.0f", static_cast<double>(s.mean));
        std::snprintf(buf[2], sizeof(buf[2]), "%.0f", static_cast<double>(s.best));
        std::snprintf(buf[3], sizeof(buf[3]), "%.0f", static_cast<double>(s.recentMean));
    }
    StatTile("##tileCount", "RAJADAS", buf[0], w);
    ImGui::SameLine(0.0f, gap);
    StatTile("##tileMean", "NOTA MÉDIA", buf[1], w);
    ImGui::SameLine(0.0f, gap);
    StatTile("##tileBest", "MELHOR NOTA", buf[2], w);
    ImGui::SameLine(0.0f, gap);
    StatTile("##tileRecent", "MÉDIA DAS ÚLTIMAS 10", buf[3], w);
}

// Linha da nota por rajada (uma série: sem legenda, o título nomeia). Passar o mouse mostra a
// rajada mais próxima; clicar seleciona para ver o replay.
void ScoreChart(AppState& state, const std::vector<std::size_t>& idx, ImVec2 size) {
    widgets::BeginCard("##scoreChart", size);
    ImGui::PushFont(theme::Fonts().bold);
    ImGui::TextUnformatted("Nota por rajada");
    ImGui::PopFont();
    const std::size_t first = idx.size() > kChartMaxPoints ? idx.size() - kChartMaxPoints : 0;
    const std::size_t n = idx.size() - first;
    ImGui::SameLine();
    if (idx.size() > kChartMaxPoints) {
        ImGui::TextColored(theme::kTextDim, "(últimas %zu)", n);
    } else {
        ImGui::TextUnformatted("");
    }

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float axisW = 34.0f;
    const ImVec2 a(origin.x + axisW, origin.y + 8.0f);
    const ImVec2 b(origin.x + avail.x - 8.0f, origin.y + avail.y - 8.0f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const auto yOf = [&](float score) { return b.y - (b.y - a.y) * (score / 100.0f); };

    // Grade discreta e eixo 0-100.
    for (int v = 0; v <= 100; v += 25) {
        const float y = yOf(static_cast<float>(v));
        dl->AddLine(ImVec2(a.x, y), ImVec2(b.x, y), theme::U32(theme::kBorder, v == 0 ? 1.0f : 0.5f), 1.0f);
        char label[8];
        std::snprintf(label, sizeof(label), "%d", v);
        const ImVec2 ts = ImGui::CalcTextSize(label);
        dl->AddText(ImVec2(a.x - ts.x - 8.0f, y - ts.y * 0.5f), theme::U32(theme::kTextDim), label);
    }

    // Meta atual como linha de referência tracejada.
    if (state.scoreGoal > 0) {
        const float y = yOf(static_cast<float>(state.scoreGoal));
        for (float x = a.x; x < b.x; x += 10.0f) {
            dl->AddLine(ImVec2(x, y), ImVec2(std::min(x + 5.0f, b.x), y), theme::U32(theme::kTextDim), 1.0f);
        }
        char label[24];
        std::snprintf(label, sizeof(label), "meta %d", state.scoreGoal);
        const ImVec2 ts = ImGui::CalcTextSize(label);
        dl->AddText(ImVec2(b.x - ts.x, y - ts.y - 2.0f), theme::U32(theme::kTextDim), label);
    }

    if (n == 0) {
        const char* empty = "Nenhuma rajada ainda. Faça uma na tela Principal.";
        const ImVec2 ts = ImGui::CalcTextSize(empty);
        dl->AddText(ImVec2(a.x + (b.x - a.x - ts.x) * 0.5f, a.y + (b.y - a.y - ts.y) * 0.5f),
                    theme::U32(theme::kTextDim), empty);
        widgets::EndCard();
        return;
    }

    const auto& entries = state.history.Entries();
    const float step = n > 1 ? (b.x - a.x) / static_cast<float>(n - 1) : 0.0f;
    const auto xOf = [&](std::size_t k) { return n > 1 ? a.x + step * static_cast<float>(k) : (a.x + b.x) * 0.5f; };
    std::vector<ImVec2> pts;
    pts.reserve(n);
    for (std::size_t k = 0; k < n; ++k) {
        pts.push_back(ImVec2(xOf(k), yOf(entries[idx[first + k]].score)));
    }
    const ImU32 lineCol = theme::U32(theme::kChartIdeal);
    dl->AddPolyline(pts.data(), static_cast<int>(pts.size()), lineCol, 0, 2.0f);
    for (std::size_t k = 0; k < n; ++k) {
        dl->AddCircleFilled(pts[k], 5.0f, theme::U32(theme::kPanel));  // anel na cor do fundo
        dl->AddCircleFilled(pts[k], 4.0f, lineCol);
        if (static_cast<int>(idx[first + k]) == state.historySelected) {
            dl->AddCircle(pts[k], 8.0f, theme::U32(theme::kText), 0, 2.0f);
        }
    }

    // Área de hover maior que os marcadores: a coluna inteira de cada rajada.
    ImGui::SetCursorScreenPos(ImVec2(a.x - step * 0.5f, a.y));
    ImGui::InvisibleButton("##chartHit", ImVec2(b.x - a.x + step, b.y - a.y));
    if (ImGui::IsItemHovered()) {
        const float mx = ImGui::GetIO().MousePos.x;
        std::size_t k = 0;
        if (n > 1) {
            const float f = (mx - a.x) / step + 0.5f;
            k = static_cast<std::size_t>(std::clamp(f, 0.0f, static_cast<float>(n - 1)));
        }
        const std::size_t entryIndex = idx[first + k];
        const BurstRecord& r = entries[entryIndex];
        dl->AddLine(ImVec2(pts[k].x, a.y), ImVec2(pts[k].x, b.y), theme::U32(theme::kTextDim, 0.6f), 1.0f);
        dl->AddCircle(pts[k], 7.0f, theme::U32(theme::kText), 0, 2.0f);
        ImGui::BeginTooltip();
        ImGui::PushFont(theme::Fonts().bold);
        ImGui::Text("%.0f pontos", static_cast<double>(r.score));
        ImGui::PopFont();
        ImGui::TextColored(theme::kTextDim, "%s  ·  %s", GetWeaponInfo(r.weapon).name.data(),
                           FormatTime(r.timestamp, "%d/%m %H:%M").c_str());
        ImGui::TextColored(theme::kTextDim, "Erro médio %.2f°  ·  clique para ver", static_cast<double>(r.meanError));
        ImGui::EndTooltip();
        if (ImGui::IsItemClicked()) {
            state.historySelected = static_cast<int>(entryIndex);
        }
    }
    widgets::EndCard();
}

void BurstTable(AppState& state, const std::vector<std::size_t>& idx, ImVec2 size) {
    widgets::BeginCard("##burstTable", size);
    const auto& entries = state.history.Entries();
    const ImGuiTableFlags flags =
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY | ImGuiTableFlags_PadOuterX;
    if (ImGui::BeginTable("##bursts", 4, flags, ImGui::GetContentRegionAvail())) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Quando", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Arma", ImGuiTableColumnFlags_WidthFixed, 74.0f);
        ImGui::TableSetupColumn("Nota", ImGuiTableColumnFlags_WidthFixed, 48.0f);
        ImGui::TableSetupColumn("Erro", ImGuiTableColumnFlags_WidthFixed, 58.0f);
        ImGui::TableHeadersRow();
        for (auto it = idx.rbegin(); it != idx.rend(); ++it) {  // mais recente primeiro
            const BurstRecord& r = entries[*it];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushID(static_cast<int>(*it));
            const std::string when = FormatTime(r.timestamp, "%d/%m  %H:%M:%S");
            if (ImGui::Selectable(when.c_str(), state.historySelected == static_cast<int>(*it),
                                  ImGuiSelectableFlags_SpanAllColumns)) {
                state.historySelected = static_cast<int>(*it);
            }
            ImGui::PopID();
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(GetWeaponInfo(r.weapon).name.data());
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.0f", static_cast<double>(r.score));
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.2f°", static_cast<double>(r.meanError));
        }
        ImGui::EndTable();
    }
    widgets::EndCard();
}

void Replay(const AppState& state, ImVec2 size) {
    widgets::BeginCard("##replay", size);
    const auto& entries = state.history.Entries();
    if (state.historySelected < 0 || static_cast<std::size_t>(state.historySelected) >= entries.size()) {
        widgets::Caption("REPLAY", theme::kTextDim);
        ImGui::TextColored(theme::kTextDim, "Selecione uma rajada no gráfico ou na lista.");
        widgets::EndCard();
        return;
    }
    const BurstRecord& r = entries[static_cast<std::size_t>(state.historySelected)];
    widgets::Caption("REPLAY", theme::kTextDim);
    ImGui::PushFont(theme::Fonts().bold);
    ImGui::Text("%s  ·  %.0f pontos", GetWeaponInfo(r.weapon).name.data(), static_cast<double>(r.score));
    ImGui::PopFont();
    ImGui::TextColored(theme::kTextDim, "%s  ·  %zu tiros  ·  erro médio %.2f°",
                       FormatTime(r.timestamp, "%d/%m/%Y %H:%M").c_str(), r.player.size(),
                       static_cast<double>(r.meanError));

    std::vector<ShotResult> shots;
    shots.reserve(r.player.size());
    for (std::size_t i = 0; i < r.player.size(); ++i) {
        shots.push_back({r.ideal[i], r.player[i], 0.0f});
    }
    const ImVec2 a = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float legendH = 2.0f * (ImGui::GetTextLineHeight() + ImGui::GetStyle().ItemSpacing.y) + 6.0f;
    const ImVec2 b(a.x + avail.x, a.y + avail.y - legendH);
    DrawSprayPlot(ImGui::GetWindowDrawList(), a, b, r.ideal, shots, nullptr, PlotOptionsFrom(state.visual));
    ImGui::SetCursorScreenPos(ImVec2(a.x, b.y + 6.0f));
    const VisualSettings& v = state.visual;
    ImGui::BeginGroup();
    PlotLegend(v);
    ImGui::EndGroup();
    widgets::EndCard();
}

void ClearModal(AppState& state) {
    const char* title = "Limpar histórico";
    if (state.confirmClearHistory && !ImGui::IsPopupOpen(title)) {
        ImGui::OpenPopup(title);
    }
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f), ImGuiCond_Appearing,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(440.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 20.0f));
    if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::TextWrapped("Apagar as %zu rajadas do histórico? Isso não pode ser desfeito.",
                           state.history.Entries().size());
        ImGui::Spacing();
        const bool yes = widgets::OutlineButton("Apagar##clearYes", nullptr, ImVec2(180.0f, 38.0f));
        ImGui::SameLine(0.0f, 10.0f);
        const bool no = widgets::OutlineButton("Cancelar##clearNo", nullptr, ImVec2(180.0f, 38.0f)) ||
                        ImGui::IsKeyPressed(ImGuiKey_Escape);
        if (yes) {
            profiles::ClearHistory(state);
        }
        if (yes || no) {
            state.confirmClearHistory = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

}  // namespace

void Historico(AppState& state) {
    const std::vector<std::size_t> idx = state.history.Filter(FilterWeapon(state));
    Toolbar(state);
    Stats(state);

    const float avail = ImGui::GetContentRegionAvail().y;
    const float chartH = std::max(avail * 0.36f, 150.0f);
    ScoreChart(state, idx, ImVec2(0.0f, chartH));

    const float rowH = ImGui::GetContentRegionAvail().y;
    const float tableW = std::min(ImGui::GetContentRegionAvail().x * 0.45f, 420.0f);
    BurstTable(state, idx, ImVec2(tableW, rowH));
    ImGui::SameLine(0.0f, 10.0f);
    Replay(state, ImVec2(0.0f, rowH));
    ClearModal(state);
}

}  // namespace st::pages
