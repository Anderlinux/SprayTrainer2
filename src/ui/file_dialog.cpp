#include "ui/file_dialog.h"

#include <commdlg.h>

#include <algorithm>
#include <vector>

namespace st {

namespace {

constexpr wchar_t kFilter[] = L"Perfil do Spray Trainer (*.json)\0*.json\0Todos os arquivos (*.*)\0*.*\0";
constexpr wchar_t kPngFilter[] = L"Imagem PNG (*.png)\0*.png\0Todos os arquivos (*.*)\0*.*\0";
constexpr DWORD kBufferChars = 4096;

}  // namespace

std::optional<std::filesystem::path> OpenJsonDialog(HWND owner) {
    std::vector<wchar_t> buffer(kBufferChars, L'\0');
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = kFilter;
    ofn.lpstrFile = buffer.data();
    ofn.nMaxFile = kBufferChars;
    ofn.lpstrTitle = L"Importar perfil";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) {
        return std::nullopt;
    }
    return std::filesystem::path(buffer.data());
}

std::optional<std::filesystem::path> OpenAnyFileDialog(HWND owner) {
    std::vector<wchar_t> buffer(kBufferChars, L'\0');
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = kPngFilter;
    ofn.lpstrFile = buffer.data();
    ofn.nMaxFile = kBufferChars;
    ofn.lpstrTitle = L"Importar imagem de referencia";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) {
        return std::nullopt;
    }
    return std::filesystem::path(buffer.data());
}

std::optional<std::filesystem::path> SaveJsonDialog(HWND owner, const std::wstring& suggestedName) {
    std::vector<wchar_t> buffer(kBufferChars, L'\0');
    suggestedName.copy(buffer.data(), std::min<std::size_t>(suggestedName.size(), kBufferChars - 1));
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = kFilter;
    ofn.lpstrFile = buffer.data();
    ofn.nMaxFile = kBufferChars;
    ofn.lpstrDefExt = L"json";
    ofn.lpstrTitle = L"Exportar perfil";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameW(&ofn)) {
        return std::nullopt;
    }
    return std::filesystem::path(buffer.data());
}

}  // namespace st
