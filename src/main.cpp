#include "ui/app.h"

#include <objbase.h>
#include <windows.h>

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int) {
    // COM é usado pelo WIC para decodificar os PNGs embutidos.
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int code = 1;
    {
        st::App app;
        if (app.Init(instance)) {
            code = app.Run();
        } else {
            MessageBoxW(nullptr, L"Não foi possível iniciar o DirectX 11.", L"Spray Trainer", MB_ICONERROR);
        }
    }
    if (SUCCEEDED(com)) {
        CoUninitialize();
    }
    return code;
}
