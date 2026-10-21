#pragma once

#include "ui/app_state.h"

#include <filesystem>
#include <string>

namespace st::profiles {

ProfileData Capture(const AppState& state);
// Aplica um perfil ao estado (padrões, configurações, cor). Não aplica durante uma rajada.
bool Apply(AppState& state, const ProfileData& data);

// true se algo mudou desde o último salvar/carregar.
bool Dirty(const AppState& state);

// Ao abrir o app: carrega o último perfil usado (ou padrões, com aviso se falhar).
void Startup(AppState& state);

// Histórico de rajadas (history.json na pasta de dados).
void LoadHistory(AppState& state);
void RecordBurst(AppState& state, const SessionResult& result);
void ClearHistory(AppState& state);

// Salva no perfil atual; sem perfil atual, abre o diálogo "Salvar como".
void SaveCurrent(AppState& state);
bool SaveAs(AppState& state, const std::string& name);
bool Load(AppState& state, const std::string& name);
bool CreateFromDefaults(AppState& state, const std::string& name);
bool Rename(AppState& state, const std::string& from, const std::string& to);
bool Duplicate(AppState& state, const std::string& from, const std::string& to);
bool Delete(AppState& state, const std::string& name);
void ImportFrom(AppState& state, const std::filesystem::path& file);
void ExportTo(AppState& state, const std::filesystem::path& file);

const std::vector<std::string>& List(AppState& state);  // com cache curto
void InvalidateList(AppState& state);

void Notify(AppState& state, const std::string& text, bool error = false);

// Imagens de referência do reconhecimento (arquivo irmão <nome>.images.json).
void SaveImages(AppState& state, const std::string& name);
void LoadImages(AppState& state, const std::string& name);

// Diálogos de nome, excluir e sair sem salvar (desenhados na janela principal).
void RenderModals(AppState& state);
void OpenPrompt(AppState& state, NamePrompt prompt, const std::string& target = {});

}  // namespace st::profiles
