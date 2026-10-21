#include "config/paths_win.h"

// windows.h primeiro: knownfolders.h/shlobj.h dependem das macros dele.
#include <windows.h>

#include <knownfolders.h>
#include <shlobj.h>

#include <string>

namespace st {

namespace {

std::filesystem::path ExeDir() {
    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
        const DWORD len = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (len == 0) {
            return {};
        }
        if (len < buf.size()) {
            buf.resize(len);
            return std::filesystem::path(buf).parent_path();
        }
        buf.resize(buf.size() * 2);
    }
}

}  // namespace

std::filesystem::path DataDir() {
    const std::filesystem::path exeDir = ExeDir();
    std::error_code ec;
    if (!exeDir.empty() && std::filesystem::exists(exeDir / L"portable.txt", ec)) {
        return exeDir / L"data";
    }

    PWSTR appData = nullptr;
    std::filesystem::path dir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData))) {
        dir = std::filesystem::path(appData) / L"SprayTrainer";
    }
    CoTaskMemFree(appData);
    if (dir.empty()) {
        dir = exeDir / L"data";  // sem %APPDATA% (raro): cai para a pasta do exe
    }
    return dir;
}

}  // namespace st
