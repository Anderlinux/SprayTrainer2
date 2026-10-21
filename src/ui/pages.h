#pragma once

#include "ui/app_state.h"

// Aba "Auto-Reconhecimento" (Ajustes): a UI é desenhada dentro do contexto ImGui
// do App; em vez de encadear ponteiros, as páginas publicam aqui os pedidos que
// só o App pode executar (captura com GDI/DX11, decoder PNG, textures).
namespace st::pages {

struct AutoVisionUiRequests {
    bool capturePreview = false;   // prévia da região configurada
    bool testMatch = false;        // captura + comparação imediata (não troca arma)
    bool reloadTemplates = false;  // religar/desligar o auto: recarrega templates já no próximo tick
    int importWeaponIndex = -1;    // importar PNG p/ esta arma (-1 = nenhum pedido)
    int removeImageIndex = -1;     // remover imagem desta arma (-1 = nenhum)
    std::vector<unsigned char> importedPng;  // resposta do App: PNG carregado
    bool importFailed = false;                // diálogo cancelado/erro de leitura
};

namespace st::pages {

void Principal(AppState& state, double nowMs);
void Ajustes(AppState& state);
void Presets(AppState& state);
void Visual(AppState& state);
void Historico(AppState& state);

extern AutoVisionUiRequests g_autoVisionReq;

}  // namespace st::pages
