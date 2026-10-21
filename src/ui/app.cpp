#include "ui/app.h"

#include "config/paths_win.h"
#include "core/repetition.h"
#include "ui/file_dialog.h"
#include "ui/main_window.h"
#include "ui/profiles.h"
#include "ui/theme.h"

#include "resource.h"

#include <dwmapi.h>
#include <shellapi.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <windowsx.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace st {

namespace {

constexpr wchar_t kClassName[] = L"SprayTrainerWindow";
constexpr int kDefaultWidth = 1120;
constexpr int kDefaultHeight = 820;
constexpr int kMinWidth = 1000;
constexpr int kMinHeight = 700;
constexpr double kMouseCheckIntervalMs = 2000.0;

bool AnyMousePresent() {
    UINT count = 0;
    if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) != 0 || count == 0) {
        return false;
    }
    std::vector<RAWINPUTDEVICELIST> list(count);
    const UINT got = GetRawInputDeviceList(list.data(), &count, sizeof(RAWINPUTDEVICELIST));
    if (got == static_cast<UINT>(-1)) {
        return false;
    }
    for (UINT i = 0; i < got; ++i) {
        if (list[i].dwType == RIM_TYPEMOUSE) {
            return true;
        }
    }
    return false;
}
constexpr int kResizeBorder = 6;

// Valores do SDK do Windows 11 (DWMWA_WINDOW_CORNER_PREFERENCE / DWMWCP_ROUND),
// definidos aqui para compilar também com SDKs mais antigos.
constexpr DWORD kDwmCornerPreference = 33;
constexpr DWORD kDwmCornerRound = 2;

// Ids das hotkeys globais da repetição (WM_HOTKEY). O id da tecla de repetir é fixo;
// as teclas de desativar usam uma faixa separada e o VK chega no lParam do registro.
constexpr int kRepeatHotkeyId = 0xB001;
constexpr int kEnableHotkeyId = 0xB002;   // tecla fixa "1": religa o replay
constexpr int kStopHotkeyIdBase = 0xB010;
constexpr int kCycleWeaponHotkeyId = 0xB020;
constexpr int kCycleBackHotkeyId = 0xB021;   // tecla de arma anterior (volta)

// true se o id de um WM_HOTKEY pertence à faixa das teclas de desativar.
bool IsStopHotkeyId(int id) {
    return id >= kStopHotkeyIdBase && id < kStopHotkeyIdBase + static_cast<int>(kMaxStopKeys);
}

}  // namespace

// VKs dos botões do mouse. RegisterHotKey não aceita botões (WM_HOTKEY é gerado só
// por teclado), então eles são vigiados via GetAsyncKeyState no TickRepetition —
// igual ao código de referência, que usa a hotkey com GetAsyncKeyState em loop.
// (Fora do anônimo: o overlay usa esta função no botão Ligar/Desligar replay.)
bool IsMouseButtonVk(int vk) {
    return vk == kVkLButton || vk == kVkRButton || vk == kVkMButton ||
           vk == kVkXButton1 || vk == kVkXButton2;
}

App::~App() {
    UnregisterAllHotkeys();
    if (cursorClipped_) {
        ClipCursor(nullptr);
    }
    overlay_.Shutdown();
    for (std::size_t i = 0; i < kWeaponCount; ++i) {
        if (state_.weaponIcons[i] != nullptr) {
            Texture tex{state_.weaponIcons[i], state_.weaponIconW[i], state_.weaponIconH[i]};
            ReleaseTexture(tex);
            state_.weaponIcons[i] = nullptr;
        }
    }
    if (imgui_ != nullptr) {
        ImGui::SetCurrentContext(imgui_);
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(imgui_);
        imgui_ = nullptr;
    }
    CleanupDevice();
    if (hwnd_ != nullptr) {
        DestroyWindow(hwnd_);
    }
    if (instance_ != nullptr) {
        UnregisterClassW(kClassName, instance_);
    }
}

namespace {

// A UI é montada em unidades lógicas (escala 1.0) e só os vértices são multiplicados na hora
// de desenhar. As fontes já são rasterizadas no tamanho final (FontGlobalScale = 1/escala),
// então o texto fica nítido em qualquer escala sem mexer em cada medida da interface.
void ScaleDrawData(ImDrawData* data, float scale) {
    if (scale == 1.0f) {
        return;
    }
    for (ImDrawList* list : data->CmdLists) {
        for (ImDrawVert& v : list->VtxBuffer) {
            v.pos.x *= scale;
            v.pos.y *= scale;
        }
    }
    data->ScaleClipRects(ImVec2(scale, scale));
    data->DisplayPos = ImVec2(data->DisplayPos.x * scale, data->DisplayPos.y * scale);
    data->DisplaySize = ImVec2(data->DisplaySize.x * scale, data->DisplaySize.y * scale);
}

}  // namespace

bool App::Init(HINSTANCE instance) {
    instance_ = instance;
    ImGui_ImplWin32_EnableDpiAwareness();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = &App::WndProcThunk;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP));
    wc.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                                GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
    wc.lpszClassName = kClassName;
    if (RegisterClassExW(&wc) == 0) {
        return false;
    }

    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int workW = work.right - work.left;
    const int workH = work.bottom - work.top;
    const POINT origin{0, 0};
    dpiScale_ = ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY));
    const int wantW = static_cast<int>(static_cast<float>(kDefaultWidth) * dpiScale_);
    const int wantH = static_cast<int>(static_cast<float>(kDefaultHeight) * dpiScale_);
    const int width = workW > 0 ? std::min(wantW, workW - 40) : wantW;
    const int height = workH > 0 ? std::min(wantH, workH - 40) : wantH;
    const int x = work.left + (workW - width) / 2;
    const int y = work.top + (workH - height) / 2;

    const DWORD style = WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    hwnd_ = CreateWindowExW(WS_EX_APPWINDOW, kClassName, L"Spray Trainer", style, x, y, width, height,
                            nullptr, nullptr, instance, this);
    if (hwnd_ == nullptr) {
        return false;
    }

    appliedWindowOpacity_ = 1.0f;
    const DWORD corner = kDwmCornerRound;
    DwmSetWindowAttribute(hwnd_, kDwmCornerPreference, &corner, sizeof(corner));

    if (!CreateDevice()) {
        return false;
    }
    if (!RegisterRawMouse(hwnd_)) {
        return false;
    }

    ShowWindow(hwnd_, SW_SHOWDEFAULT);
    UpdateWindow(hwnd_);

    IMGUI_CHECKVERSION();
    imgui_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui_);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // layout é fixo; configurações vão para o perfil JSON (Etapa 5)
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    theme::SetAccent(state_.ui.accent);
    theme::Apply();
    dpiScale_ = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd_);
    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX11_Init(device_, context_);

    // Silhuetas das armas (PNG embutido). Se falhar, a UI mostra só o nome.
    const int iconIds[kWeaponCount] = {IDR_WEAPON_AK47,  IDR_WEAPON_M4A4,  IDR_WEAPON_M4A1S, IDR_WEAPON_FAMAS,
                                       IDR_WEAPON_GALILAR, IDR_WEAPON_MAC10, IDR_WEAPON_MP9,   IDR_WEAPON_MP5SD,
                                       IDR_WEAPON_MP7,  IDR_WEAPON_P90,   IDR_WEAPON_BIZON, IDR_WEAPON_SG553,
                                       IDR_WEAPON_UMP45};
    for (std::size_t i = 0; i < kWeaponCount; ++i) {
        Texture tex;
        if (LoadTextureFromResource(device_, iconIds[i], tex)) {
            // Estado portável guarda só o ponteiro opaco + dimensões (sem imgui).
            state_.weaponIcons[i] = tex.srv;
            state_.weaponIconW[i] = tex.width;
            state_.weaponIconH[i] = tex.height;
        }
    }

    // Overlay: falhar aqui não impede o app de abrir.
    if (!overlay_.Init(instance, device_, context_, factory_, &state_)) {
        overlay_.Shutdown();
    }

    for (std::size_t i = 0; i < kWeaponCount; ++i) {
        state_.savedPatterns[i] = state_.patterns.Get(static_cast<Weapon>(i));
    }

    // Perfis: carrega o último usado (ou padrões, com aviso se o arquivo estiver com problema).
    profileStore_ = std::make_unique<ProfileStore>(DataDir());
    state_.profiles.store = profileStore_.get();
    state_.nowMs = NowMs();
    profiles::Startup(state_);

    state_.appStartMs = NowMs();
    state_.mouseDetected = AnyMousePresent();
    lastMouseCheckMs_ = state_.appStartMs;
    return true;
}

int App::Run() {
    MSG msg{};
    double lastLoopMs = NowMs();
    for (;;) {
        while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) {
                return static_cast<int>(msg.wParam);
            }
        }

        DrainMouse();
        const double now = NowMs();
        // Média móvel exponencial do período do loop: alimenta a compensação de
        // atraso da repetição (o SendInput só vira nota quando DrainMouse processa,
        // ~meio frame depois). Com vsync ≈16,7ms; sem vsync tende a poucos ms.
        const double dt = std::max(0.5, now - lastLoopMs);
        lastLoopMs = now;
        loopFrameMs_ += (dt - loopFrameMs_) * 0.1;
        repeater_.SetUiFrameMs(loopFrameMs_);
        state_.nowMs = now;
        TickRepetition(now);
        if (now - lastMouseCheckMs_ >= kMouseCheckIntervalMs) {
            state_.mouseDetected = AnyMousePresent();
            lastMouseCheckMs_ = now;
        }

        // A janela principal minimizada/coberta não desenha, mas o overlay continua.
        const bool mainHidden = IsIconic(hwnd_) != FALSE ||
                                (occluded_ && mainTarget_.Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED);
        if (!mainHidden) {
            occluded_ = false;
            RenderMain(now);
        }
        overlay_.Update(now);
        // O botão "Ligar/Desligar replay" do overlay limpa a trava deixada por
        // uma tecla de desativar e religa na hora (começando do zero).
        if (overlay_.ReplayLockoutResetRequested()) {
            overlay_.ConsumeReplayLockoutReset();
            repDisabledByStopKey_ = false;
            enableKeyDownPrev_ = true;  // anti-retrigger até soltar (não afeta a tecla "1")
            if (state_.repetition.enabled && !repeater_.Running()) {
                if (state_.session.Running()) {
                    state_.session.Stop(now);
                }
                repeater_.Stop(now, /*returnBack=*/false);
                repeater_.Configure(state_.weapon, state_.patterns.Get(state_.weapon), state_.sensitivity,
                                    state_.repetition.substeps);
                repeater_.Start(now);
                state_.repeatToggledOn = true;
                state_.repeatArmed = false;
            }
        }
        ApplyWindowRequests();
        ApplyWindowOpacity();
        HandleProfileRequests();
        if (mainHidden) {
            Sleep(10);  // sem vsync da janela principal, evita girar o loop a 100% de CPU
        }
    }
}

void App::RenderMain(double nowMs) {
    ImGui::SetCurrentContext(imgui_);
    // Cor de destaque mudou (Ajustes ou perfil carregado): reaplica o estilo deste contexto.
    const ImVec4 accent = theme::Accent();
    if (accent.x != appliedAccent_.x || accent.y != appliedAccent_.y || accent.z != appliedAccent_.z) {
        theme::Apply();
        appliedAccent_ = accent;
    }
    ApplyUiScale();
    if (resizeWidth_ != 0 && resizeHeight_ != 0) {
        mainTarget_.Resize(device_, resizeWidth_, resizeHeight_);
        resizeWidth_ = resizeHeight_ = 0;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(io.DisplaySize.x / uiScale_, io.DisplaySize.y / uiScale_);
    FixMousePosition();
    ImGui::NewFrame();
    state_.maximized = IsZoomed(hwnd_) != FALSE;
    RenderMainWindow(state_, nowMs);
    ImGui::Render();
    UpdateCursorClip();

    const float clear[4] = {theme::kBg.x, theme::kBg.y, theme::kBg.z, 1.0f};
    mainTarget_.Bind(context_, clear);
    ScaleDrawData(ImGui::GetDrawData(), uiScale_);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    occluded_ = mainTarget_.Present(1, 0) == DXGI_STATUS_OCCLUDED;
}

void App::DrainMouse() {
    // Treino (tela Principal) e gravação de padrão (editor) usam a mesma medição.
    TrainingSession* sessions[] = {&state_.session, &state_.recorder.session};
    MouseSample sample;
    while (mouse_.Pop(sample)) {
        for (TrainingSession* session : sessions) {
            if (!session->Running() || sample.timeMs < session->StartMs()) {
                continue;
            }
            // Registra os tiros que aconteceram antes deste movimento chegar.
            session->Tick(sample.timeMs);
            session->AddMovement(sample.dx, sample.dy);
        }
    }
    const double now = NowMs();
    for (TrainingSession* session : sessions) {
        session->Tick(now);
    }
}

void App::SendMouseDelta(int dx, int dy) {
    if (dx == 0 && dy == 0) {
        return;
    }
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dx = dx;
    input.mi.dy = dy;
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_MOVE_NOCOALESCE;
    SendInput(1, &input, sizeof(INPUT));
}

void App::UnregisterAllHotkeys() {
    if (hwnd_ == nullptr) {
        return;
    }
    if (state_.repeatHotkeyId != 0) {
        UnregisterHotKey(hwnd_, state_.repeatHotkeyId);
        state_.repeatHotkeyId = 0;
    }
    for (std::size_t i = 0; i < kMaxStopKeys; ++i) {
        UnregisterHotKey(hwnd_, kStopHotkeyIdBase + static_cast<int>(i));
    }
    UnregisterHotKey(hwnd_, kCycleWeaponHotkeyId);
    UnregisterHotKey(hwnd_, kCycleBackHotkeyId);
    UnregisterHotKey(hwnd_, kEnableHotkeyId);
    state_.registeredStopKeys.clear();
}

void App::SyncRepetitionKeys() {
    const RepetitionSettings& r = state_.repetition;
    if (r.enabled == lastRepeatEnabled_ && r.repeatKeyVk == lastRepeatKeyVk_ &&
        r.stopKeysVk == lastStopKeysVk_ && r.cycleWeaponKeyVk == lastCycleKeyVk_ &&
        r.cycleBackKeyVk == lastCycleBackKeyVk_) {
        return;  // nada mudou: sem churn de registro a cada frame
    }

    UnregisterAllHotkeys();
    repDisabledByStopKey_ = false;  // troca de configuração: trava antiga não vale mais
    if (!r.enabled || hwnd_ == nullptr) {
        state_.repeatArmed = true;
        state_.repeatToggledOn = false;
        lastRepeatEnabled_ = r.enabled;
        lastRepeatKeyVk_ = r.repeatKeyVk;
        lastStopKeysVk_ = r.stopKeysVk;
        lastCycleKeyVk_ = r.cycleWeaponKeyVk;
        lastCycleBackKeyVk_ = r.cycleBackKeyVk;
        return;
    }

    // Tecla que dispara a repetição. MOD_NOREPEAT evita o auto-repeat do teclado;
    // sem modificadores, o primeiro WM_KEYDOWN já gera a mensagem.
    // Botões do mouse não podem ser hotkey global (RegisterHotKey os rejeita):
    // são vigiados via GetAsyncKeyState em IsRepeatButtonDown/TickRepetition.
    // A tecla "1" (kDefaultEnableKeyVk) também NÃO é registrada como hotkey de
    // disparo: a hotkey global engoliria o KEYDOWN no jogo e impediria selecionar
    // a arma 1 (mesmo bug das key2/key3). Quando "1" é a fire key, o disparo é
    // detectado pelo polling de borda de subida no TickRepetition.
    if (IsMouseButtonVk(r.repeatKeyVk)) {
        state_.repeatHotkeyId = 0;  // sem WM_HOTKEY; detecção por polling no loop
    } else if (r.repeatKeyVk == kDefaultEnableKeyVk) {
        state_.repeatHotkeyId = 0;  // "1" nunca vira hotkey global; polling no loop
    } else if (RegisterHotKey(hwnd_, kRepeatHotkeyId, MOD_NOREPEAT, static_cast<UINT>(r.repeatKeyVk))) {
        state_.repeatHotkeyId = kRepeatHotkeyId;
    } else {
        state_.profiles.banner = "Não foi possível registrar a tecla de repetir (" +
                                 std::string(VirtualKeyName(r.repeatKeyVk)) + "): pode estar em uso por outro app.";
        state_.profiles.bannerError = true;
        state_.profiles.bannerUntilMs = 1.0e300;
    }

    // Tecla FIXA de religar ("1", kDefaultEnableKeyVk): NÃO é mais hotkey global
    // — mesma solução aplicada às teclas de desativar (bug relatado: "key2/key3
    // param de funcionar até eu religar com key1"; agora vale também para "1").
    // Enquanto "1" estava registrada como hotkey, o Windows engolia o KEYDOWN
    // dela em QUALQUER janela: no jogo, trocar para a pistola bindada em "1"
    // ficava impossível enquanto a repetição estivesse ativa. A detecção passou
    // a ser por polling GetAsyncKeyState + borda de subida em TickRepetition,
    // que funciona em qualquer foco sem interferir no jogo nem exigir o app em
    // primeiro plano. O caminho WM_HOTKEY (kEnableHotkeyId) continua vivo APENAS
    // como redundância para botões do mouse configurados como tecla de ativar em
    // perfis antigos (botões não geram WM_KEYDOWN, mas RegisterHotKey também os
    // rejeita — esses já são cobertos pelo polling da própria tecla de disparo).

    // Várias teclas para desativar. Botões do mouse não podem ser hotkey global
    // (RegisterHotKey os rejeita), então ficam vigiados por GetAsyncKeyState.
    // NÃO registra mais as teclas de desativar como hotkey global: uma hotkey
    // global engole o toque no app em primeiro plano — ao pressionar "3" com a
    // janela do app em foco, o Windows entregava só WM_HOTKEY e o jogo nunca
    // recebia o KEYDOWN, impedindo de trocar de arma (bug relatado: "key2/key3
    // param de funcionar até eu religar com key1"). A detecção por polling em
    // TickRepetition (GetAsyncKeyState + borda de subida) continua funcionando
    // em qualquer janela sem interferir no jogo; o caminho WM_HOTKEY segue vivo
    // apenas para botões do mouse registrados como desativar em perfis antigos
    // (e para o handler de stopKeysPending_, que agora também filtra pelo VK).
    // IMPORTANTE: os IDs de hotkey seguem um contador próprio (não o índice na
    // lista), porque a lista pode ter buracos (teclas de teclado ignoradas).
    // Se o ID fosse o índice, "2" ocupando a posição 1 depois de um botão na
    // posição 0 colidiria com outros IDs e o toque em "2" religaria o replay em
    // vez de desligar (bug antigo: clicar em atirar religava tudo).
    // A tecla fixa de religar ("1") NUNCA é registrada como desativar — se um
    // perfil antigo a tiver na lista, ela é ignorada aqui e vira exclusivamente
    // a tecla de religar (comportamento pedido: "1" sempre ativa).
    int stopHotkeySlot = 0;
    for (std::size_t i = 0; i < r.stopKeysVk.size() && i < kMaxStopKeys; ++i) {
        const int vk = r.stopKeysVk[i];
        if (vk == r.repeatKeyVk || vk == kDefaultEnableKeyVk) {
            continue;  // nunca registrar a tecla de disparo nem a de religar como desativar
        }
        if (IsMouseButtonVk(vk) && stopHotkeySlot < static_cast<int>(kMaxStopKeys) &&
            RegisterHotKey(hwnd_, kStopHotkeyIdBase + stopHotkeySlot, MOD_NOREPEAT, static_cast<UINT>(vk))) {
            state_.registeredStopKeys.push_back(vk);
            ++stopHotkeySlot;
        }
    }

    // Teclas para alternar armas (padrão: F1 avança, F2 volta): funcionam mesmo
    // com o jogo em foco e também com botões do mouse (esses via polling em
    // TickRepetition, pois não geram WM_HOTKEY). A tecla de voltar só é
    // registrada se for diferente da de avançar (evita conflito de id/VK).
    if (r.cycleWeaponKeyVk != 0 && !IsMouseButtonVk(r.cycleWeaponKeyVk)) {
        if (!RegisterHotKey(hwnd_, kCycleWeaponHotkeyId, MOD_NOREPEAT,
                            static_cast<UINT>(r.cycleWeaponKeyVk))) {
            state_.profiles.banner = "Não foi possível registrar a tecla de alternar armas (" +
                                         std::string(VirtualKeyName(r.cycleWeaponKeyVk)) +
                                         "): pode estar em uso por outro app.";
            state_.profiles.bannerError = true;
            state_.profiles.bannerUntilMs = 1.0e300;
        }
    }
    state_.cycleHotkeyDown = false;
    if (r.cycleBackKeyVk != 0 && r.cycleBackKeyVk != r.cycleWeaponKeyVk &&
        !IsMouseButtonVk(r.cycleBackKeyVk)) {
        if (!RegisterHotKey(hwnd_, kCycleBackHotkeyId, MOD_NOREPEAT,
                            static_cast<UINT>(r.cycleBackKeyVk))) {
            state_.profiles.banner = "Não foi possível registrar a tecla de arma anterior (" +
                                         std::string(VirtualKeyName(r.cycleBackKeyVk)) +
                                         "): pode estar em uso por outro app.";
            state_.profiles.bannerError = true;
            state_.profiles.bannerUntilMs = 1.0e300;
        }
    }
    state_.cycleBackHotkeyDown = false;
    state_.repeatArmed = true;
    lastRepeatEnabled_ = r.enabled;
    lastRepeatKeyVk_ = r.repeatKeyVk;
    lastStopKeysVk_ = r.stopKeysVk;
    lastCycleKeyVk_ = r.cycleWeaponKeyVk;
    lastCycleBackKeyVk_ = r.cycleBackKeyVk;
}

bool App::IsRepeatButtonDown() const {
    // GetAsyncKeyState reflete o estado real mesmo quando outra janela (o jogo)
    // recebe os cliques; a exceção conhecida são cliques enviados pelo próprio app
    // via SendInput — que a repetição nunca faz, apenas movimento. Para teclas de
    // teclado SEM modificadores, RegisterHotKey entrega WM_KEYDOWN/WM_KEYUP no
    // thread da fila e o polling funciona; com modificador (ex.: CTRL), o KEYUP do
    // modificador pode ser suprimido e o bit ficar "preso" — por isso essas teclas
    // continuam dependendo do caminho WM_HOTKEY (que trata o par completo).
    const int vk = state_.repetition.repeatKeyVk;
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

// Troca para a próxima arma (ida e volta pelo enum). Atualiza também a arma do
// editor, igual aos seletores de arma da interface.
void App::CycleWeapon() {
    const int next = (static_cast<int>(state_.weapon) + 1) % static_cast<int>(Weapon::Count);
    state_.weapon = static_cast<Weapon>(next);
    state_.editorWeapon = state_.weapon;
}

// Volta para a arma anterior (wrap-around: da primeira vai para a última).
void App::CycleWeaponBack() {
    const int prev = (static_cast<int>(state_.weapon) - 1 + static_cast<int>(Weapon::Count)) %
                     static_cast<int>(Weapon::Count);
    state_.weapon = static_cast<Weapon>(prev);
    state_.editorWeapon = state_.weapon;
}

void App::TickRepetition(double nowMs) {
    SyncRepetitionKeys();

    // Teclas de desativar: duas fontes, para cobrir todos os casos.
    // 1) WM_HOTKEY (handler abaixo): hoje só para botões do mouse registrados
    //    como desativar em perfis antigos. Teclas de teclado NÃO são mais
    //    hotkey global — a hotkey engolia o toque quando o app estava em foco
    //    e impedia o jogo de trocar de arma (bug das key2/key3). Basta UM
    //    toque: o evento chega na primeira premsão e o Tick desliga
    //    imediatamente (stopKeysPending_).
    // 2) Polling GetAsyncKeyState com detecção de BORDA DE SUBIDA: caminho
    //    principal das teclas de desativar. Não interfere no jogo nem rouba o
    //    toque da janela em foco; um simples toque já desativa; segurar a tecla
    //    NÃO mantém nada bloqueado nem exige soltar para funcionar de novo
    //    (anti-"preso": só a transição solta->pressionada conta como clique).
    if (stopKeysPending_) {
        stopKeysPending_ = false;
        if (repeater_.Running()) {
            repeater_.Stop(nowMs, /*returnBack=*/false);
        }
        // Trava de reativação: depois de um toque em tecla de desativar, o replay
        // NÃO pode voltar por clique do mouse/botão configurado como tecla de ativar.
        // Só a tecla "1" (WM_HOTKEY) religa. Se essa tecla for um botão do mouse,
        // ele precisa ser soltado antes (anti-retrigger) — mas nunca religa sozinho.
        repDisabledByStopKey_ = true;
        state_.repeatArmed = !IsMouseButtonVk(state_.repetition.repeatKeyVk);
        state_.repeatToggledOn = false;
        // Anti-retrigger da tecla física: se "1" foi tocada para DESLIGAR (perfil
        // antigo em que ela é a tecla de disparo/desativar), segurar ou pressionar
        // de novo não pode religar na sequência — só um toque limpo depois de solta.
        enableKeyDownPrev_ = (GetAsyncKeyState(kDefaultEnableKeyVk) & 0x8000) != 0;
    }
    // Polling das teclas de DESATIVAR: basta UM clique (borda de subida). Não há
    // mais bloqueio "enquanto segurada" — o replay para na hora do toque e a tecla
    // pode continuar pressionada sem efeito nenhum (só um novo toque em "1" religa).
    bool stopClicked = false;              // alguma tecla de desativar foi tocada agora
    std::array<bool, 256> stopDown{};      // snapshot do estado atual (indexado por VK)
    for (int vk : state_.repetition.stopKeysVk) {
        if (vk < 0 || vk >= 256) {
            continue;
        }
        stopDown[vk] = (GetAsyncKeyState(vk) & 0x8000) != 0;
        if (stopDown[vk] && !stopKeyDownPrev_[vk] && vk != kDefaultEnableKeyVk) {
            stopClicked = true;  // a tecla fixa de religar nunca desativa (ver SyncRepetitionKeys)
        }
    }
    stopKeyDownPrev_ = stopDown;
    if (stopClicked) {
        if (repeater_.Running()) {
            repeater_.Stop(nowMs, /*returnBack=*/false);
        }
        // Mesma trava do caminho WM_HOTKEY: só a tecla de ativar ("1") religa.
        repDisabledByStopKey_ = true;
        state_.repeatArmed = !IsMouseButtonVk(state_.repetition.repeatKeyVk);
        state_.repeatToggledOn = false;
        enableKeyDownPrev_ = (GetAsyncKeyState(kDefaultEnableKeyVk) & 0x8000) != 0;
    }

    // Polling da tecla de religar ("1"): AGORA É O CAMINHO PRINCIPAL (a tecla
    // "1" não é mais hotkey global — ver SyncRepetitionKeys; o jogo recebe o
    // KEYDOWN normalmente e pode trocar de arma com "1" em qualquer estado).
    // GetAsyncKeyState enxerga o toque mesmo sem foco. Dois casos:
    //  a) Travada por uma tecla de desativar: borda de subida destrava a trava
    //     e LIGA o replay imediatamente (não exige clique extra em atirar).
    //  b) Sem trava e com "1" configurada como tecla de disparo (perfil antigo):
    //     borda de subida dispara a rajada pelo mesmo caminho do WM_HOTKEY
    //     antigo (toggle/hold/restartFromZero), preservando o comportamento.
    if (state_.repetition.enabled) {
        const bool enDown = (GetAsyncKeyState(kDefaultEnableKeyVk) & 0x8000) != 0;
        const bool enableIsFireKey = state_.repetition.repeatKeyVk == kDefaultEnableKeyVk;
        if (enDown && !enableKeyDownPrev_) {
            enableKeyDownPrev_ = true;  // anti-retrigger até a tecla ser solta
            if (repDisabledByStopKey_) {
                // Toque em "1": destrava e RELIGA imediatamente.
                repDisabledByStopKey_ = false;
                if (state_.session.Running()) {
                    state_.session.Stop(nowMs);
                }
                repeater_.Stop(nowMs, /*returnBack=*/false);
                repeater_.Configure(state_.weapon, state_.patterns.Get(state_.weapon),
                                    state_.sensitivity, state_.repetition.substeps);
                repeater_.Start(nowMs);
                state_.repeatToggledOn = true;
                state_.repeatArmed = false;  // rearma ao soltar / próximo toque
            } else if (enableIsFireKey) {
                // "1" é a própria tecla de disparo do replay (perfil antigo que
                // usava KEY1 como fire key): age exatamente como o WM_HOTKEY de
                // kRepeatHotkeyId fazia — um toque limpo após soltar sempre
                // alcança este polling, pois segurar "1" no jogo gera apenas a
                // primeira borda de subida (sem auto-repeat aqui).
                if (state_.repetition.activation == RepeatActivation::Toggle) {
                    state_.repeatToggledOn = !state_.repeatToggledOn;
                    if (!state_.repeatToggledOn && repeater_.Running()) {
                        repeater_.Stop(nowMs, state_.repetition.returnBack);
                    }
                    if (state_.repeatToggledOn && !repeater_.Running()) {
                        if (state_.session.Running()) {
                            state_.session.Stop(nowMs);
                        }
                        repeater_.Configure(state_.weapon, state_.patterns.Get(state_.weapon),
                                            state_.sensitivity, state_.repetition.substeps);
                        repeater_.Start(nowMs);
                    }
                } else if (!repeater_.Running()) {
                    if (state_.session.Running()) {
                        state_.session.Stop(nowMs);
                    }
                    repeater_.Configure(state_.weapon, state_.patterns.Get(state_.weapon),
                                        state_.sensitivity, state_.repetition.substeps);
                    repeater_.Start(nowMs);
                } else if (state_.repetition.restartFromZero) {
                    // Clique novo com a rajada ainda rolando: recomeça do início.
                    repeater_.Stop(nowMs, /*returnBack=*/false);
                    if (state_.session.Running()) {
                        state_.session.Stop(nowMs);
                    }
                    repeater_.Configure(state_.weapon, state_.patterns.Get(state_.weapon),
                                        state_.sensitivity, state_.repetition.substeps);
                    repeater_.Start(nowMs);
                }
            }
        } else if (!enDown) {
            enableKeyDownPrev_ = false;  // tecla solta: pronta para o próximo toque
        }
    } else {
        // Repetição desligada nas configurações: acompanha o estado físico da
        // tecla para a detecção de borda não disparar sozinha ao reativar.
        enableKeyDownPrev_ = (GetAsyncKeyState(kDefaultEnableKeyVk) & 0x8000) != 0;
    }

    // Teclas de alternar armas por polling: cobrem botões do mouse (sem WM_HOTKEY).
    // Anti-retrigger: precisa soltar antes do próximo toque.
    if (state_.repetition.enabled && IsMouseButtonVk(state_.repetition.cycleWeaponKeyVk)) {
        const bool cycleDown = (GetAsyncKeyState(state_.repetition.cycleWeaponKeyVk) & 0x8000) != 0;
        if (cycleDown && !state_.cycleHotkeyDown) {
            CycleWeapon();
        }
        state_.cycleHotkeyDown = cycleDown;
    }
    if (state_.repetition.enabled && IsMouseButtonVk(state_.repetition.cycleBackKeyVk) &&
        state_.repetition.cycleBackKeyVk != state_.repetition.cycleWeaponKeyVk) {
        const bool backDown = (GetAsyncKeyState(state_.repetition.cycleBackKeyVk) & 0x8000) != 0;
        if (backDown && !state_.cycleBackHotkeyDown) {
            CycleWeaponBack();
        }
        state_.cycleBackHotkeyDown = backDown;
    }

    // Modo "enquanto pressionada": fim de rajada verificado ANTES de qualquer
    // novo start (ao soltar o botão, o mesmo frame não deve reiniciar a rajada).
    if (state_.repetition.activation == RepeatActivation::Hold && repeater_.Running() &&
        (GetAsyncKeyState(state_.repetition.repeatKeyVk) & 0x8000) == 0) {
        repeater_.Stop(nowMs, state_.repetition.returnBack);
        // Se o replay foi desligado por uma tecla de desativar durante a rajada,
        // soltar o botão NÃO rearma: só a tecla "1" religa. Sem isso, o clique
        // seguinte em atirar disparava o macro por cima da desativação.
        state_.repeatArmed = !repDisabledByStopKey_;
    }

    // Tecla de repetir: quando é botão do mouse, não existe WM_HOTKEY — a detecção
    // é por polling, com "arm" para o botão precisar ser solto antes de um novo
    // disparo (mesma lógica anti-retrigger do código de referência). Em modo
    // alternar (toggle), cada toque liga/desliga a repetição contínua.
    if (IsMouseButtonVk(state_.repetition.repeatKeyVk)) {
        const bool btnDown = IsRepeatButtonDown();
        // Travado por tecla de desativar: o botão só rearma o "armed" ao ser solto;
        // nenhum clique religa o replay (só a tecla "1" via WM_HOTKEY).
        const bool blocked = repDisabledByStopKey_;
        if (btnDown && state_.repeatArmed && state_.repetition.enabled && !blocked) {
            state_.repeatArmed = false;
            if (state_.repetition.activation == RepeatActivation::Toggle) {
                state_.repeatToggledOn = !state_.repeatToggledOn;
                if (!state_.repeatToggledOn && repeater_.Running()) {
                    repeater_.Stop(nowMs, state_.repetition.returnBack);
                }
                if (state_.repeatToggledOn && !repeater_.Running()) {
                    if (state_.session.Running()) {
                        state_.session.Stop(nowMs);
                    }
                    repeater_.Configure(state_.weapon, state_.patterns.Get(state_.weapon), state_.sensitivity,
                                        state_.repetition.substeps);
                    repeater_.Start(nowMs);
                }
            } else if (!repeater_.Running()) {
                if (state_.session.Running()) {
                    state_.session.Stop(nowMs);
                }
                repeater_.Configure(state_.weapon, state_.patterns.Get(state_.weapon), state_.sensitivity,
                                    state_.repetition.substeps);
                repeater_.Start(nowMs);
            } else if (state_.repetition.restartFromZero) {
                // Clique novo com a rajada ainda rolando: recomeça do início do padrão.
                // (Sem retorno à origem — o próximo Start já zera os contadores.)
                repeater_.Stop(nowMs, /*returnBack=*/false);
                if (state_.session.Running()) {
                    state_.session.Stop(nowMs);
                }
                repeater_.Configure(state_.weapon, state_.patterns.Get(state_.weapon), state_.sensitivity,
                                    state_.repetition.substeps);
                repeater_.Start(nowMs);
            }
        } else if (!btnDown) {
            // Botão solto: rearma, MAS permanece travado se uma tecla de
            // desativar estiver segurada — ao soltá-la o próximo frame rearma.
            bool stopHeld = false;
            for (int vk : state_.repetition.stopKeysVk) {
                if (vk >= 0 && vk < 256 && (GetAsyncKeyState(vk) & 0x8000) != 0) {
                    stopHeld = true;
                    break;
                }
            }
            state_.repeatArmed = !repDisabledByStopKey_ && !stopHeld;
        }
    }

    // Repetição contínua do modo alternar: quando um ciclo termina, recomeça
    // sozinho até uma nova tecla de desativar/toque desligar.
    if (state_.repeatToggledOn && state_.repetition.enabled &&
        state_.repetition.activation == RepeatActivation::Toggle && !repeater_.Running()) {
        repeater_.Configure(state_.weapon, state_.patterns.Get(state_.weapon), state_.sensitivity,
                            state_.repetition.substeps);
        repeater_.Start(nowMs);
    }

    if (!repeater_.Running()) {
        state_.repetitionRunning = false;
        return;
    }

    // Os micro-movimentos programados chegam ao jogo via SendInput. Enquanto o app
    // está em foco, o movimento enviado volta como WM_INPUT e pode afetar uma rajada
    // manual — por isso a repetição também cancela o treino antes de começar.
    for (const MouseSample& move : repeater_.Tick(nowMs)) {
        SendMouseDelta(move.dx, move.dy);
    }

    // Estado para o overlay ("REPLAY ATIVO" + progresso dos tiros).
    state_.repetitionRunning = true;
    state_.repetitionShot = static_cast<int>(repeater_.ShotsDone());
    state_.repetitionShotsTotal = static_cast<int>(repeater_.ShotsTotal());
}

void App::ApplyWindowRequests() {
    if (state_.requestMinimize) {
        ShowWindow(hwnd_, SW_MINIMIZE);
    }
    if (state_.requestToggleMaximize) {
        ShowWindow(hwnd_, IsZoomed(hwnd_) ? SW_RESTORE : SW_MAXIMIZE);
    }
    if (state_.requestClose) {
        PostMessageW(hwnd_, WM_CLOSE, 0, 0);
    }
    if (state_.requestShowMain) {
        ShowWindow(hwnd_, IsIconic(hwnd_) ? SW_RESTORE : SW_SHOW);
        SetForegroundWindow(hwnd_);
    }
    state_.requestMinimize = state_.requestToggleMaximize = state_.requestClose = false;
    state_.requestShowMain = false;
}

void App::ApplyUiScale() {
    const float target = std::clamp(state_.ui.scale, 0.8f, 1.5f) * dpiScale_;
    if (std::abs(target - uiScale_) < 0.001f) {
        return;
    }
    // Fora do frame: recria o atlas de fontes no tamanho final e a textura dele.
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    theme::LoadFonts(target);
    io.FontGlobalScale = 1.0f / target;
    ImGui_ImplDX11_InvalidateDeviceObjects();
    uiScale_ = target;
}

// O ImGui trabalha em unidades lógicas: converte as coordenadas de mouse que o backend lê.
LPARAM App::ScaleMouseLParam(UINT msg, LPARAM lParam) const {
    if (uiScale_ <= 0.0f || uiScale_ == 1.0f) {
        return lParam;
    }
    if (msg == WM_MOUSEMOVE) {
        const int x = static_cast<int>(static_cast<float>(GET_X_LPARAM(lParam)) / uiScale_);
        const int y = static_cast<int>(static_cast<float>(GET_Y_LPARAM(lParam)) / uiScale_);
        return MAKELPARAM(x, y);
    }
    if (msg == WM_NCMOUSEMOVE) {  // coordenadas de tela: cliente lógico -> tela
        POINT p{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hwnd_, &p);
        p.x = static_cast<LONG>(static_cast<float>(p.x) / uiScale_);
        p.y = static_cast<LONG>(static_cast<float>(p.y) / uiScale_);
        ClientToScreen(hwnd_, &p);
        return MAKELPARAM(p.x, p.y);
    }
    return lParam;
}

// O backend do ImGui às vezes lê a posição do cursor direto (GetCursorPos) em pixels físicos;
// esta posição, enviada depois, prevalece no frame.
void App::FixMousePosition() {
    if (uiScale_ == 1.0f || GetForegroundWindow() != hwnd_) {
        return;
    }
    POINT p{};
    GetCursorPos(&p);
    ScreenToClient(hwnd_, &p);
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    ImGuiIO& io = ImGui::GetIO();
    if (PtInRect(&rc, p) != FALSE || GetCapture() == hwnd_) {
        io.AddMousePosEvent(static_cast<float>(p.x) / uiScale_, static_cast<float>(p.y) / uiScale_);
    } else {
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    }
}

// Diálogos de arquivo e "abrir pasta" rodam fora do frame do ImGui (são modais e bloqueiam).
void App::HandleProfileRequests() {
    ProfileUiState& p = state_.profiles;
    if (p.store == nullptr) {
        return;
    }
    const FileAction action = p.pendingFile;
    p.pendingFile = FileAction::None;
    if (action == FileAction::Import) {
        if (const auto file = OpenJsonDialog(hwnd_)) {
            profiles::ImportFrom(state_, *file);
        }
    } else if (action == FileAction::Export) {
        const std::string base = p.current.empty() ? std::string("perfil") : p.current;
        if (const auto file = SaveJsonDialog(hwnd_, ProfileStore::PathFromUtf8(base + ".json").wstring())) {
            profiles::ExportTo(state_, *file);
        }
    }
    if (p.requestOpenFolder) {
        p.requestOpenFolder = false;
        p.store->EnsureDirs();
        ShellExecuteW(hwnd_, L"open", p.store->ProfilesDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}

void App::ApplyWindowOpacity() {
    const float opacity = std::clamp(state_.ui.windowOpacity, 0.6f, 1.0f);
    if (opacity == appliedWindowOpacity_) {
        return;
    }
    // Só vira janela "layered" quando há transparência; com 100% fica uma janela comum.
    LONG_PTR ex = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
    if (opacity < 1.0f) {
        if ((ex & WS_EX_LAYERED) == 0) {
            SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, ex | WS_EX_LAYERED);
        }
        SetLayeredWindowAttributes(hwnd_, 0, static_cast<BYTE>(std::lround(opacity * 255.0f)), LWA_ALPHA);
    } else if ((ex & WS_EX_LAYERED) != 0) {
        SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, ex & ~static_cast<LONG_PTR>(WS_EX_LAYERED));
    }
    appliedWindowOpacity_ = opacity;
}

// Durante uma rajada o cursor fica preso à janela, para o foco não escapar.
void App::UpdateCursorClip() {
    const bool want = state_.captureMouse && GetForegroundWindow() == hwnd_;
    if (want && !cursorClipped_) {
        RECT rc{};
        GetClientRect(hwnd_, &rc);
        MapWindowPoints(hwnd_, nullptr, reinterpret_cast<POINT*>(&rc), 2);
        ClipCursor(&rc);
        cursorClipped_ = true;
    } else if (!want && cursorClipped_) {
        ClipCursor(nullptr);
        cursorClipped_ = false;
    }
}

LRESULT App::HitTest(LPARAM lParam) const {
    POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    ScreenToClient(hwnd_, &pt);
    RECT rc{};
    GetClientRect(hwnd_, &rc);

    if (!IsZoomed(hwnd_)) {
        const LONG border = static_cast<LONG>(static_cast<float>(kResizeBorder) * dpiScale_);
        const bool left = pt.x < border;
        const bool right = pt.x >= rc.right - border;
        const bool top = pt.y < border;
        const bool bottom = pt.y >= rc.bottom - border;
        if (top && left) return HTTOPLEFT;
        if (top && right) return HTTOPRIGHT;
        if (bottom && left) return HTBOTTOMLEFT;
        if (bottom && right) return HTBOTTOMRIGHT;
        if (left) return HTLEFT;
        if (right) return HTRIGHT;
        if (top) return HTTOP;
        if (bottom) return HTBOTTOM;
    }

    const float s = uiScale_ > 0.0f ? uiScale_ : 1.0f;
    const bool inTitle = pt.y < static_cast<LONG>(theme::kTitleBarHeight * s);
    const bool onButtons = pt.x >= rc.right - static_cast<LONG>(kTitleButtonsWidth * s);
    if (inTitle && !onButtons && !state_.captureMouse) {
        return HTCAPTION;
    }
    return HTCLIENT;
}

LRESULT CALLBACK App::WndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    App* self = nullptr;
    if (msg == WM_NCCREATE) {
        const auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<App*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self == nullptr) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return self->WndProc(hwnd, msg, wParam, lParam);
}

LRESULT App::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (imgui_ != nullptr) {
        ImGuiContext* prev = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imgui_);
        const LRESULT handled = ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, ScaleMouseLParam(msg, lParam));
        ImGui::SetCurrentContext(prev);
        if (handled) {
            return TRUE;
        }
    }

    switch (msg) {
        case WM_NCCALCSIZE:
            if (wParam == TRUE) {
                // Remove a moldura padrão; maximizado, desconta a borda que fica fora da tela.
                if (IsZoomed(hwnd)) {
                    auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
                    const int fx = GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                    const int fy = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                    params->rgrc[0].left += fx;
                    params->rgrc[0].right -= fx;
                    params->rgrc[0].top += fy;
                    params->rgrc[0].bottom -= fy;
                }
                return 0;
            }
            break;
        case WM_NCHITTEST:
            return HitTest(lParam);
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
            const float s = uiScale_ > 0.0f ? uiScale_ : dpiScale_;
            info->ptMinTrackSize.x = static_cast<LONG>(static_cast<float>(kMinWidth) * s);
            info->ptMinTrackSize.y = static_cast<LONG>(static_cast<float>(kMinHeight) * s);
            return 0;
        }
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED) {
                resizeWidth_ = LOWORD(lParam);
                resizeHeight_ = HIWORD(lParam);
            }
            return 0;
        case WM_DPICHANGED: {
            // Janela arrastada para outro monitor: nova escala e o tamanho sugerido pelo Windows.
            dpiScale_ = static_cast<float>(HIWORD(wParam)) / 96.0f;
            const auto* r = reinterpret_cast<const RECT*>(lParam);
            SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_INPUT:
            ReadRawMouse(lParam, mouse_, NowMs());
            break;  // DefWindowProc libera o buffer do WM_INPUT
        case WM_HOTKEY: {
            // Teclas globais da repetição (ver SyncRepetitionKeys). wParam é o id
            // do registro; lParam traz o VK + scan code (MOD_NOREPEAT garante um
            // evento por pressionar, sem auto-repeat do teclado).
            const int vk = static_cast<int>(lParam & 0xFFFF);
            if (wParam == kEnableHotkeyId && vk == kDefaultEnableKeyVk) {
                // Compatibilidade: este id não é mais registrado como hotkey da
                // tecla "1" desde a correção do bug das key2/key3 estendida à
                // tecla de ativar — "1" não pode engolir o KEYDOWN no jogo (o
                // jogador precisa selecionar a arma 1 em qualquer estado). O
                // caminho PRINCIPAL de "1" é o polling de borda de subida em
                // TickRepetition; este ramo só age se algum ambiente ainda
                // entregar o WM_HOTKEY com esse par id/VK. A comparação dupla
                // (id + VK) garante que um VK diferente chegando por acidente
                // com este id NÃO seja tratado aqui (cai na verificação de
                // desativar abaixo, como sempre).
                // Toque na tecla "1" (arma principal no jogo): SEMPRE tem
                // prioridade sobre qualquer outra interpretação (disparo ou
                // desativar) — limpa a trava deixada por uma tecla de desativar
                // e RELIGA o replay imediatamente, começando do zero quando
                // "recomeçar do início" estiver ligado.
                if (!state_.repetition.enabled) {
                    break;
                }
                const bool wasBlocked = repDisabledByStopKey_;
                repDisabledByStopKey_ = false;
                enableKeyDownPrev_ = true;  // anti-retrigger até a tecla ser solta
                if (wasBlocked) {
                    // Estava travada por uma tecla de desativar: "1" destrava E já
                    // liga o replay na hora (não exige um clique extra em atirar).
                    if (state_.session.Running()) {
                        state_.session.Stop(NowMs());
                    }
                    repeater_.Stop(NowMs(), /*returnBack=*/false);
                    repeater_.Configure(state_.weapon, state_.patterns.Get(state_.weapon),
                                        state_.sensitivity, state_.repetition.substeps);
                    repeater_.Start(NowMs());
                    state_.repeatToggledOn = true;
                    state_.repeatArmed = false;  // rearma ao soltar (Hold) / próximo toque (Toggle)
                    break;
                }
                if (state_.repetition.activation == RepeatActivation::Toggle) {
                    // Toque liga/desliga a repetição contínua.
                    state_.repeatToggledOn = !state_.repeatToggledOn;
                    if (!state_.repeatToggledOn) {
                        if (repeater_.Running()) {
                            repeater_.Stop(NowMs(), state_.repetition.returnBack);
                        }
                    } else if (!repeater_.Running()) {
                        // Sem "repeatArmed = false" aqui: o WM_HOTKEY já é anti-
                        // retrigger (MOD_NOREPEAT) e, em modo alternar, o próximo
                        // toque deve DESLIGAR mesmo se a tecla ainda estiver
                        // fisicamente pressionada (um clique basta nos dois sentidos).
                        if (state_.session.Running()) {
                            state_.session.Stop(NowMs());
                        }
                        repeater_.Configure(state_.weapon, state_.patterns.Get(state_.weapon),
                                            state_.sensitivity, state_.repetition.substeps);
                        repeater_.Start(NowMs());
                    }
                    break;
                }
                // Modo "enquanto pressionada": rajada começa aqui e termina no
                // polling de TickRepetition quando a tecla for solta.
                // Anti-retrigger do polling: sem esta linha, ao SOL TAR "1" o
                // Tick veria uma falsa borda de subida (prev=true -> down=false
                // não gera nada, mas o ramo else re-sincroniza prev; pior: se o
                // WM_HOTKEY religou sem travar e a tecla segue pressionada, o
                // próximo frame deve considerar "1" já segurada, não disparar de
                // novo). O handler antigo dependia de MOD_NOREPEAT; agora que "1"
                // também é vigiada por polling, marcamos o estado pressionado.
                state_.repeatArmed = false;  // só rearma quando a tecla for solta
                if (repeater_.Running()) {
                    if (!state_.repetition.restartFromZero) {
                        break;  // deixa a rajada atual continuar até o fim
                    }
                    // Clique novo com a rajada ainda rolando: recomeça do início do padrão.
                    repeater_.Stop(NowMs(), /*returnBack=*/false);
                }
                // A repetição assume o lugar de uma rajada manual em andamento.
                if (state_.session.Running()) {
                    state_.session.Stop(NowMs());
                }
                repeater_.Configure(state_.weapon, state_.patterns.Get(state_.weapon), state_.sensitivity,
                                    state_.repetition.substeps);
                repeater_.Start(NowMs());
            } else if (wParam == kCycleWeaponHotkeyId ||
                       (vk != 0 && vk == state_.repetition.cycleWeaponKeyVk &&
                        wParam == kCycleBackHotkeyId)) {
                if (state_.repetition.enabled) {
                    CycleWeapon();
                }
            } else if (wParam == kCycleBackHotkeyId) {
                if (state_.repetition.enabled) {
                    CycleWeaponBack();
                }
            } else if (vk != 0 && IsStopHotkeyId(static_cast<int>(wParam)) &&
                       std::find(state_.repetition.stopKeysVk.begin(), state_.repetition.stopKeysVk.end(), vk) !=
                           state_.repetition.stopKeysVk.end()) {
                // Tecla de desativar registrada como hotkey global (hoje só botões
                // do mouse de perfis antigos; teclas de teclado são detectadas por
                // polling em TickRepetition, sem engolir o toque no jogo). Interrompe
                // na hora (e cancela o retorno): marcado como pendente para o
                // TickRepetition processar — inclusive desligando o modo alternar.
                stopKeysPending_ = true;
            }
            break;
        }
        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE && cursorClipped_) {
                ClipCursor(nullptr);
                cursorClipped_ = false;
            }
            break;
        case WM_SYSCOMMAND:
            if ((wParam & 0xFFF0) == SC_KEYMENU) {
                return 0;
            }
            break;
        case WM_CLOSE:
            // Alterações não salvas: pergunta antes (o diálogo é desenhado pelo ImGui).
            if (!state_.allowClose && state_.profiles.store != nullptr && profiles::Dirty(state_)) {
                state_.confirmClose = true;
                if (IsIconic(hwnd)) {
                    ShowWindow(hwnd, SW_RESTORE);
                }
                SetForegroundWindow(hwnd);
                return 0;
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            hwnd_ = nullptr;
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool App::CreateDevice() {
    const D3D_FEATURE_LEVEL levels[2] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL got{};
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION,
                                   &device_, &got, &context_);
    if (FAILED(hr)) {
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &device_, &got,
                               &context_);
    }
    if (FAILED(hr)) {
        return false;
    }

    // Fábrica DXGI do próprio device: usada pelos swap chains da janela e do overlay.
    IDXGIDevice* dxgiDevice = nullptr;
    IDXGIAdapter* adapter = nullptr;
    bool ok = SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&dxgiDevice))) &&
              SUCCEEDED(dxgiDevice->GetAdapter(&adapter)) &&
              SUCCEEDED(adapter->GetParent(IID_PPV_ARGS(&factory_)));
    if (adapter != nullptr) {
        adapter->Release();
    }
    if (dxgiDevice != nullptr) {
        dxgiDevice->Release();
    }
    return ok && mainTarget_.Create(device_, factory_, hwnd_);
}

void App::CleanupDevice() {
    mainTarget_.Release();
    if (factory_ != nullptr) {
        factory_->Release();
        factory_ = nullptr;
    }
    if (context_ != nullptr) {
        context_->Release();
        context_ = nullptr;
    }
    if (device_ != nullptr) {
        device_->Release();
        device_ = nullptr;
    }
}

}  // namespace st
