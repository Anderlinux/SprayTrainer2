#include "overlay/overlay.h"

#include "overlay/overlay_ui.h"
#include "ui/theme.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <algorithm>
#include <cmath>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace st {

namespace {

constexpr wchar_t kClassName[] = L"SprayTrainerOverlay";
constexpr int kMargin = 16;
constexpr double kExpandedRefreshMs = 16.0;
constexpr double kTopmostRefreshMs = 1000.0;

// Troca o contexto ImGui atual e restaura ao sair do escopo.
class ContextScope {
public:
    explicit ContextScope(ImGuiContext* ctx) : prev_(ImGui::GetCurrentContext()) { ImGui::SetCurrentContext(ctx); }
    ~ContextScope() { ImGui::SetCurrentContext(prev_); }
    ContextScope(const ContextScope&) = delete;
    ContextScope& operator=(const ContextScope&) = delete;

private:
    ImGuiContext* prev_;
};

bool SameRect(const RECT& a, const RECT& b) {
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

}  // namespace

bool Overlay::Init(HINSTANCE instance, ID3D11Device* device, ID3D11DeviceContext* context, IDXGIFactory* factory,
                   AppState* state) {
    instance_ = instance;
    device_ = device;
    context_ = context;
    state_ = state;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &Overlay::WndProcThunk;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassName;
    if (RegisterClassExW(&wc) == 0) {
        return false;
    }

    const DWORD exStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED | WS_EX_TRANSPARENT;
    hwnd_ = CreateWindowExW(exStyle, kClassName, L"Spray Trainer Overlay", WS_POPUP, 0, 0,
                            static_cast<int>(kOverlayCompactSize.x), static_cast<int>(kOverlayCompactSize.y), nullptr,
                            nullptr, instance, this);
    if (hwnd_ == nullptr) {
        return false;
    }
    clickThrough_ = true;
    SetLayeredWindowAttributes(hwnd_, 0, 255, LWA_ALPHA);

    if (!target_.Create(device_, factory, hwnd_)) {
        return false;
    }

    ImGuiContext* prev = ImGui::GetCurrentContext();
    imgui_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui_);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    theme::Apply();
    theme::LoadFonts();
    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX11_Init(device_, context_);
    ImGui::SetCurrentContext(prev);
    return true;
}

void Overlay::Shutdown() {
    if (imgui_ != nullptr) {
        ImGuiContext* prev = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imgui_);
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(imgui_);
        imgui_ = nullptr;
        ImGui::SetCurrentContext(prev);
    }
    target_.Release();
    if (hwnd_ != nullptr) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (instance_ != nullptr) {
        UnregisterClassW(kClassName, instance_);
        instance_ = nullptr;
    }
}

void Overlay::Update(double nowMs) {
    if (hwnd_ == nullptr) {
        return;
    }
    if (!state_->overlay.visible) {
        if (shown_) {
            ShowWindow(hwnd_, SW_HIDE);
            shown_ = false;
        }
        return;
    }

    ApplyLayout();
    UpdateClickThrough();

    // Jogos em tela cheia "sem borda" podem passar por cima; reafirma o topo de vez em quando.
    if (nowMs - lastTopmostMs_ >= kTopmostRefreshMs) {
        SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        lastTopmostMs_ = nowMs;
    }

    const double interval =
        state_->overlayExpanded ? kExpandedRefreshMs : static_cast<double>(std::max(state_->overlay.refreshMs, 1));
    if (forceRender_ || nowMs - lastRenderMs_ >= interval) {
        Render();
        lastRenderMs_ = nowMs;
        forceRender_ = false;
    }
}

void Overlay::ApplyLayout() {
    const OverlaySettings& cfg = state_->overlay;
    const float opacity = std::clamp(cfg.opacity, 0.3f, 1.0f);
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);

    // A escala pedida vale para os dois modos, mas o painel expandido nunca passa da área útil.
    const ImVec2 base = state_->overlayExpanded ? kOverlayExpandedSize : kOverlayCompactSize;
    const float fitW = static_cast<float>(work.right - work.left - 2 * kMargin) / base.x;
    const float fitH = static_cast<float>(work.bottom - work.top - 2 * kMargin) / base.y;
    // Escala do overlay × escala do monitor principal (onde fica a área útil usada).
    const POINT origin{0, 0};
    const float dpi = ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY));
    float scale = std::clamp(cfg.scale, 0.75f, 1.5f) * dpi;
    if (fitW > 0.0f && fitH > 0.0f) {
        scale = std::min(scale, std::max(std::min(fitW, fitH), 0.5f));
    }

    if (opacity != appliedOpacity_) {
        SetLayeredWindowAttributes(hwnd_, 0, static_cast<BYTE>(std::lround(opacity * 255.0f)), LWA_ALPHA);
        appliedOpacity_ = opacity;
    }

    const ImVec4 accent = theme::Accent();
    const bool accentChanged = accent.x != appliedAccent_.x || accent.y != appliedAccent_.y || accent.z != appliedAccent_.z;
    if (accentChanged && appliedScale_ > 0.0f) {
        ContextScope scope(imgui_);
        theme::Apply();
        ImGui::GetStyle().ScaleAllSizes(appliedScale_);
        appliedAccent_ = accent;
        forceRender_ = true;
    }

    const bool expanded = state_->overlayExpanded;
    if (shown_ && expanded == appliedExpanded_ && scale == appliedScale_ && cfg.corner == appliedCorner_ &&
        SameRect(work, appliedWorkArea_)) {
        return;
    }

    const int w = static_cast<int>(std::lround(base.x * scale));
    const int h = static_cast<int>(std::lround(base.y * scale));
    const bool left = cfg.corner == OverlayCorner::TopLeft || cfg.corner == OverlayCorner::BottomLeft;
    const bool top = cfg.corner == OverlayCorner::TopLeft || cfg.corner == OverlayCorner::TopRight;
    const int x = left ? work.left + kMargin : work.right - w - kMargin;
    const int y = top ? work.top + kMargin : work.bottom - h - kMargin;

    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    const int corner = static_cast<int>(std::lround(28.0f * scale));
    SetWindowRgn(hwnd_, CreateRoundRectRgn(0, 0, w + 1, h + 1, corner, corner), TRUE);  // a janela fica dona da região
    pendingWidth_ = static_cast<UINT>(w);
    pendingHeight_ = static_cast<UINT>(h);

    if (scale != appliedScale_) {
        // Fontes rasterizadas já no tamanho final (nítidas); medidas do estilo escaladas junto.
        ContextScope scope(imgui_);
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->Clear();
        theme::LoadFonts(scale);
        io.FontGlobalScale = 1.0f;
        ImGui_ImplDX11_InvalidateDeviceObjects();
        theme::Apply();
        ImGui::GetStyle().ScaleAllSizes(scale);
        appliedAccent_ = accent;
    }

    shown_ = true;
    appliedExpanded_ = expanded;
    appliedScale_ = scale;
    appliedCorner_ = cfg.corner;
    appliedWorkArea_ = work;
    forceRender_ = true;
}

void Overlay::UpdateClickThrough() {
    if (state_->overlayExpanded) {
        SetClickThrough(false);
        return;
    }
    POINT p{};
    GetCursorPos(&p);
    ScreenToClient(hwnd_, &p);
    SetClickThrough(PtInRect(&hotRect_, p) == FALSE);
}

void Overlay::SetClickThrough(bool enabled) {
    if (enabled == clickThrough_) {
        return;
    }
    LONG_PTR ex = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
    ex = enabled ? (ex | WS_EX_TRANSPARENT) : (ex & ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT));
    SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, ex);
    clickThrough_ = enabled;
    forceRender_ = true;  // atualiza o hover do botão sem esperar o intervalo
}

void Overlay::Render() {
    ContextScope scope(imgui_);
    if (pendingWidth_ != 0 && pendingHeight_ != 0) {
        target_.Resize(device_, pendingWidth_, pendingHeight_);
        pendingWidth_ = pendingHeight_ = 0;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    const OverlayUiResult ui = RenderOverlayUi(*state_, appliedScale_);
    ImGui::Render();

    const float clear[4] = {theme::kPanel.x, theme::kPanel.y, theme::kPanel.z, 1.0f};
    target_.Bind(context_, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    target_.Present(0, 0);

    hotRect_ = RECT{static_cast<LONG>(ui.hotMin.x), static_cast<LONG>(ui.hotMin.y), static_cast<LONG>(ui.hotMax.x),
                    static_cast<LONG>(ui.hotMax.y)};
    if (ui.toggleExpanded) {
        state_->overlayExpanded = !state_->overlayExpanded;
    }
    // Botão principal do overlay pediu religar/desligar o replay: guarda a
    // requisição para o App tratar no loop (só ele conhece as flags internas).
    if (ui.resetReplayLockout) {
        replayLockoutReset_ = true;
    }
}

LRESULT CALLBACK Overlay::WndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Overlay* self = nullptr;
    if (msg == WM_NCCREATE) {
        const auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<Overlay*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<Overlay*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self == nullptr) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return self->WndProc(hwnd, msg, wParam, lParam);
}

LRESULT Overlay::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (imgui_ != nullptr) {
        ContextScope scope(imgui_);
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
            return TRUE;
        }
    }
    switch (msg) {
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;  // clicar no overlay não tira o foco do jogo/app
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
            forceRender_ = true;  // resposta imediata ao clique, mesmo com intervalo longo
            break;
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED) {
                pendingWidth_ = LOWORD(lParam);
                pendingHeight_ = HIWORD(lParam);
            }
            return 0;
        case WM_CLOSE:
            return 0;  // só fecha junto com o app
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace st
