#pragma once

#include <d3d11.h>
#include <imgui.h>

#include <cstddef>  // std::size_t (MSVC: d3d11/imgui não garantem este tipo)

namespace st {

struct Texture {
    ID3D11ShaderResourceView* srv = nullptr;
    float width = 0.0f;
    float height = 0.0f;

    ImTextureID Id() const { return reinterpret_cast<ImTextureID>(srv); }
    bool Valid() const { return srv != nullptr; }
};

// Decodifica um PNG embutido como RCDATA (via WIC) e cria a textura DX11.
// Exige COM inicializado na thread.
bool LoadTextureFromResource(ID3D11Device* device, int resourceId, Texture& out);
// Mesma decodificação WIC+DX11, a partir de um PNG em memória (prévia do
// reconhecimento: imagens cadastradas e última captura).
bool LoadTextureFromMemory(ID3D11Device* device, const unsigned char* data, std::size_t size, Texture& out);
void ReleaseTexture(Texture& tex);

}  // namespace st
