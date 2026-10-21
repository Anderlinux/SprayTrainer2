#pragma once

#include "ui/auto_vision.h"
#include "platform/win32/capture.h"

#include "config/profile_store.h"
#include "core/repetition.h"
#include "input/raw_input.h"
#include "overlay/overlay.h"
#include "ui/app_state.h"
#include "ui/swap_target.h"

#include <d3d11.h>
#include <dxgi.h>
#include <windows.h>

#include <imgui.h>

#include <array>
#include <memory>

struct ImGuiContext;

namespace st {

// Janela nativa sem moldura + DirectX 11 + Dear ImGui.
class App {
public:
    App() = default;
    App(const App&) = delete;
    App& operator=(const App&) = delete;
    ~App();

    bool Init(HINSTANCE instance);
    int Run();

private:
    static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HitTest(LPARAM lParam) const;

    bool CreateDevice();
    void CleanupDevice();
    void RenderMain(double nowMs);

    void DrainMouse();
    void TickRepetition(double nowMs);
    void TickAutoVision(double nowMs);   // reconhecimento visual (captura->match->troca)
    void SyncRepetitionKeys();
    void CycleWeapon();                     // tecla de próxima arma (WM_HOTKEY ou polling)
    void CycleWeaponBack();                 // tecla de arma anterior (WM_HOTKEY ou polling)
    void SelectWeapon(int weaponIndex);     // troca direta (auto-reconhecimento; mesmo efeito do seletor da UI)
    bool IsRepeatButtonDown() const;   // botões do mouse como tecla de repetir
    void UnregisterAllHotkeys();
    static void SendMouseDelta(int dx, int dy);
    void ApplyWindowRequests();
    void UpdateCursorClip();
    void ApplyWindowOpacity();
    void HandleProfileRequests();
    void ApplyUiScale();
    LPARAM ScaleMouseLParam(UINT msg, LPARAM lParam) const;
    void FixMousePosition();

    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    ImGuiContext* imgui_ = nullptr;

    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
    IDXGIFactory* factory_ = nullptr;
    SwapTarget mainTarget_;
    Overlay overlay_;
    bool occluded_ = false;
    UINT resizeWidth_ = 0;
    UINT resizeHeight_ = 0;
    bool cursorClipped_ = false;
    double lastMouseCheckMs_ = 0.0;
    float appliedWindowOpacity_ = -1.0f;
    ImVec4 appliedAccent_{-1.0f, -1.0f, -1.0f, -1.0f};

    // Escala: dpiScale_ vem do monitor; uiScale_ = dpi × escala escolhida em Ajustes.
    float dpiScale_ = 1.0f;
    float uiScale_ = 0.0f;

    std::unique_ptr<ProfileStore> profileStore_;

    PatternRepeater repeater_;      // repetição automática (hotkey global; ver TickRepetition)
    double loopFrameMs_ = 16.7;     // período médio do loop de UI (EMA) -> compensação de atraso
    bool lastRepeatEnabled_ = false;   // último estado sincronizado das teclas registradas
    int lastRepeatKeyVk_ = 0;
    std::vector<int> lastStopKeysVk_{};
    int lastCycleKeyVk_ = 0;
    int lastCycleBackKeyVk_ = 0;
    bool stopKeysPending_ = false;  // WM_HOTKEY de uma tecla de desativar chegou (Tick processa)
    bool repDisabledByStopKey_ = false;  // true após toque em tecla de desativar: bloqueia
                                         // reativação pelo mouse/botão; só a tecla "1" religa
    std::array<bool, 256> stopKeyDownPrev_{};  // estado anterior do polling das teclas de desativar
    bool enableKeyDownPrev_ = false;  // borda de subida da tecla "1": religa por UM toque
                                      // quando ela é a própria tecla de disparo (perfil antigo)

    AppState state_;
    MouseRing mouse_;
};

}  // namespace st
