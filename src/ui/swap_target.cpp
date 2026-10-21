#include "ui/swap_target.h"

namespace st {

bool SwapTarget::Create(ID3D11Device* device, IDXGIFactory* factory, HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    // Modelo "blt": funciona também em janelas WS_EX_LAYERED (opacidade do overlay).
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    if (FAILED(factory->CreateSwapChain(device, &sd, &swapChain_))) {
        return false;
    }
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
    CreateRtv(device);
    return rtv_ != nullptr;
}

void SwapTarget::Resize(ID3D11Device* device, UINT width, UINT height) {
    if (swapChain_ == nullptr || width == 0 || height == 0) {
        return;
    }
    ReleaseRtv();
    swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    CreateRtv(device);
}

void SwapTarget::Release() {
    ReleaseRtv();
    if (swapChain_ != nullptr) {
        swapChain_->Release();
        swapChain_ = nullptr;
    }
}

void SwapTarget::Bind(ID3D11DeviceContext* context, const float clear[4]) {
    context->OMSetRenderTargets(1, &rtv_, nullptr);
    context->ClearRenderTargetView(rtv_, clear);
}

void SwapTarget::CreateRtv(ID3D11Device* device) {
    ID3D11Texture2D* backBuffer = nullptr;
    if (SUCCEEDED(swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) {
        device->CreateRenderTargetView(backBuffer, nullptr, &rtv_);
        backBuffer->Release();
    }
}

void SwapTarget::ReleaseRtv() {
    if (rtv_ != nullptr) {
        rtv_->Release();
        rtv_ = nullptr;
    }
}

}  // namespace st
