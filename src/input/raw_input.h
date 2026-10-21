#pragma once

#include "core/spsc_ring.h"
#include "core/training_session.h"

#include <windows.h>

namespace st {

using MouseRing = SpscRing<MouseSample, 4096>;

// Registra a janela para receber WM_INPUT do mouse. Sem RIDEV_INPUTSINK:
// o app só lê o mouse enquanto a própria janela está em foco.
bool RegisterRawMouse(HWND hwnd);

// Lê um WM_INPUT e enfileira o movimento relativo (se houver).
void ReadRawMouse(LPARAM lParam, MouseRing& ring, double nowMs);

// Relógio monotônico em milissegundos (QueryPerformanceCounter).
double NowMs();

}  // namespace st
