#include "ui/pattern_editor.h"

#include "core/pattern_recording.h"
#include "ui/profiles.h"
#include "ui/spray_plot.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cstdio>

namespace st {

namespace {

constexpr float kMaxDegrees = 10.0f;

bool SamePattern(const Pattern& a, const Pattern& b) {
    for (std::size_t i = 0; i < kPatternPoints; ++i) {
        if (a[i].x != b[i].x || a[i].y != b[i].y) {
            return false;
        }
    }
    return true;
}

void ActionButtons(AppState& state, Weapon weapon, const Pattern& current, float k, bool compact) {
    Pattern& saved = state.savedPatterns[Index(weapon)];
    const bool dirty = !SamePattern(current, saved);
    const bool isDefault = SamePattern(current, DefaultPattern(weapon));
    const float h = 36.0f * k;
    const float gap = 8.0f * k;
    const float w = compact ? (ImGui::GetContentRegionAvail().x - 2.0f * gap) / 3.0f : 0.0f;

    ImGui::PushID(static_cast<int>(weapon));
    ImGui::BeginDisabled(!dirty);
    if (widgets::OutlineButton(compact ? "Salvar##save" : "Salvar alterações##save", nullptr, ImVec2(w, h))) {
        // Grava o perfil inteiro (sem perfil atual, pede um nome na janela principal).
        if (state.profiles.store != nullptr) {
            profiles::SaveCurrent(state);
        } else {
            saved = current;
        }
    }
    ImGui::SameLine(0.0f, gap);
    if (widgets::OutlineButton("Desfazer##undo", nullptr, ImVec2(w, h))) {
        state.patterns.Set(weapon, saved);
    }
    ImGui::EndDisabled();
    ImGui::SameLine(0.0f, gap);
    ImGui::BeginDisabled(isDefault);
    if (widgets::OutlineButton(compact ? "Padrão##reset" : "Restaurar padrão##reset", nullptr, ImVec2(w, h))) {
        state.patterns.ResetToDefault(weapon);
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Volta aos valores de fábrica desta arma");
    }
    if (!compact) {
        // Gravar desenhando com o mouse (só na janela principal: o overlay não lê o mouse).
        PatternRecorder& rec = state.recorder;
        const bool recording = rec.mode != RecorderMode::Off;
        const icons::Icon icon = recording ? icons::Icon::Trash : icons::Icon::Mouse;
        ImGui::SameLine(0.0f, gap);
        ImGui::BeginDisabled(rec.session.Running());
        if (widgets::OutlineButton(recording ? "Cancelar gravação##rec" : "Gravar com o mouse##rec", &icon,
                                   ImVec2(0.0f, h))) {
            rec.mode = recording ? RecorderMode::Off : RecorderMode::Ready;
            rec.weapon = weapon;
            rec.path.clear();
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered() && !recording) {
            ImGui::SetTooltip("Desenhe a rajada segurando o botão: o desenho vira o padrão desta arma");
        }
    }
    ImGui::PopID();

    if (!compact) {
        ImGui::SameLine(0.0f, 16.0f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (h - ImGui::GetFontSize()) * 0.5f);
    }
    if (dirty) {
        ImGui::TextColored(theme::kPink, "Alterações não salvas");
    } else {
        ImGui::TextColored(theme::kTextDim, isDefault ? "Valores de fábrica" : "Tudo salvo");
    }
}

// Devolve a linha sob o mouse (ou em edição), -1 se nenhuma.
int PatternTable(AppState& state, Weapon weapon, const Pattern& p, ImVec2 size, float k) {
    const int magazine = GetWeaponInfo(weapon).magazine;
    int hovered = -1;
    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY |
                                  ImGuiTableFlags_BordersOuter | ImGuiTableFlags_PadOuterX;
    ImGui::PushID(static_cast<int>(weapon));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f * k, 3.0f * k));
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6.0f * k, 2.0f * k));
    if (ImGui::BeginTable("##pattern", 3, flags, size)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 34.0f * k);
        ImGui::TableSetupColumn("X (°)", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Y (°)", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(kPatternPoints); ++i) {
            const auto idx = static_cast<std::size_t>(i);
            ImGui::TableNextRow();
            ImGui::PushID(i);
            if (i == state.editorRow) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, theme::U32(theme::Accent(), 0.18f));
            }

            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(i < magazine ? theme::kText : theme::kTextDim, "%d", i);

            Vec2 v = p[idx];
            bool rowActive = false;
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::DragFloat("##x", &v.x, 0.005f, -kMaxDegrees, kMaxDegrees, "%.3f",
                                 ImGuiSliderFlags_AlwaysClamp)) {
                state.patterns.SetPoint(weapon, idx, v);
            }
            rowActive |= ImGui::IsItemActive();
            hovered = ImGui::IsItemHovered() ? i : hovered;

            ImGui::TableSetColumnIndex(2);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::DragFloat("##y", &v.y, 0.005f, -kMaxDegrees, kMaxDegrees, "%.3f",
                                 ImGuiSliderFlags_AlwaysClamp)) {
                state.patterns.SetPoint(weapon, idx, v);
            }
            rowActive |= ImGui::IsItemActive();
            hovered = ImGui::IsItemHovered() ? i : hovered;

            if (rowActive) {
                state.editorRow = i;
                hovered = i;
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopID();
    return hovered;
}

void PatternPlot(const Pattern& p, Weapon weapon, int highlight, ImVec2 size, float k) {
    const ImVec2 a = ImGui::GetCursorScreenPos();
    const ImVec2 b(a.x + size.x, a.y + size.y);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(a, b, theme::U32(theme::kBg), 8.0f);
    dl->AddRect(a, b, theme::U32(theme::kBorder), 8.0f);
    PlotOptions options;
    options.grid = true;
    options.shotNumbers = true;
    options.highlight = highlight;
    options.activeCount = static_cast<std::size_t>(GetWeaponInfo(weapon).magazine);
    DrawSprayPlot(dl, a, ImVec2(b.x, b.y - 22.0f * k), IdealPath(p, kPatternPoints), {}, nullptr, options);

    ImGui::PushFont(theme::Fonts().caption);
    const char* legend = "Linha: movimento ideal do mouse  ·  cinza: fora do pente";
    const ImVec2 ts = ImGui::CalcTextSize(legend);
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(a.x + (size.x - ts.x) * 0.5f, b.y - ts.y - 8.0f * k),
                theme::U32(theme::kTextDim), legend);
    ImGui::PopFont();
    ImGui::Dummy(size);
}

// Área de desenho: segure o botão e puxe o mouse como faria para controlar o recuo.
void RecordCanvas(AppState& state, Weapon weapon, const Pattern& current, ImVec2 size, float k) {
    PatternRecorder& rec = state.recorder;
    const WeaponInfo& info = GetWeaponInfo(weapon);
    const ImVec2 a = ImGui::GetCursorScreenPos();
    const ImVec2 b(a.x + size.x, a.y + size.y);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(a, b, theme::U32(theme::kBg), 8.0f);
    dl->AddRect(a, b, theme::U32(rec.session.Running() ? theme::kPink : theme::Accent()), 8.0f, 0, 2.0f);

    const ImGuiButtonFlags flags =
        state.fireButton == FireButton::Left ? ImGuiButtonFlags_MouseButtonLeft : ImGuiButtonFlags_MouseButtonRight;
    ImGui::InvisibleButton("##recordArea", size, flags);
    if (ImGui::IsItemActivated()) {
        rec.session.Start(weapon, current, state.sensitivity, state.nowMs);
    }
    if (ImGui::IsItemDeactivated() && rec.session.Running()) {
        rec.session.Stop(state.nowMs);
    }
    state.captureMouse = rec.session.Running();
    if (state.captureMouse) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    }

    if (rec.session.GetState() == TrainingSession::State::Done) {
        const SessionResult r = rec.session.Result();
        rec.session.Reset();
        if (r.shots.size() >= 3) {
            rec.path.clear();
            for (const ShotResult& s : r.shots) {
                rec.path.push_back(s.player);
            }
            rec.mode = RecorderMode::Review;
        } else {
            profiles::Notify(state, "Rajada curta demais: segure o botão por mais tempo.", true);
        }
    }

    // O padrão atual aparece como referência; a sua linha é o desenho.
    PlotOptions options = PlotOptionsFrom(state.visual);
    options.activeCount = static_cast<std::size_t>(info.magazine);
    const std::vector<Vec2> reference = IdealPath(current, static_cast<std::size_t>(info.magazine));
    const ImVec2 plotA(a.x, a.y + 54.0f * k);
    if (rec.session.Running()) {
        const Vec2 live = rec.session.LivePlayer();
        DrawSprayPlot(dl, plotA, b, reference, rec.session.Shots(), &live, options);
    } else {
        DrawSprayPlot(dl, plotA, b, reference, {}, nullptr, options);
    }

    char title[96];
    if (rec.session.Running()) {
        std::snprintf(title, sizeof(title), "Gravando...  tiro %zu de %zu", rec.session.ShotsFired(),
                      rec.session.ShotsTotal());
    } else {
        std::snprintf(title, sizeof(title), "Desenhe a rajada da %s", info.name.data());
    }
    ImGui::PushFont(theme::Fonts().bold);
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(a.x + 14.0f * k, a.y + 10.0f * k),
                theme::U32(rec.session.Running() ? theme::kPink : theme::kText), title);
    ImGui::PopFont();
    char hint[160];
    std::snprintf(hint, sizeof(hint), "Segure o botão %s aqui e puxe como faria no jogo. A cada %.0f ms vira um tiro.",
                  state.fireButton == FireButton::Left ? "esquerdo" : "direito",
                  static_cast<double>(info.fireIntervalMs));
    dl->AddText(ImVec2(a.x + 14.0f * k, a.y + 32.0f * k), theme::U32(theme::kTextDim), hint);
}

// Revisão: o padrão gerado pelo desenho, com suavização ajustável, antes de aplicar.
void RecordReview(AppState& state, Weapon weapon, const Pattern& current, ImVec2 size, float k) {
    PatternRecorder& rec = state.recorder;
    const WeaponInfo& info = GetWeaponInfo(weapon);
    const Pattern recorded = PatternFromPath(rec.path, current, rec.smoothing);

    const ImVec2 start = ImGui::GetCursorScreenPos();
    ImGui::BeginGroup();
    ImGui::PushFont(theme::Fonts().bold);
    ImGui::Text("Padrão gravado: %zu de %d tiros", rec.path.size(), info.magazine);
    ImGui::PopFont();
    if (rec.path.size() < static_cast<std::size_t>(info.magazine)) {
        ImGui::TextColored(theme::kTextDim, "Os tiros que faltaram mantêm os valores atuais.");
    } else {
        ImGui::TextColored(theme::kTextDim, "Confira o desenho e aplique ao padrão da arma.");
    }

    const char* levels[] = {"Sem suavizar", "Leve", "Média", "Forte"};
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Suavização:");
    ImGui::SameLine();
    widgets::Segmented("##smooth", levels, 4, &rec.smoothing, ImVec2(std::min(size.x - 110.0f * k, 420.0f * k), 32.0f * k));

    const float btnH = 36.0f * k;
    const icons::Icon apply = icons::Icon::Target;
    if (widgets::OutlineButton("Aplicar ao padrão##apply", &apply, ImVec2(0.0f, btnH))) {
        state.patterns.Set(weapon, recorded);
        rec.mode = RecorderMode::Off;
        rec.path.clear();
        profiles::Notify(state, std::string("Padrão da ") + info.name.data() +
                                    " atualizado com o seu desenho. Use \"Salvar alterações\" para guardar no perfil.");
    }
    ImGui::SameLine(0.0f, 8.0f * k);
    if (widgets::OutlineButton("Gravar de novo##again", nullptr, ImVec2(0.0f, btnH))) {
        rec.mode = RecorderMode::Ready;
        rec.path.clear();
    }
    ImGui::SameLine(0.0f, 8.0f * k);
    if (widgets::OutlineButton("Cancelar##cancelRec", nullptr, ImVec2(0.0f, btnH))) {
        rec.mode = RecorderMode::Off;
        rec.path.clear();
    }
    ImGui::EndGroup();

    // Altura do próprio grupo (o cursor pularia a altura da tabela, que está na mesma linha).
    const float used = ImGui::GetItemRectSize().y + 10.0f * k;
    const ImVec2 a(start.x, start.y + used);
    const ImVec2 b(start.x + size.x, start.y + size.y);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(a, b, theme::U32(theme::kBg), 8.0f);
    dl->AddRect(a, b, theme::U32(theme::kBorder), 8.0f);

    // Desenho (sua linha) sobre o padrão novo (linha ideal): mostra o efeito da suavização.
    std::vector<ShotResult> drawn;
    const std::vector<Vec2> ideal = IdealPath(recorded, rec.path.size());
    for (std::size_t i = 0; i < rec.path.size(); ++i) {
        drawn.push_back({ideal[i], rec.path[i], 0.0f});
    }
    PlotOptions options = PlotOptionsFrom(state.visual);
    options.shotNumbers = true;
    DrawSprayPlot(dl, ImVec2(a.x, a.y + 30.0f * k), b, ideal, drawn, nullptr, options);
    ImGui::SetCursorScreenPos(ImVec2(a.x + 10.0f * k, a.y + 8.0f * k));
    ImGui::BeginGroup();
    PlotLegend(state.visual, "Padrão novo", "Seu desenho");
    ImGui::EndGroup();
    ImGui::SetCursorScreenPos(start);
    ImGui::Dummy(size);
}

}  // namespace

bool PatternDirty(const AppState& state, Weapon weapon) {
    return !SamePattern(state.patterns.Get(weapon), state.savedPatterns[Index(weapon)]);
}

void PatternEditor(AppState& state, Weapon weapon, const PatternEditorLayout& layout) {
    const float k = layout.scale;
    ActionButtons(state, weapon, state.patterns.Get(weapon), k, layout.stacked);
    ImGui::Spacing();

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    // Lê de novo após os botões (Desfazer/Restaurar podem ter mudado o padrão).
    const Pattern p = state.patterns.Get(weapon);
    if (layout.stacked) {
        const float plotH = std::min(layout.plotHeight, avail.y * 0.45f);
        PatternPlot(p, weapon, state.editorRow, ImVec2(avail.x, plotH), k);
        PatternTable(state, weapon, p, ImVec2(avail.x, ImGui::GetContentRegionAvail().y), k);
        return;
    }

    // Trocar de arma no meio da gravação cancela (o desenho era para a outra arma).
    PatternRecorder& rec = state.recorder;
    if (rec.mode != RecorderMode::Off && rec.weapon != weapon && !rec.session.Running()) {
        rec.mode = RecorderMode::Off;
        rec.path.clear();
    }
    if (rec.mode != RecorderMode::Off && !rec.session.Running() && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        rec.mode = RecorderMode::Off;
        rec.path.clear();
    }

    const float tableW = std::max(avail.x * 0.42f, 300.0f);
    const int hovered = PatternTable(state, weapon, p, ImVec2(tableW, avail.y), k);
    ImGui::SameLine(0.0f, 12.0f);
    const ImVec2 rightSize(ImGui::GetContentRegionAvail().x, avail.y);
    switch (rec.mode) {
        case RecorderMode::Ready:
            RecordCanvas(state, weapon, state.patterns.Get(weapon), rightSize, k);
            break;
        case RecorderMode::Review:
            RecordReview(state, weapon, state.patterns.Get(weapon), rightSize, k);
            break;
        case RecorderMode::Off:
            PatternPlot(state.patterns.Get(weapon), weapon, hovered >= 0 ? hovered : state.editorRow, rightSize, k);
            break;
    }
}

}  // namespace st
