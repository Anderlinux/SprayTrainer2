#include "input/raw_input.h"

namespace st {

bool RegisterRawMouse(HWND hwnd) {
    RAWINPUTDEVICE rid{};
    rid.usUsagePage = 0x01;  // HID_USAGE_PAGE_GENERIC
    rid.usUsage = 0x02;      // HID_USAGE_GENERIC_MOUSE
    rid.dwFlags = 0;
    rid.hwndTarget = hwnd;
    return RegisterRawInputDevices(&rid, 1, sizeof(rid)) != FALSE;
}

void ReadRawMouse(LPARAM lParam, MouseRing& ring, double nowMs) {
    RAWINPUT raw{};
    UINT size = sizeof(raw);
    const UINT read = GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &raw, &size,
                                      sizeof(RAWINPUTHEADER));
    if (read == static_cast<UINT>(-1) || raw.header.dwType != RIM_TYPEMOUSE) {
        return;
    }
    const RAWMOUSE& m = raw.data.mouse;
    if ((m.usFlags & MOUSE_MOVE_ABSOLUTE) != 0) {
        return;  // tablets/desktop remoto: sem deltas relativos confiáveis
    }
    if (m.lLastX == 0 && m.lLastY == 0) {
        return;
    }
    ring.Push({static_cast<int>(m.lLastX), static_cast<int>(m.lLastY), nowMs});
}

double NowMs() {
    static const double ticksPerMs = [] {
        LARGE_INTEGER f{};
        QueryPerformanceFrequency(&f);
        return static_cast<double>(f.QuadPart) / 1000.0;
    }();
    LARGE_INTEGER c{};
    QueryPerformanceCounter(&c);
    return static_cast<double>(c.QuadPart) / ticksPerMs;
}

}  // namespace st
