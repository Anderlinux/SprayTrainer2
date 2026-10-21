#pragma once

#include <d3d11.h>
#include <dxgi.h>

namespace st {

// Swap chain + render target de uma janela (a principal e o overlay usam um cada).
class SwapTarget {
public:
    SwapTarget() = default;
    SwapTarget(const SwapTarget&) = delete;
    SwapTarget& operator=(const SwapTarget&) = delete;
    ~SwapTarget() { Release(); }

    bool Create(ID3D11Device* device, IDXGIFactory* factory, HWND hwnd);
    void Resize(ID3D11Device* device, UINT width, UINT height);
    void Release();

    void Bind(ID3D11DeviceContext* context, const float clear[4]);
    HRESULT Present(UINT syncInterval, UINT flags) { return swapChain_->Present(syncInterval, flags); }

private:
    void CreateRtv(ID3D11Device* device);
    void ReleaseRtv();

    IDXGISwapChain* swapChain_ = nullptr;
    ID3D11RenderTargetView* rtv_ = nullptr;
};

}  // namespace st
