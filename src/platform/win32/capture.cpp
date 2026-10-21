#include "platform/win32/capture.h"

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

namespace st {

namespace {

// UTF-8 -> UTF-16 (padrão do projeto: perfis e títulos são UTF-8 em memória).
std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                      nullptr, 0);
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), n);
    return out;
}

// Converte um retângulo de área cliente para coordenadas de tela.
bool ClientToScreenRect(HWND hwnd, RECT& rc) {
    POINT tl{rc.left, rc.top};
    POINT br{rc.right, rc.bottom};
    if (!ClientToScreen(hwnd, &tl) || !ClientToScreen(hwnd, &br)) {
        return false;
    }
    rc.left = tl.x;
    rc.top = tl.y;
    rc.right = br.x;
    rc.bottom = br.y;
    return true;
}

// Copia os pixels de `src` (DC compatível, origem em 0,0) para uma ImageRgb.
bool ReadDcPixels(HDC src, int w, int h, ImageRgb& out) {
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;  // top-down: linha 0 = topo, como ImageRgb espera
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    std::vector<std::uint32_t> px(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
    const int got = GetDIBits(src, /*hbm=*/0, 0, static_cast<UINT>(h), px.data(), &bi, DIB_RGB_COLORS);
    if (got != h) {
        return false;
    }
    out.width = w;
    out.height = h;
    out.rgb.assign(px.size() * 3u, 0u);
    for (std::size_t i = 0; i < px.size(); ++i) {
        const std::uint32_t v = px[i];
        out.rgb[i * 3u + 0u] = static_cast<std::uint8_t>((v >> 16) & 0xFFu);  // R
        out.rgb[i * 3u + 1u] = static_cast<std::uint8_t>((v >> 8) & 0xFFu);   // G
        out.rgb[i * 3u + 2u] = static_cast<std::uint8_t>(v & 0xFFu);          // B
    }
    return true;
}

}  // namespace

AutoVisionController::CaptureResult CaptureScreenRegion(const ProfileData& profile) {
    AutoVisionController::CaptureResult res;
    const int w = profile.autoRegionW;
    const int h = profile.autoRegionH;
    if (w <= 0 || h <= 0 || w > kMaxAutoRegionSize || h > kMaxAutoRegionSize) {
        res.error = "regiao invalida";
        return res;
    }

    int x = profile.autoRegionX;
    int y = profile.autoRegionY;

    // Janela associada: reancora a regiao em cada captura. Se ela sumiu ou foi
    // minimizada, falha explicitamente (nunca usa coordenadas mortas em silencio).
    if (!profile.autoWindowTitle.empty()) {
        HWND hwnd = FindWindowW(nullptr, Utf8ToWide(profile.autoWindowTitle).c_str());
        if (hwnd == nullptr) {
            res.error = "janela nao encontrada: \"" + profile.autoWindowTitle + "\"";
            return res;
        }
        if (IsIconic(hwnd) != FALSE || !IsWindowVisible(hwnd)) {
            res.error = "janela minimizada ou oculta";
            return res;
        }
        RECT rc{};
        if (!GetClientRect(hwnd, &rc) || !ClientToScreenRect(hwnd, rc)) {
            res.error = "falha ao ler a janela monitorada";
            return res;
        }
        // A regiao cadastrada e relativa ao canto da area cliente da janela:
        // mover a janela acompanha a regiao automaticamente.
        x = rc.left + profile.autoRegionX;
        y = rc.top + profile.autoRegionY;
    }

    HDC screen = GetDC(nullptr);
    if (screen == nullptr) {
        res.error = "sem acesso a tela";
        return res;
    }
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP bmp = (mem != nullptr) ? CreateCompatibleBitmap(screen, w, h) : nullptr;
    HGDIOBJ oldBmp = nullptr;
    bool copied = false;
    if (mem != nullptr && bmp != nullptr) {
        oldBmp = SelectObject(mem, bmp);
        copied = BitBlt(mem, 0, 0, w, h, screen, x, y, SRCCOPY | CAPTUREBLT) != FALSE;
    }
    if (copied) {
        copied = ReadDcPixels(mem, w, h, res.image);
    }
    if (oldBmp != nullptr) {
        SelectObject(mem, oldBmp);
    }
    if (bmp != nullptr) {
        DeleteObject(bmp);
    }
    if (mem != nullptr) {
        DeleteDC(mem);
    }
    ReleaseDC(nullptr, screen);

    if (!copied || res.image.Empty()) {
        res.error = "falha na captura (BitBlt)";
        res.image = ImageRgb{};
        return res;
    }
    res.ok = true;
    return res;
}

AutoVisionController::CaptureResult CaptureAppStateRegion(const AppState& state) {
    // Mesma moeda do restante do pipeline: ProfileData derivado do estado da UI.
    ProfileData p;
    p.autoVisionEnabled = state.autoVision.enabled;
    p.autoRegionX = state.autoVision.regionX;
    p.autoRegionY = state.autoVision.regionY;
    p.autoRegionW = state.autoVision.regionW;
    p.autoRegionH = state.autoVision.regionH;
    p.autoWindowTitle = state.autoVision.windowTitle;
    p.autoConfidenceMin = state.autoVision.confidenceMin;
    p.autoIntervalMs = state.autoVision.intervalMs;
    p.autoConfirmFrames = state.autoVision.confirmFrames;
    return CaptureScreenRegion(p);
}

namespace {

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    auto* out = reinterpret_cast<std::vector<std::string>*>(lParam);
    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }
    const int len = GetWindowTextLengthW(hwnd);
    if (len <= 0) {
        return TRUE;
    }
    std::wstring title(static_cast<std::size_t>(len), L'\0');
    if (GetWindowTextW(hwnd, title.data(), len + 1) <= 0) {
        return TRUE;
    }
    const int need = WideCharToMultiByte(CP_UTF8, 0, title.data(), len, nullptr, 0, nullptr, nullptr);
    if (need <= 0) {
        return TRUE;
    }
    std::string utf8(static_cast<std::size_t>(need), '\0');
    WideCharToMultiByte(CP_UTF8, 0, title.data(), len, utf8.data(), need, nullptr, nullptr);
    out->push_back(std::move(utf8));
    return TRUE;
}

}  // namespace

std::vector<std::string> ListTopLevelWindowTitles(std::size_t max) {
    std::vector<std::string> titles;
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&titles));
    if (titles.size() > max) {
        titles.resize(max);
    }
    return titles;
}

}  // namespace st
