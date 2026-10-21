#include "core/vision/auto_picker.h"

#include <algorithm>

namespace st {

namespace {

const char* StatusName(AutoPicker::Status s) {
    switch (s) {
        case AutoPicker::Status::Waiting: return "aguardando imagem";
        case AutoPicker::Status::Pending: return "reconhecendo";
        case AutoPicker::Status::Identified: return "identificado";
        case AutoPicker::Status::Ambiguous: return "ambiguo";
        case AutoPicker::Status::LowScore: return "confianca baixa";
    }
    return "desconhecido";
}

}  // namespace

AutoPicker::Result AutoPicker::Observe(double nowMs, const std::vector<float>& scores) {
    Result res;

    // Melhor e segunda melhor leitura desta verificação.
    int best = -1;
    float bestScore = 0.0f;
    float secondScore = 0.0f;
    for (std::size_t i = 0; i < scores.size(); ++i) {
        const float s = std::clamp(scores[i], 0.0f, 1.0f);
        if (s > bestScore) {
            secondScore = bestScore;
            bestScore = s;
            best = static_cast<int>(i);
        } else if (s > secondScore) {
            secondScore = s;
        }
    }

    // Sem candidato válido: rejeição por confiança baixa ou espera simples.
    if (best < 0 || bestScore < params_.minConfidence) {
        candidate_ = -1;
        candidateCount_ = 0;
        candidateConfidence_ = 0.0f;
        if (best >= 0 && bestScore > 0.3f) {
            status_ = Status::LowScore;
            res.status = Status::LowScore;
            res.confidence = bestScore;
            res.reason = "melhor batida abaixo do minimo de confianca";
        } else {
            status_ = Status::Waiting;
            res.status = Status::Waiting;
            res.reason = "nenhuma imagem reconhecida na regiao";
        }
        // Arma atual permanece ativa (a imagem pode ter sumido temporariamente);
        // nada é executado ate uma nova transicao confirmada.
        res.weaponIndex = current_;
        return res;
    }

    // Ambiguidade: duas armas acima do minimo com confiancas muito proximas.
    if (secondScore >= params_.minConfidence &&
        (bestScore - secondScore) < params_.ambiguityMargin) {
        candidate_ = -1;
        candidateCount_ = 0;
        candidateConfidence_ = 0.0f;
        status_ = Status::Ambiguous;
        res.status = Status::Ambiguous;
        res.confidence = bestScore;
        res.reason = "duas imagens parecidas: ajuste a confianca minima ou as referencias";
        res.weaponIndex = current_;
        return res;
    }

    // Mesma arma ja confirmada em uso: nao repetir execucao (requisito 6).
    if (best == current_) {
        candidate_ = -1;
        candidateCount_ = 0;
        candidateConfidence_ = 0.0f;
        currentConfidence_ = bestScore;
        status_ = Status::Identified;
        res.status = Status::Identified;
        res.confidence = bestScore;
        res.weaponIndex = current_;
        res.reason = std::string("mantendo ") + StatusName(Status::Identified);
        return res;
    }

    // Anti-oscilação: janela minima apos uma troca — leituras diferentes dentro
    // dela sao ignoradas, impedindo alternancias rapidas entre dois itens. O
    // candidato corrente continua confirmado normalmente (a troca que acabou de
    // acontecer precisa "pegar" mesmo com confirmacoes > 1).
    if (best != current_ && best != candidate_ && nowMs - lastSwitchMs_ < params_.minIntervalMs) {
        res.status = status_;
        res.confidence = bestScore;
        res.weaponIndex = current_;
        res.reason = "intervalo minimo entre trocas";
        return res;
    }

    // Candidato diferente da arma atual: acumula confirmacoes consecutivas.
    if (best != candidate_) {
        candidate_ = best;
        candidateCount_ = 1;
        candidateConfidence_ = bestScore;
    } else {
        ++candidateCount_;
        candidateConfidence_ = std::max(candidateConfidence_, bestScore);
    }

    if (candidateCount_ < std::max(1, params_.confirmations)) {
        status_ = Status::Pending;
        res.status = Status::Pending;
        res.confidence = bestScore;
        res.weaponIndex = current_;
        res.reason = "aguardando confirmacoes (" + std::to_string(candidateCount_) + "/" +
                     std::to_string(std::max(1, params_.confirmations)) + ")";
        return res;
    }

    // Transicao confirmada: executar exatamente uma vez.
    current_ = candidate_;
    currentConfidence_ = candidateConfidence_;
    lastSwitchMs_ = nowMs;
    candidate_ = -1;
    candidateCount_ = 0;
    candidateConfidence_ = 0.0f;
    status_ = Status::Identified;
    res.switched = true;
    res.status = Status::Identified;
    res.confidence = currentConfidence_;
    res.weaponIndex = current_;
    res.reason = "imagem reconhecida: mudar para o item correspondente";
    return res;
}

void AutoPicker::Reset() {
    status_ = Status::Waiting;
    current_ = -1;
    currentConfidence_ = 0.0f;
    candidate_ = -1;
    candidateCount_ = 0;
    candidateConfidence_ = 0.0f;
    lastSwitchMs_ = -1.0e9;
}

}  // namespace st
