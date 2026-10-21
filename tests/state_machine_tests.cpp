// Testes portáveis da máquina de estados das teclas do replay (espelha App::TickRepetition).
// Valida: toque em tecla de desativar bloqueia reativação pelo mouse; só a tecla "1" religa.
#include <array>
#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool cond, const char* msg) {
    if (!cond) {
        ++g_failures;
        std::printf("FALHOU: %s\n", msg);
    }
}

// Modelo fiel da lógica de app.cpp: stopKeysPending_/repDisabledByStopKey_/repeatArmed_.
struct Machine {
    bool repeatKeyIsMouse = true;
    bool repDisabledByStopKey = false;
    bool repeatArmed = true;
    bool running = false;

    // TickRepetition: WM_HOTKEY de tecla de desativar chegou.
    void OnStopHotkey() {
        running = false;
        repDisabledByStopKey = true;
        repeatArmed = !repeatKeyIsMouse;
    }

    // TickRepetition: botão do mouse pressionado (com armed).
    void OnFireClick() {
        if (repeatKeyIsMouse && repeatArmed && !repDisabledByStopKey) {
            repeatArmed = false;
            running = true;  // replay liga com o clique de atirar
        }
    }
    void OnFireRelease() {
        if (repeatKeyIsMouse) {
            repeatArmed = true;
        }
    }

    // WM_HOTKEY da tecla de ativar ("1").
    void OnActivateKey() {
        repDisabledByStopKey = false;
        if (!running) {
            running = true;
        }
    }
};

void TestStopKeyBlocksMouseRestart() {
    Machine m;
    m.OnFireClick();
    Check(m.running, "clique no fogo liga o replay");
    m.OnStopHotkey();
    Check(!m.running, "toque na tecla de desativar para o replay na hora");
    // O bug relatado: soltar e clicar de novo no fogo NÃO pode religar.
    m.OnFireRelease();
    m.OnFireClick();
    Check(!m.running, "apos desativar por tecla, clique no fogo NAO religa o replay");
    m.OnFireRelease();
    m.OnFireClick();
    Check(!m.running, "varios cliques seguidos continuam sem religar");
    // Só a tecla "1" religa.
    m.OnActivateKey();
    Check(m.running, "tecla 1 religa o replay");
    // E depois disso o clique no fogo volta a funcionar normalmente.
    m.running = false;
    m.OnFireClick();
    Check(m.running, "depois da tecla 1, o clique no fogo volta a ligar");
}

void TestDeactivateStillWorksViaToggleKey() {
    Machine m;
    m.OnActivateKey();
    Check(m.running, "tecla 1 liga");
    m.OnStopHotkey();
    Check(!m.running, "tecla de desativar desliga mesmo com replay ligado pela tecla 1");
    Check(m.repeatArmed == false, "com tecla de ativar sendo mouse, fica desarmado ate soltar");
}

}  // namespace

int main() {
    TestStopKeyBlocksMouseRestart();
    TestDeactivateStillWorksViaToggleKey();
    if (g_failures != 0) {
        std::printf("%d falha(s)\n", g_failures);
        return 1;
    }
    std::printf("OK: maquina de estados das teclas do replay\n");
    return 0;
}
