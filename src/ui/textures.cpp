#include "ui/textures.h"

#include <wincodec.h>
#include <windows.h>

#include <vector>

namespace st {

namespace {

// Libera a interface COM ao sair do escopo.
template <typename T>
class ComRef {
public:
    ComRef() = default;
    ComRef(const ComRef&) = delete;
    ComRef& operator=(const ComRef&) = delete;
    ~ComRef() {
        if (ptr_ != nullptr) {
            ptr_->Release();
        }
    }
    T** Put() { return &ptr_; }
    T* operator->() const { return ptr_; }
    T* Get() const { return ptr_; }

private:
    T* ptr_ = nullptr;
};

}  // namespace

bool LoadTextureFromResource(ID3D11Device* device, int resourceId, Texture& out) {
    HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(resourceId), MAKEINTRESOURCEW(10));  // RT_RCDATA
    if (res == nullptr) {
        return false;
    }
    HGLOBAL handle = LoadResource(nullptr, res);
    const DWORD size = SizeofResource(nullptr, res);
    void* data = handle != nullptr ? LockResource(handle) : nullptr;
    if (data == nullptr || size == 0) {
        return false;
    }

    ComRef<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory,
                                reinterpret_cast<void**>(factory.Put())))) {
        return false;
    }
    ComRef<IWICStream> stream;
    ComRef<IWICBitmapDecoder> decoder;
    ComRef<IWICBitmapFrameDecode> frame;
    ComRef<IWICFormatConverter> converter;
    if (FAILED(factory->CreateStream(stream.Put())) ||
        FAILED(stream->InitializeFromMemory(static_cast<BYTE*>(data), size)) ||
        FAILED(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, decoder.Put())) ||
        FAILED(decoder->GetFrame(0, frame.Put())) || FAILED(factory->CreateFormatConverter(converter.Put())) ||
        FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr,
                                     0.0, WICBitmapPaletteTypeCustom))) {
        return false;
    }

    UINT w = 0;
    UINT h = 0;
    if (FAILED(converter->GetSize(&w, &h)) || w == 0 || h == 0) {
        return false;
    }
    std::vector<BYTE> pixels(static_cast<std::size_t>(w) * h * 4);
    if (FAILED(converter->CopyPixels(nullptr, w * 4, static_cast<UINT>(pixels.size()), pixels.data()))) {
        return false;
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = pixels.data();
    init.SysMemPitch = w * 4;

    ComRef<ID3D11Texture2D> texture;
    if (FAILED(device->CreateTexture2D(&desc, &init, texture.Put()))) {
        return false;
    }
    D3D11_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format = desc.Format;
    view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    view.Texture2D.MipLevels = 1;
    if (FAILED(device->CreateShaderResourceView(texture.Get(), &view, &out.srv))) {
        return false;
    }
    out.width = static_cast<float>(w);
    out.height = static_cast<float>(h);
    return true;
}

bool LoadTextureFromMemory(ID3D11Device* device, const unsigned char* data, std::size_t size, Texture& out) {
    if (device == nullptr || data == nullptr || size == 0) {
        return false;
    }
    ComRef<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory,
                                reinterpret_cast<void**>(factory.Put())))) {
        return false;
    }
    ComRef<IWICStream> stream;
    ComRef<IWICBitmapDecoder> decoder;
    ComRef<IWICBitmapFrameDecode> frame;
    ComRef<IWICFormatConverter> converter;
    // WIC aceita o buffer somente durante InitializeFromMemory? Não: ele mantém
    // referência — mas nossos PNGs vêm de vetores que vivem enquanto a textura
    // é criada (uso local na UI), e CopPixels copia antes de sair do escopo.
    if (FAILED(factory->CreateStream(stream.Put())) ||
        FAILED(stream->InitializeFromMemory(static_cast<BYTE*>(const_cast<unsigned char*>(data)),
                                            static_cast<UINT>(size))) ||
        FAILED(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, decoder.Put())) ||
        FAILED(decoder->GetFrame(0, frame.Put())) || FAILED(factory->CreateFormatConverter(converter.Put())) ||
        FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr,
                                     0.0, WICBitmapPaletteTypeCustom))) {
        return false;
    }

    UINT w = 0;
    UINT h = 0;
    if (FAILED(converter->GetSize(&w, &h)) || w == 0 || h == 0) {
        return false;
    }
    std::vector<BYTE> pixels(static_cast<std::size_t>(w) * h * 4);
    if (FAILED(converter->CopyPixels(nullptr, w * 4, static_cast<UINT>(pixels.size()), pixels.data()))) {
        return false;
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = pixels.data();
    init.SysMemPitch = w * 4;

    ComRef<ID3D11Texture2D> texture;
    if (FAILED(device->CreateTexture2D(&desc, &init, texture.Put()))) {
        return false;
    }
    D3D11_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format = desc.Format;
    view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    view.Texture2D.MipLevels = 1;
    if (FAILED(device->CreateShaderResourceView(texture.Get(), &view, &out.srv))) {
        return false;
    }
    out.width = static_cast<float>(w);
    out.height = static_cast<float>(h);
    return true;
}

void ReleaseTexture(Texture& tex) {
    if (tex.srv != nullptr) {
        tex.srv->Release();
        tex.srv = nullptr;
    }
}

}  // namespace st
