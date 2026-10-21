#pragma once

#include "core/vision/matcher.h"

#include <string>
#include <vector>

namespace st {

// Máquina de estados do reconhecimento automático: recebe o resultado de cada
// verificação (as confianças por arma) e decide quando MUDAR a arma do replay.
// Regras (requisito 6): só executa em transição confirmada; nada de repetição
// enquanto a mesma imagem persiste; ambiguidade e confiança baixa não acionam;
// exige N confirmações consecutivas antes de trocar; volta para "esperando"
// quando nenhuma imagem bate. Pura: o tempo vem como parâmetro, sem threads.
class AutoPicker {
public:
    struct Params {
        float minConfidence = 0.85f;   // abaixo disso: rejeitado ("confiança baixa")
        float ambiguityMargin = 0.03f; // diferença mínima entre 1º e 2º colocados
        int confirmations = 3;         // leituras consecutivas da mesma arma p/ confirmar
        double minIntervalMs = 400.0;  // espera mínima depois de uma troca (anti-oscilação)
    };

    enum class Status {
        Waiting,     // nenhuma imagem reconhecida (aguardando)
        Pending,     // candidato visto, ainda sem confirmações suficientes
        Identified,  // arma confirmada (executada nesta transição)
        Ambiguous,   // duas armas com confiança parecida acima do mínimo
        LowScore,    // melhor batida abaixo do mínimo de confiança
    };

    struct Result {
        bool switched = false;          // true exatamente quando o App deve trocar a arma
        int weaponIndex = -1;           // arma ativa agora (-1 = esperando)
        Status status = Status::Waiting;
        float confidence = 0.0f;        // confiança da arma ativa/candidata
        std::string reason;             // motivo legível da última decisão/rejeição
    };

    AutoPicker() = default;
    explicit AutoPicker(const Params& params) : params_(params) {}

    void SetParams(const Params& params) { params_ = params; }
    const Params& GetParams() const { return params_; }

    // Uma verificação: `scores[i]` é a confiança da arma de índice i (0 se sem template).
    // `nowMs` é o relógio do app. Devolve a decisão; `switched` dispara o replay.
    Result Observe(double nowMs, const std::vector<float>& scores);

    // Desliga o modo automático: esquece tudo (a próxima leitura recomeça do zero).
    void Reset();

    Status GetStatus() const { return status_; }
    int CurrentWeapon() const { return current_; }
    float CurrentConfidence() const { return currentConfidence_; }

private:
    Params params_;
    Status status_ = Status::Waiting;
    int current_ = -1;                    // arma em uso pelo modo automático (-1 = nenhuma)
    float currentConfidence_ = 0.0f;
    int candidate_ = -1;                  // arma aguardando confirmação
    int candidateCount_ = 0;
    float candidateConfidence_ = 0.0f;
    double lastSwitchMs_ = -1.0e9;
};

}  // namespace st
