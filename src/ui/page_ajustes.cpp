#include "ui/pages.h"
#include "core/repetition.h"
#include "ui/pattern_editor.h"
#include "ui/spray_plot.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <string>
#include <vector>

namespace st::pages {

namespace {

using icons::Icon;

enum SettingsTab { kTabPatterns = 0, kTabTraining, kTabRepetition, kTabOverlay, kTabInterface };

struct AccentPreset {
    const char* name;
    ImVec4 color;
};

constexpr AccentPreset kAccentPresets[] = {
    {"Roxo", {0.616f, 0.306f, 0.867f, 1.0f}},   // #9D4EDD
    {"Azul", {0.161f, 0.475f, 1.0f, 1.0f}},     // #2979FF
    {"Verde", {0.298f, 0.686f, 0.314f, 1.0f}},  // #4CAF50
    {"Laranja", {1.0f, 0.596f, 0.0f, 1.0f}},    // #FF9800
    {"Amarelo", {1.0f, 0.839f, 0.0f, 1.0f}},    // #FFD600
};

bool SameColor(const ImVec4& a, const ImVec4& b) {
    return std::abs(a.x - b.x) < 0.002f && std::abs(a.y - b.y) < 0.002f && std::abs(a.z - b.z) < 0.002f;
}

void SetAccent(AppState& state, const ImVec4& color) {
    state.ui.accent = color;
    theme::SetAccent(color);
    theme::Apply();  // contexto da janela principal; o overlay reaplica sozinho
}

// ------------------------------------------------------------------ Padrões de recuo

void TabPatterns(AppState& state) {
    widgets::BeginCard("##patternsCard", ImVec2(0.0f, 0.0f));
    ImGui::PushFont(theme::Fonts().bold);
    ImGui::TextUnformatted("Padrão de recuo por arma");
    ImGui::PopFont();
    ImGui::TextColored(theme::kTextDim,
                       "Cada linha é o deslocamento (em graus) que o tiro soma ao anterior. "
                       "Arraste o valor ou dê dois cliques para digitar.");
    ImGui::Spacing();

    const float tilesW = ImGui::GetContentRegionAvail().x;
    Weapon w = state.editorWeapon;
    if (widgets::WeaponTiles("##editorWeapon", state.weaponIcons, &w, tilesW, 0.9f, false, 7)) {
        state.editorWeapon = w;
        state.editorRow = -1;
    }
    ImGui::Spacing();

    PatternEditorLayout layout;
    PatternEditor(state, state.editorWeapon, layout);
    widgets::EndCard();
}

// ------------------------------------------------------------------ Treino

void TabTraining(AppState& state) {
    widgets::BeginCard("##trainingCard", ImVec2(0.0f, 0.0f));
    if (widgets::BeginSettings("##trainingRows")) {
        widgets::SettingLabel("Sensibilidade no jogo", "A mesma do jogo; converte o movimento do mouse em graus.");
        ImGui::SetNextItemWidth(220.0f);
        ImGui::BeginDisabled(state.session.Running());
        if (ImGui::InputFloat("##sens", &state.sensitivity, 0.05f, 0.25f, "%.2f")) {
            state.sensitivity = std::clamp(state.sensitivity, 0.05f, 20.0f);
        }
        ImGui::EndDisabled();

        widgets::SettingLabel("Botão de disparo", "Botão que você segura na área de treino para disparar a rajada.");
        const char* buttons[] = {"Esquerdo", "Direito"};
        int fire = static_cast<int>(state.fireButton);
        if (widgets::Segmented("##fire", buttons, 2, &fire, ImVec2(260.0f, 38.0f))) {
            state.fireButton = static_cast<FireButton>(fire);
        }

        widgets::SettingLabel("Meta de nota", "Opcional. O resultado mostra se a meta foi batida.");
        ImGui::SetNextItemWidth(260.0f);
        ImGui::SliderInt("##goal", &state.scoreGoal, 0, 100, state.scoreGoal == 0 ? "Nenhuma" : "%d pontos",
                         ImGuiSliderFlags_AlwaysClamp);

        widgets::SettingLabel("Rigor da nota",
                              "Erro médio (em graus) que já dá nota zero. Menor = mais exigente. Padrão: 2.0°.");
        ImGui::SetNextItemWidth(260.0f);
        ImGui::SliderFloat("##tolerance", &state.scoreTolerance, 0.5f, 6.0f, "%.1f°", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SameLine();
        if (ImGui::SmallButton("Padrão##tol")) {
            state.scoreTolerance = TrainingSession::kScoreTolerance;
        }

        widgets::SettingLabel("Treino ligado", "Mesmo interruptor da tela Principal.");
        widgets::Toggle("##enabled", &state.trainingEnabled, 30.0f);
        widgets::EndSettings();
    }
    widgets::EndCard();
}

// ------------------------------------------------------------------ Repetição automática

// Índice em kKeyOptions de um VK; -1 se a tecla não está na lista (ex.: perfil antigo).
int KeyOptionIndex(int vk) {
    for (std::size_t i = 0; i < kKeyOptionCount; ++i) {
        if (kKeyOptions[i].vk == vk) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// Botões do mouse: RegisterHotKey não os aceita (WM_HOTKEY é gerado só por teclado),
// então o app os vigia via GetAsyncKeyState. A UI mostra isso no modo de ativação.
bool IsMouseButtonVk(int vk) {
    return vk == kVkLButton || vk == kVkRButton || vk == kVkMButton ||
           vk == kVkXButton1 || vk == kVkXButton2;
}

// ------------------------------------------------------------------ Repetição automática

void TabRepetition(AppState& state) {
    widgets::BeginCard("##repetitionCard", ImVec2(0.0f, 0.0f));
    if (widgets::BeginSettings("##repetitionRows")) {
        widgets::SettingLabel("Repetição automática ligada",
                              "Segure a tecla configurada e o app reproduz o padrão da arma selecionada "
                              "no lugar do seu movimento (funciona com o jogo em foco).");
        widgets::Toggle("##repEnabled", &state.repetition.enabled, 30.0f);

        widgets::SettingLabel("Botão para repetir",
                              "Padrão: LBUTTON (botão de atirar) — segura o clique e o replay faz a "
                              "rajada; solta, para. A tecla \"1\" não dispara aqui: ela apenas RELIGA o "
                              "replay depois de uma tecla de desativar ter cortado. Todas as teclas "
                              "listadas funcionam normalmente no jogo enquanto o app vigia (nenhuma "
                              "rouba o toque da janela em foco).");
        ImGui::SetNextItemWidth(260.0f);
        int keyIdx = KeyOptionIndex(state.repetition.repeatKeyVk);
        const char* preview = keyIdx >= 0 ? kKeyOptions[keyIdx].name : "—";
        if (ImGui::BeginCombo("##repeatKey", preview)) {
            for (std::size_t i = 0; i < kKeyOptionCount; ++i) {
                const bool selected = static_cast<int>(i) == keyIdx;
                // KEY1 está reservada como tecla fixa de religar: fora da lista
                // de disparo (mesma regra do polling em TickRepetition).
                if (kKeyOptions[i].vk == kDefaultEnableKeyVk) {
                    continue;
                }
                if (ImGui::Selectable(kKeyOptions[i].name, selected)) {
                    state.repetition.repeatKeyVk = kKeyOptions[i].vk;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        const bool repeatIsMouse = IsMouseButtonVk(state.repetition.repeatKeyVk);
        widgets::SettingLabel("Modo de ativação",
                              "Enquanto pressionada: repete só com a tecla segurada. "
                              "Alternar: um toque liga e outro desliga a repetição contínua.");
        if (repeatIsMouse) {
            // Botões do mouse não geram WM_HOTKEY; em modo alternar o toque é
            // detectado por polling no loop (TickRepetition).
            const char* modeNames[] = {"Enquanto pressionada", "Alternar (toque liga/desliga)"};
            const int mode = state.repetition.activation == RepeatActivation::Toggle ? 1 : 0;
            ImGui::SetNextItemWidth(260.0f);
            if (ImGui::BeginCombo("##repMode", modeNames[mode])) {
                for (int i = 0; i < 2; ++i) {
                    if (ImGui::Selectable(modeNames[i], i == mode)) {
                        state.repetition.activation = i == 1 ? RepeatActivation::Toggle : RepeatActivation::Hold;
                    }
                }
                ImGui::EndCombo();
            }
        } else if (state.repetition.activation == RepeatActivation::Toggle) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(theme::kCyan, "Alternar (toque liga/desliga)");
            ImGui::SameLine();
            if (ImGui::SmallButton("Mudar para pressionar##actMode")) {
                state.repetition.activation = RepeatActivation::Hold;
            }
        } else {
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(theme::kTextDim, "Enquanto pressionada");
            ImGui::SameLine();
            if (ImGui::SmallButton("Usar como alternar##actMode")) {
                state.repetition.activation = RepeatActivation::Toggle;
            }
        }

        widgets::SettingLabel("Teclas para desativar",
                              "Basta um toque em qualquer uma delas para desativar na hora "
                              "(não precisa segurar). Padrão: teclas 2 a 9 e letras B, G, H. "
                              "São as mesmas teclas de armas secundárias no CS2: ao trocar de arma "
                              "o replay desliga sozinho; só a tecla de ativar (padrão \"1\") religa.");
        // Linha com as duas funções separadas: DISPARAR (repetir o recuo) e
        // ATIVAR/RELIGAR (tecla fixa "1"). Botão "Restaurar padrão" volta ambas.
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(theme::kTextDim, "Repetir (atirar):");
            ImGui::SameLine();
            const std::string_view repName = VirtualKeyName(state.repetition.repeatKeyVk);
            ImGui::TextColored(theme::kCyan, "%.*s", static_cast<int>(repName.size()), repName.data());
            ImGui::SameLine();
            ImGui::TextColored(theme::kTextDim, "| Ativar (religar):");
            ImGui::SameLine();
            ImGui::TextColored(theme::kCyan, "KEY1 (fixo)");
            ImGui::SameLine();
            if (ImGui::SmallButton("Restaurar padrão##restoreRepKeys")) {
                state.repetition.repeatKeyVk = kDefaultRepeatKeyVk;  // LBUTTON (atirar)
                state.repetition.stopKeysVk.assign(kDefaultStopKeysVk,
                                                   kDefaultStopKeysVk + kDefaultStopKeysCount);
                state.repetition.cycleWeaponKeyVk = kDefaultCycleWeaponKeyVk;  // desligado
            }
        }
        for (std::size_t i = 0; i < state.repetition.stopKeysVk.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            const std::string_view name = VirtualKeyName(state.repetition.stopKeysVk[i]);
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(theme::kTextDim, "%.*s", static_cast<int>(name.size()), name.data());
            ImGui::SameLine();
            if (ImGui::SmallButton("Remover")) {
                state.repetition.stopKeysVk.erase(state.repetition.stopKeysVk.begin() +
                                                  static_cast<std::ptrdiff_t>(i));
                ImGui::PopID();
                break;  // o vetor mudou: recomeça a lista no próximo frame
            }
            ImGui::PopID();
        }
        if (state.repetition.stopKeysVk.empty()) {
            ImGui::TextColored(theme::kTextDim, "Nenhuma tecla de desativar configurada.");
        }
        if (state.repetition.stopKeysVk.size() < kMaxStopKeys) {
            ImGui::SetNextItemWidth(260.0f);
            if (ImGui::BeginCombo("##addStopKey", "+ Adicionar tecla...")) {
                for (std::size_t i = 0; i < kKeyOptionCount; ++i) {
                    const int vk = kKeyOptions[i].vk;
                    // "1" é exclusivamente a tecla de religar: nunca entra como
                    // desativar (mesma regra de SyncRepetitionKeys/Tick).
                    const bool used = vk == state.repetition.repeatKeyVk ||
                                      vk == kDefaultEnableKeyVk ||
                                      std::find(state.repetition.stopKeysVk.begin(),
                                                state.repetition.stopKeysVk.end(),
                                                vk) != state.repetition.stopKeysVk.end();
                    ImGui::PushID(static_cast<int>(i));
                    ImGui::BeginDisabled(used);
                    if (!used && ImGui::Selectable(kKeyOptions[i].name)) {
                        state.repetition.stopKeysVk.push_back(vk);
                    }
                    ImGui::EndDisabled();
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
        }

        widgets::SettingLabel("Suavização do movimento",
                              "Divide cada passo do padrão em micro-movimentos. Poucos = rígido; muitos = suave.");
        ImGui::SetNextItemWidth(260.0f);
        ImGui::SliderInt("##substeps", &state.repetition.substeps, 1, PatternRepeater::kMaxSubsteps,
                         "%d por passo", ImGuiSliderFlags_AlwaysClamp);

        widgets::SettingLabel("Devolver à origem",
                              "Ao soltar a tecla, compensa o que foi enviado para o mouse voltar ao ponto inicial.");
        widgets::Toggle("##returnBack", &state.repetition.returnBack, 30.0f);

        widgets::SettingLabel("Recomeçar do início a cada clique",
                              "Clicar de novo durante uma repetição interrompe e reinicia o padrão do primeiro tiro. "
                              "Desligado: o clique extra é ignorado e a rajada atual segue até o fim.");
        widgets::Toggle("##restartFromZero", &state.repetition.restartFromZero, 30.0f);

        widgets::SettingLabel("Tecla para a próxima arma",
                              "Cada toque avança para a próxima arma da lista (padrão: F1; também aceita botões do mouse).");
        ImGui::SetNextItemWidth(260.0f);
        const int cycIdx = KeyOptionIndex(state.repetition.cycleWeaponKeyVk);
        const char* cycPreview = state.repetition.cycleWeaponKeyVk == 0
                                     ? "Desligado"
                                     : (cycIdx >= 0 ? kKeyOptions[cycIdx].name : "—");
        if (ImGui::BeginCombo("##cycleKey", cycPreview)) {
            if (ImGui::Selectable("Desligado", state.repetition.cycleWeaponKeyVk == 0)) {
                state.repetition.cycleWeaponKeyVk = 0;
            }
            for (std::size_t i = 0; i < kKeyOptionCount; ++i) {
                const bool selected = static_cast<int>(i) == cycIdx;
                if (ImGui::Selectable(kKeyOptions[i].name, selected)) {
                    state.repetition.cycleWeaponKeyVk = kKeyOptions[i].vk;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        widgets::SettingLabel("Tecla para a arma anterior",
                              "Cada toque volta para a arma anterior da lista (padrão: F2; também aceita botões do mouse).");
        ImGui::SetNextItemWidth(260.0f);
        const int cbkIdx = KeyOptionIndex(state.repetition.cycleBackKeyVk);
        const char* cbkPreview = state.repetition.cycleBackKeyVk == 0
                                     ? "Desligado"
                                     : (cbkIdx >= 0 ? kKeyOptions[cbkIdx].name : "—");
        if (ImGui::BeginCombo("##cycleBackKey", cbkPreview)) {
            if (ImGui::Selectable("Desligado", state.repetition.cycleBackKeyVk == 0)) {
                state.repetition.cycleBackKeyVk = 0;
            }
            for (std::size_t i = 0; i < kKeyOptionCount; ++i) {
                const bool selected = static_cast<int>(i) == cbkIdx;
                if (ImGui::Selectable(kKeyOptions[i].name, selected)) {
                    state.repetition.cycleBackKeyVk = kKeyOptions[i].vk;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (state.repetition.enabled) {
            ImGui::Spacing();
            ImGui::TextColored(theme::kTextDim,
                               "A hotkey funciona com qualquer janela em foco. Se outra tecla já estiver em uso "
                               "por outro programa, um aviso aparece no topo da janela.");
        }
        widgets::EndSettings();
    }
    widgets::EndCard();
}

// ------------------------------------------------------------------ Overlay

void TabOverlay(AppState& state) {
    OverlaySettings& o = state.overlay;
    widgets::BeginCard("##overlayCard", ImVec2(0.0f, 0.0f));
    if (widgets::BeginSettings("##overlayRows")) {
        widgets::SettingLabel("Mostrar overlay", "Também pelo ícone de olho na barra de título.");
        widgets::Toggle("##visible", &o.visible, 30.0f);

        widgets::SettingLabel("Posição", "Canto da tela (área útil, acima da barra de tarefas).");
        const char* top[] = {"Sup. esquerdo", "Sup. direito"};
        const char* bottom[] = {"Inf. esquerdo", "Inf. direito"};
        int corner = static_cast<int>(o.corner);
        int topSel = corner < 2 ? corner : -1;
        int bottomSel = corner >= 2 ? corner - 2 : -1;
        if (widgets::Segmented("##cornerTop", top, 2, &topSel, ImVec2(300.0f, 36.0f))) {
            o.corner = static_cast<OverlayCorner>(topSel);
        }
        if (widgets::Segmented("##cornerBottom", bottom, 2, &bottomSel, ImVec2(300.0f, 36.0f))) {
            o.corner = static_cast<OverlayCorner>(bottomSel + 2);
        }

        widgets::SettingLabel("Tamanho", "Escala do overlay.");
        ImGui::SetNextItemWidth(300.0f);
        ImGui::SliderFloat("##scale", &o.scale, 0.75f, 1.5f, "%.2fx", ImGuiSliderFlags_AlwaysClamp);

        widgets::SettingLabel("Opacidade", "Transparência do overlay inteiro.");
        float opacityPct = o.opacity * 100.0f;
        ImGui::SetNextItemWidth(300.0f);
        if (ImGui::SliderFloat("##opacity", &opacityPct, 30.0f, 100.0f, "%.0f%%", ImGuiSliderFlags_AlwaysClamp)) {
            o.opacity = opacityPct / 100.0f;
        }

        widgets::SettingLabel("Intervalo de atualização",
                              "Modo compacto. Maior = menos uso de CPU/GPU. Expandido sempre usa 16 ms.");
        ImGui::SetNextItemWidth(300.0f);
        ImGui::SliderInt("##refresh", &o.refreshMs, 16, 250, "%d ms", ImGuiSliderFlags_AlwaysClamp);

        widgets::SettingLabel("Painel", "Abre ou fecha o painel expandido do overlay.");
        if (widgets::OutlineButton(state.overlayExpanded ? "Recolher overlay##exp" : "Expandir overlay##exp", nullptr,
                                   ImVec2(200.0f, 36.0f))) {
            state.overlayExpanded = !state.overlayExpanded;
        }
        widgets::EndSettings();
    }
    widgets::EndCard();
}

// ------------------------------------------------------------------ Interface

void InterfacePreview(AppState& state) {
    widgets::Caption("PRÉ-VISUALIZAÇÃO", theme::kTextDim);
    widgets::BeginCard("##preview", ImVec2(0.0f, 0.0f));
    ImGui::PushFont(theme::Fonts().title);
    ImGui::TextColored(theme::Accent(), "TREINO INATIVO");
    ImGui::PopFont();
    static bool demoToggle = true;
    widgets::Toggle("##demoToggle", &demoToggle, 30.0f);
    ImGui::SameLine(0.0f, 20.0f);
    const Icon mouse = Icon::Mouse;
    widgets::OutlineButton("Botão de exemplo##demo", &mouse, ImVec2(220.0f, 36.0f));

    const ImVec2 a = ImGui::GetCursorScreenPos();
    const ImVec2 size(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
    const Pattern p = state.patterns.Get(state.weapon);
    DrawSprayPlot(ImGui::GetWindowDrawList(), a, ImVec2(a.x + size.x, a.y + size.y), IdealPath(p, 30), {}, nullptr);
    ImGui::Dummy(size);
    widgets::EndCard();
}

void TabInterface(AppState& state) {
    widgets::BeginCard("##interfaceCard", ImVec2(0.0f, 250.0f));
    if (widgets::BeginSettings("##interfaceRows")) {
        widgets::SettingLabel("Cor de destaque",
                              "Títulos, botões e seleção. Os gráficos usam cores próprias, testadas para daltonismo.");
        for (const AccentPreset& preset : kAccentPresets) {
            ImGui::PushID(preset.name);
            const bool selected = SameColor(state.ui.accent, preset.color);
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            if (ImGui::InvisibleButton("##swatch", ImVec2(34.0f, 34.0f))) {
                SetAccent(state, preset.color);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", preset.name);
            }
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(pos, ImVec2(pos.x + 34.0f, pos.y + 34.0f), theme::U32(preset.color), 8.0f);
            if (selected) {
                dl->AddRect(ImVec2(pos.x - 3.0f, pos.y - 3.0f), ImVec2(pos.x + 37.0f, pos.y + 37.0f),
                            theme::U32(theme::kText), 10.0f, 0, 2.0f);
            }
            ImGui::PopID();
            ImGui::SameLine(0.0f, 10.0f);
        }
        float rgb[3] = {state.ui.accent.x, state.ui.accent.y, state.ui.accent.z};
        if (ImGui::ColorEdit3("##customAccent", rgb,
                              ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel |
                                  ImGuiColorEditFlags_PickerHueWheel)) {
            SetAccent(state, ImVec4(rgb[0], rgb[1], rgb[2], 1.0f));
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Cor personalizada");
        }

        widgets::SettingLabel("Opacidade da janela", "Transparência da janela principal.");
        float pct = state.ui.windowOpacity * 100.0f;
        ImGui::SetNextItemWidth(300.0f);
        if (ImGui::SliderFloat("##winOpacity", &pct, 60.0f, 100.0f, "%.0f%%", ImGuiSliderFlags_AlwaysClamp)) {
            state.ui.windowOpacity = pct / 100.0f;
        }

        widgets::SettingLabel("Escala da interface",
                              "Multiplica a escala do Windows (100%, 125%, 150%...). Aplica ao soltar o controle.");
        // Aplicar só ao soltar: mudar a escala no meio do arraste moveria o próprio controle.
        float scalePct = (state.ui.scaleDraft > 0.0f ? state.ui.scaleDraft : state.ui.scale) * 100.0f;
        ImGui::SetNextItemWidth(300.0f);
        if (ImGui::SliderFloat("##uiScale", &scalePct, 80.0f, 150.0f, "%.0f%%", ImGuiSliderFlags_AlwaysClamp)) {
            state.ui.scaleDraft = std::round(scalePct / 5.0f) * 5.0f / 100.0f;
        }
        if (ImGui::IsItemDeactivatedAfterEdit() && state.ui.scaleDraft > 0.0f) {
            state.ui.scale = state.ui.scaleDraft;
            state.ui.scaleDraft = -1.0f;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("100%##scaleReset")) {
            state.ui.scale = 1.0f;
            state.ui.scaleDraft = -1.0f;
        }
        widgets::EndSettings();
    }
    widgets::EndCard();
    InterfacePreview(state);
}

}  // namespace

void Ajustes(AppState& state) {
    if (!ImGui::BeginTabBar("##settingsTabs")) {
        return;
    }
    const char* names[] = {"Padrões de recuo", "Treino", "Repetição", "Overlay", "Interface"};
    for (int i = 0; i < 5; ++i) {
        ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;
        if (state.settingsTabRequest == i) {
            flags |= ImGuiTabItemFlags_SetSelected;
        }
        if (ImGui::BeginTabItem(names[i], nullptr, flags)) {
            ImGui::Spacing();
            switch (i) {
                case kTabPatterns:
                    TabPatterns(state);
                    break;
                case kTabTraining:
                    TabTraining(state);
                    break;
                case kTabRepetition:
                    TabRepetition(state);
                    break;
                case kTabOverlay:
                    TabOverlay(state);
                    break;
                default:
                    TabInterface(state);
                    break;
            }
            ImGui::EndTabItem();
        }
    }
    state.settingsTabRequest = -1;
    ImGui::EndTabBar();
}

}  // namespace st::pages
