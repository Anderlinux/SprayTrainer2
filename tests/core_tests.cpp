// Testes do núcleo portável (sem Windows). Rodam com ctest.
#include "core/pattern_recording.h"
#include "core/repetition.h"
#include "core/pattern_store.h"
#include "core/spsc_ring.h"
#include "core/training_session.h"

#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

namespace {

int g_failures = 0;

void Check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FALHOU: %s\n", what);
        ++g_failures;
    }
}

void TestDefaultPatterns() {
    for (std::size_t i = 0; i < st::kWeaponCount; ++i) {
        const auto w = static_cast<st::Weapon>(i);
        const st::Pattern p = st::DefaultPattern(w);
        Check(p[0].x == 0.0f && p[0].y == 0.0f, "primeiro tiro sem recuo");
        Check(p[1].y > 0.0f, "padrão sobe no início");
        Check(st::GetWeaponInfo(w).magazine <= static_cast<int>(st::kPatternPoints), "pente cabe no padrão");
    }
}

void TestPatternStore() {
    st::PatternStore store;
    const auto v0 = store.Version();
    store.SetPoint(st::Weapon::M4A4, 5, {1.0f, 2.0f});
    Check(store.Get(st::Weapon::M4A4)[5].y == 2.0f, "SetPoint grava valor");
    Check(store.Version() != v0, "versão muda após edição");
    store.ResetToDefault(st::Weapon::M4A4);
    Check(store.Get(st::Weapon::M4A4)[5].y == st::DefaultPattern(st::Weapon::M4A4)[5].y, "restaurar padrão");
    store.SetPoint(st::Weapon::AK47, 99, {1.0f, 1.0f});  // fora do intervalo: ignorado
}

void TestRing() {
    st::SpscRing<int, 4> ring;
    Check(ring.Push(1) && ring.Push(2) && ring.Push(3), "push até a capacidade");
    Check(!ring.Push(4), "fila cheia descarta");
    int v = 0;
    Check(ring.Pop(v) && v == 1, "ordem FIFO");
}

// Simula um jogador perfeito: antes de cada tiro, move exatamente o que falta.
void TestPerfectPlayer() {
    const st::Weapon w = st::Weapon::AK47;
    const float sens = 1.25f;
    const float degPerCount = sens * st::TrainingSession::kDegreesPerCount;
    const double interval = static_cast<double>(st::GetWeaponInfo(w).fireIntervalMs);

    st::TrainingSession s;
    s.Start(w, st::DefaultPattern(w), sens, 0.0);
    long long sentX = 0;
    long long sentY = 0;
    for (std::size_t i = 0; i < s.ShotsTotal(); ++i) {
        const st::Vec2 target = s.IdealPath()[i];
        const long long wantX = std::llround(target.x / degPerCount);
        const long long wantY = std::llround(target.y / degPerCount);
        s.AddMovement(static_cast<int>(wantX - sentX), static_cast<int>(wantY - sentY));
        sentX = wantX;
        sentY = wantY;
        s.Tick(static_cast<double>(i) * interval);
    }
    Check(s.GetState() == st::TrainingSession::State::Done, "sessão termina após o pente");
    const st::SessionResult r = s.Result();
    Check(r.shots.size() == 30, "AK registra 30 tiros");
    Check(r.score > 95.0f, "jogador perfeito tira nota alta");
}

void TestIdlePlayer() {
    st::TrainingSession s;
    s.Start(st::Weapon::AK47, st::DefaultPattern(st::Weapon::AK47), 1.0f, 1000.0);
    s.Tick(999.0);
    Check(s.ShotsFired() == 0, "nada antes do início");
    s.Stop(1000.0 + 2950.0);
    const st::SessionResult r = s.Result();
    Check(r.shots.size() == 30, "todos os tiros vencidos são registrados no Stop");
    Check(r.score < 20.0f, "sem compensar a nota é baixa");
}

void TestEarlyStop() {
    st::TrainingSession s;
    s.Start(st::Weapon::M4A1S, st::DefaultPattern(st::Weapon::M4A1S), 1.0f, 0.0);
    s.Stop(450.0);
    Check(s.Result().shots.size() == 5, "soltar cedo registra só os tiros disparados");
    s.AddMovement(10, 10);
    Check(s.LivePlayer().x == 0.0f, "movimento após o fim é ignorado");
}

void TestIdealPathAndTolerance() {
    st::Pattern p{};
    p[1] = {1.0f, 2.0f};
    p[2] = {-0.5f, 1.0f};
    const auto path = st::IdealPath(p, 3);
    Check(path.size() == 3, "IdealPath respeita a contagem");
    Check(path[2].x == -0.5f && path[2].y == 3.0f, "IdealPath acumula e inverte X");
    Check(st::IdealPath(p, 99).size() == st::kPatternPoints, "IdealPath limita a 31 pontos");

    // Mesmo erro, régua mais folgada -> nota maior.
    st::TrainingSession strict;
    st::TrainingSession loose;
    strict.Start(st::Weapon::AK47, st::DefaultPattern(st::Weapon::AK47), 1.0f, 0.0, 2.0f);
    loose.Start(st::Weapon::AK47, st::DefaultPattern(st::Weapon::AK47), 1.0f, 0.0, 8.0f);
    strict.Stop(1000.0);
    loose.Stop(1000.0);
    Check(loose.Result().score > strict.Result().score, "tolerância maior dá nota maior");
}

bool Near(float a, float b) {
    return std::fabs(a - b) < 1e-4f;
}

void TestPatternFromPath() {
    // Desenhar exatamente o caminho ideal devolve o próprio padrão.
    const st::Pattern p = st::DefaultPattern(st::Weapon::AK47);
    const st::Pattern back = st::PatternFromPath(st::IdealPath(p, st::kPatternPoints), st::Pattern{}, 0);
    bool same = true;
    for (std::size_t i = 0; i < st::kPatternPoints; ++i) {
        same = same && Near(back[i].x, p[i].x) && Near(back[i].y, p[i].y);
    }
    Check(same, "gravar o caminho ideal reconstrói o padrão");

    // Rajada curta: o resto vem do padrão base.
    const st::Pattern base = st::DefaultPattern(st::Weapon::M4A4);
    const std::vector<st::Vec2> shortPath = {{0.0f, 0.0f}, {-0.5f, 1.0f}, {-0.5f, 2.5f}};
    const st::Pattern partial = st::PatternFromPath(shortPath, base, 0);
    Check(Near(partial[1].x, 0.5f) && Near(partial[1].y, 1.0f), "mouse para a esquerda vira kick para a direita");
    Check(Near(partial[2].x, 0.0f) && Near(partial[2].y, 1.5f), "pontos viram deltas entre tiros");
    Check(partial[10].y == base[10].y, "pontos não gravados mantêm o padrão base");

    // Suavização reduz o tremido (diferença entre deltas vizinhos).
    std::vector<st::Vec2> shaky;
    for (int i = 0; i < 30; ++i) {
        const float jitter = (i % 2 == 0) ? 0.3f : -0.3f;
        shaky.push_back({jitter, 0.5f * static_cast<float>(i)});
    }
    const auto roughness = [](const st::Pattern& q) {
        float sum = 0.0f;
        for (std::size_t i = 2; i < 30; ++i) {
            sum += std::fabs(q[i].x - q[i - 1].x);
        }
        return sum;
    };
    Check(roughness(st::PatternFromPath(shaky, st::Pattern{}, 2)) < roughness(st::PatternFromPath(shaky, st::Pattern{}, 0)),
          "suavização reduz o tremido");
    Check(st::PatternFromPath({}, base, 1)[5].y == base[5].y, "caminho vazio devolve o base");
    Check(!std::signbit(st::PatternFromPath(shortPath, base, 0)[0].x), "sem zero negativo");

    std::vector<st::Vec2> wild = {{0.0f, 0.0f}, {-50.0f, 50.0f}};
    Check(st::PatternFromPath(wild, base, 0)[1].x == 10.0f, "valores limitados a 10°");
}

}  // namespace


void TestRepeaterMatchesIdeal() {
    const st::Weapon w = st::Weapon::AK47;
    const float sens = 1.25f;

    st::PatternRepeater rep;
    // O teste alimenta o repetidor em passos de 1 ms (sem frames reais de UI),
    // então desliga a compensação de atraso para obter o comportamento puro.
    rep.SetUiFrameMs(0.0);
    rep.Configure(w, st::DefaultPattern(w), sens, /*substeps=*/2);
    rep.Start(0.0);

    // Alimenta o repetidor em passos de 1 ms e soma os deltas enviados.
    long long sumX = 0;
    long long sumY = 0;
    std::vector<std::pair<long long, long long>> positions;  // acumulados a cada fim de tiro
    for (double t = 0.0; t < 6000.0 && rep.Running(); t += 1.0) {
        for (const st::MouseSample& m : rep.Tick(t)) {
            sumX += m.dx;
            sumY += m.dy;
        }
        if (positions.empty() || positions.size() < rep.ShotsDone()) {
            positions.push_back({sumX, sumY});
        }
    }
    Check(!rep.Running(), "repetição termina sozinha após o pente");
    Check(positions.size() >= 25, "repetiu quase todos os tiros da AK");

    // O caminho percorrido deve bater com o IdealPath do treino. A tolerância é
    // acumulada: cada tiro pode perder até meio count em cada eixo ao arredondar os
    // deltas inteiros enviados (o retorno final ainda devolve exatamente o total).
    const std::vector<st::Vec2> ideal = st::IdealPath(st::DefaultPattern(w), 30);
    const float degPerCount = sens * st::TrainingSession::kDegreesPerCount;
    bool close = true;
    for (std::size_t i = 0; i < positions.size() && i < ideal.size(); ++i) {
        const double tol = 0.5 * static_cast<double>(i + 1) + 1.0;
        const double dx = static_cast<double>(positions[i].first) * degPerCount - ideal[i].x;
        const double dy = static_cast<double>(positions[i].second) * degPerCount - ideal[i].y;
        if (std::fabs(dx) > tol * degPerCount) {
            close = false;
        }
        if (std::fabs(dy) > tol * degPerCount) {
            close = false;
        }
    }
    Check(close, "movimento repetido segue o padrão ideal (erro acumulado ≤ 0,5 count/tiro)");

    // Retorno: para no meio e devolve tudo (soma final = 0).
    st::PatternRepeater rep2;
    rep2.SetUiFrameMs(0.0);
    rep2.Configure(w, st::DefaultPattern(w), sens, 2);
    rep2.Start(0.0);
    long long x2 = 0;
    long long y2 = 0;
    double t = 0.0;
    for (; t < 1000.0; t += 1.0) {
        for (const st::MouseSample& m : rep2.Tick(t)) {
            x2 += m.dx;
            y2 += m.dy;
        }
    }
    Check(rep2.Running(), "ainda repetindo antes de parar");
    rep2.Stop(t, /*returnBack=*/true);
    Check(rep2.Running(), "retorno também roda depois do Stop");
    for (; t < 8000.0 && rep2.Running(); t += 1.0) {
        for (const st::MouseSample& m : rep2.Tick(t)) {
            x2 += m.dx;
            y2 += m.dy;
        }
    }
    Check(!rep2.Running(), "retorno terminou");
    Check(x2 == 0 && y2 == 0, "retorno devolve exatamente o total enviado");
}

void TestKeyNames() {
    // Nome de uma tecla conhecida e ida/volta pela tabela da UI.
    // Botão padrão de disparar o replay: LBUTTON (o clique de atirar). A tecla
    // fixa de religar é "1" (kDefaultEnableKeyVk), reservada à arma principal.
    if (std::string(st::VirtualKeyName(st::kDefaultRepeatKeyVk)) != "LBUTTON") {
        ++g_failures;
        std::printf("FALHOU: nome da tecla padrao deveria ser LBUTTON\n");
    }
    if (st::kDefaultEnableKeyVk != st::kVkDigit1) {
        ++g_failures;
        std::printf("FALHOU: a tecla fixa de religar deveria ser KEY1\n");
    }
    const int vk = st::VirtualKeyFromName("LBUTTON");
    Check(vk == st::kVkLButton, "nome LBUTTON resolve para VK_LBUTTON");
    Check(st::VirtualKeyFromName("lbutton") == st::kVkLButton, "busca ignora maiusculas");
    // Os 5 botões do mouse devem funcionar como tecla de repetir/desativar:
    // precisam resolver por nome (a UI monta a lista a partir dela).
    Check(st::VirtualKeyFromName("LBUTTON") == st::kVkLButton, "LBUTTON resolvido");
    Check(st::VirtualKeyFromName("RBUTTON") == st::kVkRButton, "RBUTTON resolvido");
    Check(st::VirtualKeyFromName("MBUTTON") == st::kVkMButton, "MBUTTON resolvido");
    Check(st::VirtualKeyFromName("X1BUTTON") == st::kVkXButton1, "X1BUTTON resolvido");
    Check(st::VirtualKeyFromName("X2BUTTON") == st::kVkXButton2, "X2BUTTON resolvido");
    Check(st::VirtualKeyFromName("NAOEXISTE") == 0, "tecla desconhecida vira 0");
    Check(st::VirtualKeyName(0) == "VK?", "VK invalido ganha nome legivel");
    // Toda opcao oferecida na UI precisa sobreviver ao ciclo nome -> VK -> nome.
    bool roundTrip = true;
    for (std::size_t i = 0; i < st::kKeyOptionCount; ++i) {
        const int resolved = st::VirtualKeyFromName(st::kKeyOptions[i].name);
        if (resolved != st::kKeyOptions[i].vk || st::VirtualKeyName(resolved) != st::kKeyOptions[i].name) {
            roundTrip = false;
        }
    }
    Check(roundTrip, "todas as teclas da lista fazem ida e volta pelo nome");
}

int main() {
    TestDefaultPatterns();
    TestPatternStore();
    TestRing();
    TestPerfectPlayer();
    TestIdlePlayer();
    TestEarlyStop();
    TestIdealPathAndTolerance();
    TestPatternFromPath();
    TestRepeaterMatchesIdeal();
    TestKeyNames();
    if (g_failures == 0) {
        std::printf("Todos os testes passaram.\n");
        return 0;
    }
    return 1;
}
