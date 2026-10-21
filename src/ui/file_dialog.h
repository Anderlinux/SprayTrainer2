#pragma once

#include <windows.h>

#include <filesystem>
#include <optional>
#include <string>

namespace st {

// Diálogos nativos de abrir/salvar arquivo .json. Devolvem nada se o usuário cancelar.
std::optional<std::filesystem::path> OpenJsonDialog(HWND owner);
std::optional<std::filesystem::path> SaveJsonDialog(HWND owner, const std::wstring& suggestedName);

// Abre qualquer arquivo (usado pela aba Auto-Reconhecimento para importar PNG).
// Devolve nada se o usuário cancelar.
std::optional<std::filesystem::path> OpenAnyFileDialog(HWND owner);

}  // namespace st
