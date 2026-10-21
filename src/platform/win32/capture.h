#pragma once

#include "ui/auto_vision.h"

namespace st {

// Implementação Win32 da captura de tela (BitBlt em DC compatível). É a única
// borda nativa do reconhecimento visual; toda a lógica fica em AutoVisionController
// (portável e testada em Linux). Chamada apenas na thread da UI, no ritmo de
// autoIntervalMs — nunca em thread separada.
AutoVisionController::CaptureResult CaptureScreenRegion(const ProfileData& profile);

}  // namespace st
