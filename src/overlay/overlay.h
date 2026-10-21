#pragma once

#include "ui/app_state.h"
#include "ui/swap_target.h"

#include <imgui.h>

#include <d3d11.h>
#include <windows.h>

struct ImGuiContext;

namespace st {

// Janela pequena, sempre no topo e sem foco. No modo compacto só o botão de
// expandir recebe clique (o resto deixa o clique atravessar); expandida, vira
// um painel de configurações. Tem contexto ImGui próprio e usa o mesmo device DX11.
class Overlay {
public:
    Overlay() = default;
    Overlay(const Overlay&) = delete;
    Overlay& operator=(const Overlay&) = delete;
    ~Overlay() { Shutdown(); }

    bool Init(HINSTANCE instance, ID3D11Device* device, ID3D11DeviceContext* context, IDXGIFactory* factory,
              AppState* state);
    void Shutdown();

    // Chamado a cada volta do loop principal; desenha quando vence o intervalo.
    void Update(double nowMs);

    // true se o botão principal do overlay pediu para religar/desligar o replay
    // no último frame (o App consome com ConsumeReplayLockoutReset).
    bool ReplayLockoutResetRequested() const { return replayLockoutReset_; }
    // Limpa a requisição pendente depois de tratada pelo App.
    void ConsumeReplayLockoutReset() { replayLockoutReset_ = false; }

private:
    static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void ApplyLayout();
    void UpdateClickThrough();
    void SetClickThrough(bool enabled);
    void Render();

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    ImGuiContext* imgui_ = nullptr;
    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
    SwapTarget target_;
    AppState* state_ = nullptr;

    // Último layout aplicado (para só mexer na janela quando algo muda).
    bool shown_ = false;
    bool appliedExpanded_ = false;
    float appliedScale_ = 0.0f;
    float appliedOpacity_ = -1.0f;
    OverlayCorner appliedCorner_ = OverlayCorner::BottomRight;
    RECT appliedWorkArea_{};
    ImVec4 appliedAccent_{-1.0f, -1.0f, -1.0f, -1.0f};

    bool clickThrough_ = true;
    RECT hotRect_{};  // área clicável (cliente) no modo compacto
    bool replayLockoutReset_ = false;  // botão do overlay pediu religar/desligar o replay
    UINT pendingWidth_ = 0;
    UINT pendingHeight_ = 0;
    double lastRenderMs_ = 0.0;
    double lastTopmostMs_ = 0.0;
    bool forceRender_ = true;
};

}  // namespace st
