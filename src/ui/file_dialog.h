#pragma once

#include <windows.h>

#include <filesystem>
#include <optional>
#include <string>

namespace st {

// Diálogos nativos de abrir/salvar arquivo .json. Devolvem nada se o usuário cancelar.
std::optional<std::filesystem::path> OpenJsonDialog(HWND owner);
std::optional<std::filesystem::path> SaveJsonDialog(HWND owner, const std::wstring& suggestedName);

}  // namespace st
