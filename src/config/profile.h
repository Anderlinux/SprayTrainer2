#pragma once

#include "core/defaults.h"
#include "core/repetition.h"
#include "core/weapon.h"

#include <array>
#include <string>
#include <vector>

namespace st {

// Tudo o que um perfil guarda. Tipos simples (sem ImGui/Windows) para poder ser
// testado em qualquer plataforma; a UI converte de/para o AppState.
struct ProfileData {
    // Treino
    Weapon weapon = kDefaultWeapon;
    float sensitivity = kDefaultSensitivity;
    int fireButton = 0;  // 0 = esquerdo, 1 = direito
    int scoreGoal = kDefaultScoreGoal;
    float scoreTolerance = 2.0f;

    std::array<Pattern, kWeaponCount> patterns{};

    // Overlay
    bool overlayVisible = true;
    int overlayCorner = 3;  // 0 sup. esq., 1 sup. dir., 2 inf. esq., 3 inf. dir.
    float overlayScale = 1.0f;
    float overlayOpacity = 0.92f;
    int overlayRefreshMs = 33;

    // Visual (gráfico de treino)
    int visualGuide = 0;  // 0 sempre, 1 só os próximos tiros, 2 escondida (desafio)
    int visualGuideAhead = 3;
    int visualPlayerColor = 0;  // índice em kPlayerColors
    float visualLineWidth = 2.0f;
    bool visualGrid = true;
    bool visualShotNumbers = false;

    // Interface
    float uiScale = 1.0f;  // 0.8 .. 1.5 (multiplica a escala do monitor)
    std::array<float, 3> accent{157.0f / 255.0f, 78.0f / 255.0f, 221.0f / 255.0f};  // #9D4EDD
    float windowOpacity = 1.0f;

    // Repetição automática do padrão (hotkey global). Os números são códigos virtuais
    // do Windows (VK_*); nomes e validação ficam em core/repetition.h, que st_config
    // já linka via st_core.
    bool repetitionEnabled = true;
    int repeatKeyVk = kDefaultRepeatKeyVk;  // padrão: LBUTTON — o clique de atirar dispara o replay
    // Padrão: "enquanto pressionada" (segura o botão, rajada; solta, para).
    // A tecla fixa "1" (arma principal) é quem religa após uma tecla de desativar.
    bool repeatToggle = false;
    // Várias teclas para desligar/pausar; padrão = 2..8, B, G, H (ver kDefaultStopKeysVk).
    std::vector<int> stopKeysVk{kDefaultStopKeysVk, kDefaultStopKeysVk + kDefaultStopKeysCount};
    int repeatSubsteps = 2;                 // suavização 1..20 (menor = mais rígido)
    bool repeatAutoScore = true;            // replay avalia a si mesmo (nota no histórico)
    bool repeatReturnBack = false;          // devolver o mouse à origem ao parar
    bool repeatRestartFromZero = true;      // cada clique novo do replay recomeça do início do padrão
    int cycleWeaponKeyVk = kDefaultCycleWeaponKeyVk;  // tecla para a próxima arma (0 = desligado)
    int cycleBackKeyVk = kDefaultCycleBackKeyVk;      // tecla para a arma anterior (0 = desligado)

    // Reconhecimento visual automático (seção "autoVision", opcional). Quando
    // ligado, o app captura a região abaixo e escolhe a arma sozinha — sem F1/F2.
    bool autoVisionEnabled = false;   // inicia desligado: modo manual é o comportamento padrão
    int autoRegionX = 0;              // região em pixels de tela (x, y, w, h)
    int autoRegionY = 0;
    int autoRegionW = 0;
    int autoRegionH = 0;
    std::string autoWindowTitle;      // título da janela monitorada (vazio = região fixa na tela)
    float autoConfidenceMin = 0.85f;  // confiança mínima (0..1) para aceitar um match
    int autoIntervalMs = 200;         // intervalo entre verificações (100..2000)
    int autoConfirmFrames = 3;        // leituras iguais consecutivas antes de trocar (1..10)

    bool operator==(const ProfileData& other) const;
};

// Limite aceito no load/save para tamanho da região (maior que isso é erro de configuração).
inline constexpr int kMaxAutoRegionSize = 16384;

// Valores de fábrica (padrões de recuo padrão incluídos).
ProfileData DefaultProfile();

inline constexpr int kProfileVersion = 1;

std::string ProfileToJson(const ProfileData& data);

struct ParseResult {
    bool ok = false;
    std::string error;                  // preenchido quando ok == false
    std::vector<std::string> warnings;  // valores ajustados para a faixa válida, campos ausentes etc.
};

// Lê e valida um perfil. Seções ausentes ficam com o padrão (com aviso);
// tipos errados, JSON inválido ou padrões com tamanho errado são erro.
ParseResult ProfileFromJson(const std::string& text, ProfileData& out);

}  // namespace st
