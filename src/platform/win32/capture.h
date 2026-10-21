#pragma once

#include "ui/auto_vision.h"

#include <string>
#include <vector>

namespace st {

// Implementação Win32 da captura de tela (BitBlt em DC compatível). É a única
// borda nativa do reconhecimento visual; toda a lógica fica em AutoVisionController
// (portável e testada em Linux). Chamada apenas na thread da UI, no ritmo de
// autoIntervalMs — nunca em thread separada.
AutoVisionController::CaptureResult CaptureScreenRegion(const ProfileData& profile);

// Versão que lê a região diretamente do estado da UI (mesma fonte que a aba
// "Auto-Reconhecimento" edita ao vivo).
AutoVisionController::CaptureResult CaptureAppStateRegion(const AppState& state);

// Lista os títulos UTF-8 das janelas de nível superior visíveis, para o combo
// da aba de reconhecimento (limitada a `max` itens).
std::vector<std::string> ListTopLevelWindowTitles(std::size_t max = 64);

}  // namespace st
