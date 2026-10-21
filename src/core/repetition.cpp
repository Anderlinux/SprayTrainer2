#include "core/repetition.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace st {

namespace {

// Arredonda para o inteiro mais próximo, com meio-arredondamento afastado de zero
// (igual ao std::round do código de referência).
int RoundCount(float value) {
    return static_cast<int>(std::lround(value));
}

}  // namespace

void PatternRepeater::Configure(Weapon weapon, const Pattern& pattern, float sensitivity, int substeps) {
    weapon_ = weapon;
    substeps_ = std::clamp(substeps, 1, kMaxSubsteps);
    degPerCount_ = std::max(sensitivity, 0.01f) * TrainingSession::kDegreesPerCount;
    intervalMs_ = static_cast<double>(GetWeaponInfo(weapon).fireIntervalMs);
    effSubsteps_ = substeps_;  // refinado pelo clamp de cadência em Start/BuildQueue
    const auto shots = static_cast<std::size_t>(GetWeaponInfo(weapon).magazine);
    path_ = IdealPath(pattern, shots);
    state_ = State::Idle;
    queue_.clear();
    cursor_ = 0;
    step_ = 0;
    sentX_ = 0;
    sentY_ = 0;
}

// Atualiza o frame médio da UI (medido em tempo real pelo App::Run). O clamp de cadência
// é refeito no Start() seguinte — aqui só guardamos o valor.
void PatternRepeater::SetUiFrameMs(double frameMs) {
    uiFrameMs_ = std::max(0.0, frameMs);
}

void PatternRepeater::Start(double nowMs) {
    if (state_ != State::Idle || path_.empty()) {
        return;
    }
    BuildQueue(nowMs);
    cursor_ = 0;
    step_ = 0;
    sentX_ = 0;
    sentY_ = 0;
    state_ = State::Running;
}

void PatternRepeater::Stop(double nowMs, bool returnBack) {
    if (state_ == State::Idle) {
        return;
    }
    if (returnBack && (sentX_ != 0 || sentY_ != 0)) {
        BuildReturn(nowMs);
        cursor_ = 0;
        state_ = State::Returning;
    } else {
        state_ = State::Idle;
        queue_.clear();
        cursor_ = 0;
    }
}

// Fila de micro-movimentos: cada passo i do caminho ideal (que o jogo dispara no
// instante start + i*intervalo) é dividido em `effSubsteps_` envios igualmente espaçados
// dentro do intervalo do tiro — mesma ideia do "smoothness" do código de referência,
// mas agendada pelo relógio do app em vez de sleep em thread própria.
//
// A fila guarda a POSIÇÃO ABSOLUTA desejada (em counts fracionários), não o delta já
// arredondado: Tick() envia sempre round(alvo_absoluto) − total_já_enviado. Assim nenhum
// resíduo de arredondamento se empilha ao longo da rajada e o desvio fica limitado a
// meio count (≈0,015° na sensibilidade padrão), independentemente do número de subpassos.
//
// COMPENSAÇÃO DE DEFASAGEM (dead-time). O movimento só conta para a nota quando a sessão
// processa a amostra; entre gerar (Tick) e medir (RecordShot) passam-se ~2 frames da UI
// (a amostra nasce no fim do frame N, volta via SendInput/WM_INPUT e só é drenada no
// frame N+1). Sem correção, um replay perfeito "na teoria" chega sempre atrasado e a
// nota fica limitada a ~38–76 mesmo com o fix de arredondamento (simulador
// tools/replay_score.cpp). Com uiFrameMs_ == 0 nada disso se aplica: comportamento
// nominal antigo, determinístico (usado pelos testes de núcleo).
//
// Estratégia em tempo de MEDIÇÃO (o horário atMs passa a significar "instante em que a
// posição deve estar registrada na sessão", não "instante de envio"):
//  1) Janela do tiro adiantada da latência ida-e-volta (2 frames): a curva ideal do
//     tiro i começa em nowMs + i*T − 2*T_ui, então o topo (fim do intervalo) coincide
//     exatamente com o instante de medição t_i = nowMs + i*T.
//  2) Alinhamento por frame: cada horário é ajustado para o último tick cujo ENVIO ainda
//     chega à sessão antes dele (entrega ≈ horário programado + T_ui). Interpolar pela
//     fase REAL dos frames (e não pela fração nominal s/substeps) elimina o erro
//     sistemático de defasagem que derrubava a AK-47 para ~40–84 nas tentativas
//     anteriores, inclusive quando vários subpassos vencem dentro do mesmo frame.
//  3) Fechamento garantido: o ÚLTIMO subpasso de cada tiro nunca pode ser entregue após
//     t_i — se o tick mais tardio possível cair depois da medição (fase adversária),
//     recua-se um frame e o alvo é interpolado nesse instante real.
//  4) Clamp de cadência: subdividir um intervalo mais curto que o frame da UI em muitos
//     micro-movimentos não melhora a suavidade (todos vencem juntos); limita-se a
//     ~1 micro-movimento por frame para manter a semântica "um envio por horário".
void PatternRepeater::BuildQueue(double nowMs) {
    queue_.clear();
    // Clamp de cadência (~1 micro-movimento por frame da UI) — ver regra 4 acima.
    if (uiFrameMs_ > 0.0) {
        effSubsteps_ = std::clamp(static_cast<int>(intervalMs_ / uiFrameMs_), 1, effSubsteps_);
    } else {
        effSubsteps_ = substeps_;
    }
    queue_.reserve(path_.size() * static_cast<std::size_t>(effSubsteps_));
    const double latencyMs = 2.0 * std::max(0.0, uiFrameMs_);  // ida (gerar) + volta (drenar)
    const double stepMs = intervalMs_ / static_cast<double>(effSubsteps_);
    for (std::size_t i = 0; i < path_.size(); ++i) {
        // Alvo acumulado deste tiro, em counts (x inverte o recuo; y compensa para baixo).
        const double targetX = static_cast<double>(path_[i].x) / degPerCount_;
        const double targetY = static_cast<double>(path_[i].y) / degPerCount_;
        const Vec2 prev = i > 0 ? path_[i - 1] : Vec2{};
        const double prevX = static_cast<double>(prev.x) / degPerCount_;
        const double prevY = static_cast<double>(prev.y) / degPerCount_;
        // Instante de medição do tiro i pela sessão (grade com origem em nowMs).
        const double due = static_cast<double>(i) * intervalMs_;
        const double shotStart = nowMs + due - latencyMs;  // regra 1
        for (int s = 0; s < effSubsteps_; ++s) {
            SubMove move;
            const bool last = (s + 1 == effSubsteps_);
            double desired = shotStart + static_cast<double>(s) * stepMs;
            double frac = (static_cast<double>(s) + 1.0) / static_cast<double>(effSubsteps_);
            if (uiFrameMs_ > 0.0) {
                // Regra 2: maior múltiplo de T_ui <= desired (com folga de ponto flutuante).
                double aligned = std::floor(desired / uiFrameMs_ + 1e-9) * uiFrameMs_;
                if (last && due > 0.0) {
                    // Regra 3: entrega precisa acontecer ANTES de t_i (frame de medição
                    // round_down(t_i) drena as amostras geradas até round_down(t_i) − T_ui).
                    const double latest = std::floor((due - latencyMs) / uiFrameMs_ + 1e-9) * uiFrameMs_;
                    aligned = std::min(desired, latest);
                }
                // Fase real apenas para subpassos INTERMEDIÁRIOS: quando vários vencem
                // dentro do mesmo frame (stepMs < T_ui), o alvo deles é a curva ideal no
                // instante em que chegam à sessão — senão ficariam sistematicamente
                // adiantados/atrasados (SG 553 caía para ~87 com substeps=3). O topo do
                // passo (frac=1) é preservado pela regra 3 e nunca deve ser recuado.
                if (!last) {
                    frac = std::clamp((aligned - shotStart + latencyMs) / stepMs / static_cast<double>(s + 1),
                                      0.0, 1.0);
                }
                desired = aligned;
            }
            move.atMs = desired;
            move.x = static_cast<float>(prevX + (targetX - prevX) * frac);  // absoluta, não delta
            move.y = static_cast<float>(prevY + (targetY - prevY) * frac);
            queue_.push_back(move);
        }
    }
}

// Devolve o cursor à posição inicial: mesmo número de passos usados até aqui,
// somando exatamente o total enviado (o resto do arredondamento vai no último passo).
void PatternRepeater::BuildReturn(double nowMs) {
    queue_.clear();
    const int steps = std::max(1, static_cast<int>(step_) * effSubsteps_);
    const double per = 6.0;  // ms entre micro-movimentos do retorno
    long long doneX = 0;
    long long doneY = 0;
    for (int s = 0; s < steps; ++s) {
        SubMove move;
        move.atMs = nowMs + static_cast<double>(s) * per;
        if (s + 1 == steps) {
            move.x = static_cast<float>(-sentX_ - doneX);
            move.y = static_cast<float>(-sentY_ - doneY);
        } else {
            move.x = static_cast<float>(-sentX_) / static_cast<float>(steps);
            move.y = static_cast<float>(-sentY_) / static_cast<float>(steps);
            doneX += RoundCount(move.x);
            doneY += RoundCount(move.y);
        }
        queue_.push_back(move);
    }
}

std::vector<MouseSample> PatternRepeater::Tick(double nowMs) {
    std::vector<MouseSample> out;
    if (state_ == State::Idle) {
        return out;
    }
    while (cursor_ < queue_.size() && nowMs >= queue_[cursor_].atMs) {
        const SubMove& move = queue_[cursor_];
        MouseSample sample;
        if (state_ == State::Running) {
            // queue_ guarda a posição absoluta desejada: o delta inteiro enviado fecha o
            // buraco até lá, absorvendo qualquer resíduo de arredondamento anterior.
            sample.dx = RoundCount(move.x) - static_cast<int>(sentX_);
            sample.dy = RoundCount(move.y) - static_cast<int>(sentY_);
        } else {
            // No retorno, queue_ guarda deltas (compensam exatamente o total enviado).
            sample.dx = RoundCount(move.x);
            sample.dy = RoundCount(move.y);
        }
        sample.timeMs = nowMs;
        out.push_back(sample);
        sentX_ += sample.dx;
        sentY_ += sample.dy;
        if (state_ == State::Running) {
            // Passo concluído => o tiro correspondente começou (aproximação por tempo).
            if ((cursor_ + 1U) % static_cast<std::size_t>(effSubsteps_) == 0U) {
                ++step_;
            }
        }
        ++cursor_;
    }

    if (cursor_ >= queue_.size()) {
        state_ = State::Idle;
        queue_.clear();
        cursor_ = 0;
        if (sentX_ == 0 && sentY_ == 0) {
            step_ = 0;  // retorno fechou o ciclo: recomeça do zero na próxima vez
        }
    }
    return out;
}

std::string_view VirtualKeyName(int vk) {
    for (const KeyOption& option : kKeyOptions) {
        if (option.vk == vk) {
            return option.name;
        }
    }
    return "VK?";  // desconhecida (não deveria acontecer: a UI só oferece kKeyOptions)
}

int VirtualKeyFromName(std::string_view name) {
    for (const KeyOption& option : kKeyOptions) {
        const std::string_view candidate = option.name;
        if (candidate.size() != name.size()) {
            continue;
        }
        bool same = true;
        for (std::size_t i = 0; i < candidate.size() && same; ++i) {
            same = std::toupper(static_cast<unsigned char>(candidate[i])) ==
                   std::toupper(static_cast<unsigned char>(name[i]));
        }
        if (same) {
            return option.vk;
        }
    }
    return 0;
}

}  // namespace st
