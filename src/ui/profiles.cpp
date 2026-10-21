#include "ui/profiles.h"

#include "config/profile_store.h"
#include "core/repetition.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#include <imgui.h>

#include <algorithm>
#include <ctime>
#include <cfloat>
#include <cstdio>
#include <cstring>

namespace st::profiles {

namespace {

constexpr double kBannerMs = 4000.0;
constexpr double kListCacheMs = 2000.0;

bool Busy(AppState& state) {
    if (state.session.Running()) {
        Notify(state, "Termine a rajada antes de trocar de perfil.", true);
        return true;
    }
    return false;
}

void MarkSaved(AppState& state, const std::string& name, const ProfileData& data) {
    state.profiles.current = name;
    state.profiles.savedSnapshot = data;
    for (std::size_t i = 0; i < kWeaponCount; ++i) {
        state.savedPatterns[i] = data.patterns[i];
    }
    state.profiles.store->SetLastProfile(name);
    InvalidateList(state);
}

std::string JoinWarnings(const std::vector<std::string>& warnings) {
    std::string text;
    for (std::size_t i = 0; i < warnings.size() && i < 3; ++i) {
        text += (i == 0 ? "" : "; ") + warnings[i];
    }
    if (warnings.size() > 3) {
        text += " (+" + std::to_string(warnings.size() - 3) + ")";
    }
    return text;
}

}  // namespace

ProfileData Capture(const AppState& s) {
    ProfileData d;
    d.weapon = s.weapon;
    d.sensitivity = s.sensitivity;
    d.fireButton = static_cast<int>(s.fireButton);
    d.scoreGoal = s.scoreGoal;
    d.scoreTolerance = s.scoreTolerance;
    for (std::size_t i = 0; i < kWeaponCount; ++i) {
        d.patterns[i] = s.patterns.Get(static_cast<Weapon>(i));
    }
    d.overlayVisible = s.overlay.visible;
    d.overlayCorner = static_cast<int>(s.overlay.corner);
    d.overlayScale = s.overlay.scale;
    d.overlayOpacity = s.overlay.opacity;
    d.overlayRefreshMs = s.overlay.refreshMs;
    d.accent = {s.ui.accent.x, s.ui.accent.y, s.ui.accent.z};
    d.windowOpacity = s.ui.windowOpacity;
    d.uiScale = s.ui.scale;
    d.visualGuide = static_cast<int>(s.visual.guide);
    d.visualGuideAhead = s.visual.guideAhead;
    d.visualPlayerColor = s.visual.playerColor;
    d.visualLineWidth = s.visual.lineWidth;
    d.visualGrid = s.visual.grid;
    d.visualShotNumbers = s.visual.shotNumbers;
    d.repetitionEnabled = s.repetition.enabled;
    d.repeatKeyVk = s.repetition.repeatKeyVk;
    d.repeatToggle = s.repetition.activation == RepeatActivation::Toggle;
    d.stopKeysVk = s.repetition.stopKeysVk;
    d.repeatSubsteps = s.repetition.substeps;
    d.repeatAutoScore = s.repetition.autoScore;
    d.repeatReturnBack = s.repetition.returnBack;
    d.repeatRestartFromZero = s.repetition.restartFromZero;
    d.cycleWeaponKeyVk = s.repetition.cycleWeaponKeyVk;
    d.cycleBackKeyVk = s.repetition.cycleBackKeyVk;
    return d;
}

bool Apply(AppState& s, const ProfileData& d) {
    if (s.session.Running()) {
        return false;
    }
    s.weapon = d.weapon;
    s.editorWeapon = d.weapon;
    s.editorRow = -1;
    s.sensitivity = d.sensitivity;
    s.fireButton = d.fireButton == 1 ? FireButton::Right : FireButton::Left;
    s.scoreGoal = d.scoreGoal;
    s.scoreTolerance = d.scoreTolerance;
    for (std::size_t i = 0; i < kWeaponCount; ++i) {
        s.patterns.Set(static_cast<Weapon>(i), d.patterns[i]);
        s.savedPatterns[i] = d.patterns[i];
    }
    s.overlay.visible = d.overlayVisible;
    s.overlay.corner = static_cast<OverlayCorner>(std::clamp(d.overlayCorner, 0, 3));
    s.overlay.scale = d.overlayScale;
    s.overlay.opacity = d.overlayOpacity;
    s.overlay.refreshMs = d.overlayRefreshMs;
    s.ui.accent = ImVec4(d.accent[0], d.accent[1], d.accent[2], 1.0f);
    s.ui.windowOpacity = d.windowOpacity;
    s.ui.scale = d.uiScale;
    s.visual.guide = static_cast<GuideMode>(std::clamp(d.visualGuide, 0, 2));
    s.visual.guideAhead = d.visualGuideAhead;
    s.visual.playerColor = std::clamp(d.visualPlayerColor, 0, theme::kPlayerColorCount - 1);
    s.visual.lineWidth = d.visualLineWidth;
    s.visual.grid = d.visualGrid;
    s.visual.shotNumbers = d.visualShotNumbers;
    s.repetition.enabled = d.repetitionEnabled;
    s.repetition.repeatKeyVk = d.repeatKeyVk;
    // Perfis antigos podem ter "toggle": false (modo pressionar); os novos vêm com
    // true. O default em memória é Toggle, então só forçamos Hold quando o JSON diz.
    s.repetition.activation = d.repeatToggle ? RepeatActivation::Toggle : RepeatActivation::Hold;
    s.repetition.stopKeysVk = d.stopKeysVk;
    s.repetition.substeps = std::clamp(d.repeatSubsteps, 1, PatternRepeater::kMaxSubsteps);
    s.repetition.autoScore = d.repeatAutoScore;
    s.repetition.returnBack = d.repeatReturnBack;
    s.repetition.restartFromZero = d.repeatRestartFromZero;
    s.repetition.cycleWeaponKeyVk = d.cycleWeaponKeyVk;
    s.repetition.cycleBackKeyVk = d.cycleBackKeyVk;
    theme::SetAccent(s.ui.accent);  // cada janela reaplica o estilo no próprio contexto
    s.lastResult.reset();
    return true;
}

bool Dirty(const AppState& state) {
    return !(Capture(state) == state.profiles.savedSnapshot);
}

void Notify(AppState& state, const std::string& text, bool error) {
    state.profiles.banner = text;
    state.profiles.bannerError = error;
    state.profiles.bannerUntilMs = error ? 1.0e300 : state.nowMs + kBannerMs;
}

void Startup(AppState& state) {
    ProfileUiState& p = state.profiles;
    p.savedSnapshot = Capture(state);  // estado de fábrica
    if (p.store == nullptr) {
        return;
    }
    if (StoreResult r = p.store->EnsureDirs(); !r.ok) {
        Notify(state, "Perfis indisponíveis: " + r.error, true);
        return;
    }
    LoadHistory(state);  // antes do perfil: um aviso de perfil (mais importante) fica por cima
    const std::string last = p.store->LastProfile();
    if (last.empty()) {
        return;
    }
    if (!p.store->Exists(last)) {
        Notify(state, "O último perfil (\"" + last + "\") não foi encontrado. Usando os padrões.", true);
        return;
    }
    ProfileData data;
    std::vector<std::string> warnings;
    if (StoreResult r = p.store->Load(last, data, &warnings); !r.ok) {
        // Não sobrescreve o arquivo com problema: o usuário decide (corrigir à mão, excluir...).
        Notify(state, "O perfil \"" + last + "\" está corrompido (" + r.error +
                          "). Usando os padrões; o arquivo não foi alterado.",
               true);
        return;
    }
    Apply(state, data);
    p.current = last;
    p.savedSnapshot = Capture(state);
    p.selected = last;
    if (!warnings.empty()) {
        Notify(state, "Perfil \"" + last + "\" carregado com ajustes: " + JoinWarnings(warnings), true);
    }
}

void LoadHistory(AppState& state) {
    std::vector<BurstRecord> entries;
    int skipped = 0;
    if (StoreResult r = state.profiles.store->LoadHistory(entries, &skipped); !r.ok) {
        Notify(state, "Histórico ilegível: " + r.error + ". Começando um novo.", true);
        return;
    }
    state.history.Set(std::move(entries));
    if (skipped > 0) {
        Notify(state, std::to_string(skipped) + " rajada(s) inválida(s) do histórico foram ignoradas.", true);
    }
}

void RecordBurst(AppState& state, const SessionResult& result) {
    state.history.Add(MakeBurstRecord(result, static_cast<std::int64_t>(std::time(nullptr)), state.scoreGoal));
    state.historySelected = -1;  // índices mudam quando a mais antiga sai
    if (state.profiles.store == nullptr) {
        return;
    }
    if (StoreResult r = state.profiles.store->SaveHistory(state.history.Entries()); !r.ok) {
        Notify(state, "Não foi possível salvar o histórico: " + r.error, true);
    }
}

void ClearHistory(AppState& state) {
    state.history.Clear();
    state.historySelected = -1;
    if (state.profiles.store != nullptr) {
        state.profiles.store->SaveHistory({});
    }
    Notify(state, "Histórico apagado.");
}

void SaveCurrent(AppState& state) {
    ProfileUiState& p = state.profiles;
    if (p.current.empty()) {
        OpenPrompt(state, NamePrompt::SaveAs);
        state.requestShowMain = true;  // o diálogo de nome fica na janela principal
        return;
    }
    SaveAs(state, p.current);
}

bool SaveAs(AppState& state, const std::string& name) {
    ProfileUiState& p = state.profiles;
    const ProfileData data = Capture(state);
    if (StoreResult r = p.store->Save(name, data); !r.ok) {
        Notify(state, "Não foi possível salvar: " + r.error, true);
        return false;
    }
    MarkSaved(state, name, data);
    p.selected = name;
    Notify(state, "Perfil \"" + name + "\" salvo.");
    return true;
}

bool Load(AppState& state, const std::string& name) {
    if (Busy(state)) {
        return false;
    }
    ProfileData data;
    std::vector<std::string> warnings;
    if (StoreResult r = state.profiles.store->Load(name, data, &warnings); !r.ok) {
        Notify(state, "Não foi possível carregar \"" + name + "\": " + r.error, true);
        return false;
    }
    Apply(state, data);
    MarkSaved(state, name, Capture(state));
    state.profiles.selected = name;
    if (warnings.empty()) {
        Notify(state, "Perfil \"" + name + "\" carregado.");
    } else {
        Notify(state, "Perfil \"" + name + "\" carregado com ajustes: " + JoinWarnings(warnings), true);
    }
    return true;
}

bool CreateFromDefaults(AppState& state, const std::string& name) {
    if (Busy(state)) {
        return false;
    }
    const ProfileData data = DefaultProfile();
    if (StoreResult r = state.profiles.store->Save(name, data); !r.ok) {
        Notify(state, "Não foi possível criar: " + r.error, true);
        return false;
    }
    Apply(state, data);
    MarkSaved(state, name, Capture(state));
    state.profiles.selected = name;
    Notify(state, "Perfil \"" + name + "\" criado com os padrões de fábrica.");
    return true;
}

bool Rename(AppState& state, const std::string& from, const std::string& to) {
    ProfileUiState& p = state.profiles;
    if (StoreResult r = p.store->Rename(from, to); !r.ok) {
        p.promptError = r.error;
        return false;
    }
    if (p.current == from) {
        p.current = to;
    }
    p.selected = to;
    InvalidateList(state);
    Notify(state, "Perfil renomeado para \"" + to + "\".");
    return true;
}

bool Duplicate(AppState& state, const std::string& from, const std::string& to) {
    ProfileUiState& p = state.profiles;
    if (StoreResult r = p.store->Duplicate(from, to); !r.ok) {
        p.promptError = r.error;
        return false;
    }
    p.selected = to;
    InvalidateList(state);
    Notify(state, "Cópia \"" + to + "\" criada.");
    return true;
}

bool Delete(AppState& state, const std::string& name) {
    ProfileUiState& p = state.profiles;
    if (StoreResult r = p.store->Delete(name); !r.ok) {
        Notify(state, r.error, true);
        return false;
    }
    if (p.current == name) {
        // As configurações continuam carregadas; só não estão mais ligadas a um arquivo.
        p.current.clear();
        p.savedSnapshot = DefaultProfile();
    }
    p.selected.clear();
    InvalidateList(state);
    Notify(state, "Perfil \"" + name + "\" excluído.");
    return true;
}

void ImportFrom(AppState& state, const std::filesystem::path& file) {
    if (Busy(state)) {
        return;
    }
    ProfileData data;
    std::vector<std::string> warnings;
    if (StoreResult r = ProfileStore::ReadProfileFile(file, data, &warnings); !r.ok) {
        Notify(state, "Importação recusada: " + r.error, true);
        return;
    }
    ProfileStore& store = *state.profiles.store;
    const std::string name =
        store.UniqueName(ProfileStore::SanitizeName(ProfileStore::PathToUtf8(file.stem())));
    if (StoreResult r = store.Save(name, data); !r.ok) {
        Notify(state, "Não foi possível importar: " + r.error, true);
        return;
    }
    Apply(state, data);
    MarkSaved(state, name, Capture(state));
    state.profiles.selected = name;
    if (warnings.empty()) {
        Notify(state, "Perfil importado como \"" + name + "\".");
    } else {
        Notify(state, "Importado como \"" + name + "\" com ajustes: " + JoinWarnings(warnings), true);
    }
}

void ExportTo(AppState& state, const std::filesystem::path& file) {
    if (StoreResult r = ProfileStore::WriteProfileFile(file, Capture(state)); !r.ok) {
        Notify(state, "Não foi possível exportar: " + r.error, true);
        return;
    }
    Notify(state, "Perfil exportado para " + ProfileStore::PathToUtf8(file) + ".");
}

const std::vector<std::string>& List(AppState& state) {
    ProfileUiState& p = state.profiles;
    if (p.store != nullptr && state.nowMs - p.listRefreshedMs >= kListCacheMs) {
        p.list = p.store->List();
        p.listRefreshedMs = state.nowMs;
    }
    return p.list;
}

void InvalidateList(AppState& state) {
    state.profiles.listRefreshedMs = -1.0e9;
}

void OpenPrompt(AppState& state, NamePrompt prompt, const std::string& target) {
    ProfileUiState& p = state.profiles;
    p.prompt = prompt;
    p.promptTarget = target;
    p.promptError.clear();
    std::string initial;
    switch (prompt) {
        case NamePrompt::SaveAs:
            initial = p.current.empty() ? "Meu perfil" : p.current + " (cópia)";
            break;
        case NamePrompt::New:
            initial = "Novo perfil";
            break;
        case NamePrompt::Rename:
            initial = target;
            break;
        case NamePrompt::Duplicate:
            initial = target + " (cópia)";
            break;
        case NamePrompt::None:
            break;
    }
    if (p.store != nullptr && prompt != NamePrompt::Rename) {
        initial = p.store->UniqueName(initial);
    }
    std::snprintf(p.promptBuffer, sizeof(p.promptBuffer), "%s", initial.c_str());
}

namespace {

const char* PromptTitle(NamePrompt prompt) {
    switch (prompt) {
        case NamePrompt::SaveAs:
            return "Salvar perfil como";
        case NamePrompt::New:
            return "Novo perfil (padrões de fábrica)";
        case NamePrompt::Rename:
            return "Renomear perfil";
        case NamePrompt::Duplicate:
            return "Duplicar perfil";
        case NamePrompt::None:
            break;
    }
    return "";
}

void BeginModalStyle() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 20.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.0f, 0.0f, 0.0f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg, theme::kPanelAlt);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, theme::kPanelAlt);
}

void EndModalStyle() {
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
}

void CenterNextModal() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
}

void NameModal(AppState& state) {
    ProfileUiState& p = state.profiles;
    const char* title = PromptTitle(p.prompt);
    if (p.prompt != NamePrompt::None && !ImGui::IsPopupOpen(title)) {
        ImGui::OpenPopup(title);
    }
    CenterNextModal();
    ImGui::SetNextWindowSize(ImVec2(440.0f, 0.0f));
    if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        return;
    }
    ImGui::TextColored(theme::kTextDim, "Nome do perfil (até 40 caracteres):");
    if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere();
    }
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool enter = ImGui::InputText("##name", p.promptBuffer, sizeof(p.promptBuffer),
                                        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
    std::string name = p.promptBuffer;
    const auto invalid = ProfileStore::ValidateName(name);
    const std::string& shownError = !p.promptError.empty() ? p.promptError : (invalid ? *invalid : std::string());
    if (!shownError.empty()) {
        ImGui::TextColored(theme::kPink, "%s", shownError.c_str());
    } else if (p.store->Exists(name) && p.prompt == NamePrompt::SaveAs) {
        ImGui::TextColored(theme::kTextDim, "Já existe: será substituído.");
    } else {
        ImGui::TextUnformatted(" ");
    }
    ImGui::Spacing();

    bool confirm = enter;
    ImGui::BeginDisabled(invalid.has_value());
    confirm |= widgets::OutlineButton("Confirmar##prompt", nullptr, ImVec2(180.0f, 38.0f));
    ImGui::EndDisabled();
    ImGui::SameLine(0.0f, 10.0f);
    const bool cancel = widgets::OutlineButton("Cancelar##prompt", nullptr, ImVec2(180.0f, 38.0f)) ||
                        ImGui::IsKeyPressed(ImGuiKey_Escape);

    if (confirm && !invalid) {
        p.promptError.clear();
        bool ok = false;
        switch (p.prompt) {
            case NamePrompt::SaveAs:
                ok = SaveAs(state, name);
                break;
            case NamePrompt::New:
                if (p.store->Exists(name)) {
                    p.promptError = "já existe um perfil com esse nome";
                } else {
                    ok = CreateFromDefaults(state, name);
                }
                break;
            case NamePrompt::Rename:
                ok = Rename(state, p.promptTarget, name);
                break;
            case NamePrompt::Duplicate:
                ok = Duplicate(state, p.promptTarget, name);
                break;
            case NamePrompt::None:
                break;
        }
        if (ok) {
            p.prompt = NamePrompt::None;
            ImGui::CloseCurrentPopup();
            if (p.exitAfterSave) {
                state.allowClose = true;
                state.requestClose = true;
            }
        }
    } else if (cancel) {
        p.prompt = NamePrompt::None;
        p.exitAfterSave = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void DeleteModal(AppState& state) {
    ProfileUiState& p = state.profiles;
    const char* title = "Excluir perfil";
    if (p.confirmDelete && !ImGui::IsPopupOpen(title)) {
        ImGui::OpenPopup(title);
    }
    CenterNextModal();
    ImGui::SetNextWindowSize(ImVec2(440.0f, 0.0f));
    if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        return;
    }
    ImGui::TextWrapped("Excluir o perfil \"%s\"? O arquivo será apagado e isso não pode ser desfeito.",
                       p.selected.c_str());
    ImGui::Spacing();
    const bool yes = widgets::OutlineButton("Excluir##del", nullptr, ImVec2(180.0f, 38.0f));
    ImGui::SameLine(0.0f, 10.0f);
    const bool no =
        widgets::OutlineButton("Cancelar##del", nullptr, ImVec2(180.0f, 38.0f)) || ImGui::IsKeyPressed(ImGuiKey_Escape);
    if (yes) {
        Delete(state, p.selected);
    }
    if (yes || no) {
        p.confirmDelete = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void CloseModal(AppState& state) {
    const char* title = "Alterações não salvas";
    if (state.confirmClose && !ImGui::IsPopupOpen(title)) {
        ImGui::OpenPopup(title);
    }
    CenterNextModal();
    ImGui::SetNextWindowSize(ImVec2(520.0f, 0.0f));
    if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        return;
    }
    const std::string& current = state.profiles.current;
    if (current.empty()) {
        ImGui::TextWrapped("Suas configurações ainda não foram salvas em um perfil. Salvar antes de sair?");
    } else {
        ImGui::TextWrapped("O perfil \"%s\" tem alterações não salvas. Salvar antes de sair?", current.c_str());
    }
    ImGui::Spacing();
    const ImVec2 size(150.0f, 38.0f);
    const bool save = widgets::OutlineButton("Salvar e sair##close", nullptr, size);
    ImGui::SameLine(0.0f, 10.0f);
    const bool discard = widgets::OutlineButton("Sair sem salvar##close", nullptr, size);
    ImGui::SameLine(0.0f, 10.0f);
    const bool cancel =
        widgets::OutlineButton("Cancelar##close", nullptr, size) || ImGui::IsKeyPressed(ImGuiKey_Escape);

    if (save || discard || cancel) {
        state.confirmClose = false;
        ImGui::CloseCurrentPopup();
    }
    if (save) {
        if (current.empty()) {
            state.profiles.exitAfterSave = true;
            OpenPrompt(state, NamePrompt::SaveAs);
        } else if (SaveAs(state, current)) {
            state.allowClose = true;
            state.requestClose = true;
        }
    } else if (discard) {
        state.allowClose = true;
        state.requestClose = true;
    }
    ImGui::EndPopup();
}

}  // namespace

void RenderModals(AppState& state) {
    if (state.profiles.store == nullptr) {
        return;
    }
    BeginModalStyle();
    NameModal(state);
    DeleteModal(state);
    CloseModal(state);
    EndModalStyle();
}

}  // namespace st::profiles
