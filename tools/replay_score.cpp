// Ferramenta de diagnóstico (fora do build oficial): simula o laço do app em Linux —
// PatternReater gera micro-movimentos, "SendInput" devolve as amostras ao anel,
// DrainMouse alimenta a TrainingSession e medimos a nota final por arma.
#include "core/repetition.h"
#include "core/training_session.h"
#include "core/weapon.h"

#include <algorithm>
#include <cstdio>
#include <deque>
#include <string>
#include <vector>

using namespace st;

struct Sim {
    double frameMs = 15.625;
    int substeps = 3;
    float sensitivity = 1.35f;
    std::deque<MouseSample> ring;
};

static SessionResult RunOne(Sim& sim, Weapon w, const Pattern& pat, bool withReturn, double startDelay = 0.0) {
    TrainingSession session;
    PatternRepeater repeater;
    repeater.Configure(w, pat, sim.sensitivity, sim.substeps);
    // Compensação de dead-time: o repeater adianta a curva por T_ui/2, exatamente
    // como o App::Run faz ao medir a duração média do frame da UI.
    repeater.SetUiFrameMs(sim.frameMs);
    const WeaponInfo& info = GetWeaponInfo(w);
    const double burstMs = static_cast<double>(info.magazine) * info.fireIntervalMs;
    (void)burstMs;

    // Alinha o início da sessão com o start do repeater (como no app).
    // startDelay > 0 simula o atraso de um frame entre o Start() do repeater e o
    // primeiro Tick() dele (no app, Start é chamado no WndProc e o primeiro Tick
    // só acontece no próximo frame da UI).
    double now = -startDelay;
    session.Start(w, pat, sim.sensitivity, 0.0);
    repeater.Start(0.0);

    while (repeater.Running()) {
        now += sim.frameMs;
        // "SendInput": os deltas voltam como WM_INPUT no próximo drain.
        for (const MouseSample& m : repeater.Tick(now)) {
            sim.ring.push_back(m);
        }
        // DrainMouse: tick até a última amostra + tick com o relógio atual.
        MouseSample s;
        while (!sim.ring.empty()) {
            s = sim.ring.front();
            sim.ring.pop_front();
            if (session.Running() && s.timeMs >= session.StartMs()) {
                session.Tick(s.timeMs);
                session.AddMovement(s.dx, s.dy);
            }
        }
        session.Tick(now);
    }
    if (withReturn) {
        // retorno à origem: ainda dentro da janela da rajada para armas lentas
        while (!sim.ring.empty()) {
            const MouseSample s = sim.ring.front();
            sim.ring.pop_front();
            if (session.Running()) {
                session.Tick(s.timeMs);
                session.AddMovement(s.dx, s.dy);
            }
        }
    }
    session.Stop(now);
    return session.Result();
}

int main(int argc, char** argv) {
    const int from = argc > 1 ? std::atoi(argv[1]) : 0;
    const int to = argc > 2 ? std::atoi(argv[2]) : static_cast<int>(kWeaponCount) - 1;
    const double startDelay = argc > 3 ? std::atof(argv[3]) : 0.0;
    printf("%-8s %6s %4s %7s %7s   %s\n", "arma", "sub", "mag", "nota", "erro", "piores tiros (idx:erro)");
    for (int wi = from; wi <= to; ++wi) {
        const Weapon w = static_cast<Weapon>(wi);
        const WeaponInfo& info = GetWeaponInfo(w);
        const Pattern pat = DefaultPattern(w);
        for (int sub : {1, 2, 3, 5}) {
            Sim sim;
            sim.substeps = sub;
            SessionResult r = RunOne(sim, w, pat, false, startDelay);
            std::vector<std::pair<float, std::size_t>> errs;
            for (std::size_t i = 0; i < r.shots.size(); ++i)
                errs.push_back({r.shots[i].error, i});
            std::sort(errs.rbegin(), errs.rend());
            char worst[96] = "";
            int off = 0;
            for (int k = 0; k < 3 && k < static_cast<int>(errs.size()); ++k)
                off += std::snprintf(worst + off, sizeof(worst) - static_cast<std::size_t>(off), "%zu:%.2f ",
                                     errs[static_cast<std::size_t>(k)].second, errs[static_cast<std::size_t>(k)].first);
            std::printf("%-8s %6d %4d %7.1f %7.3f   %s\n", std::string(info.name).c_str(), sub, info.magazine,
                        static_cast<double>(r.score), static_cast<double>(r.meanError), worst);
        }
    }
    return 0;
}
