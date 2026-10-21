#pragma once

#include "config/profile.h"
#include "ui/app_state.h"  // AppState (usado por referência nos APIs State)

#include <string>
#include <vector>

#include "core/vision/auto_picker.h"
#include "core/vision/image.h"
#include "core/vision/matcher.h"
#include "core/vision/weapon_images.h"

namespace st {

// Orquestração do reconhecimento visual automático, separada das chamadas
// Win32 para poder ser testada em qualquer plataforma (padrão do projeto:
// lógica no núcleo/UI portável, API nativa só na borda).
// O App chama TickAutoVision() a cada frame; quando auto == true, esta classe
// pede uma captura da região configurada, roda o TemplateMatcher e decide via
// AutoPicker. A troca de arma efetiva acontece no App (thread da UI), usando
// exatamente o mesmo caminho das teclas F1/F2 (SelectWeapon).
class AutoVisionController {
public:
    struct FrameStatus {
        bool enabled = false;         // modo automático ligado
        bool captureFailed = false;   // última captura falhou (janela fechada etc.)
        AutoPicker::Status status = AutoPicker::Status::Waiting;
        int weaponIndex = -1;         // arma reconhecida agora (-1 = nenhuma)
        float confidence = 0.0f;
        std::string reason;           // motivo legível da última decisão/rejeição
        double lastSwitchMs = -1.0e9; // relógio do app na última troca confirmada
    };

    // Recarrega templates a partir do perfil (seção weaponImages + região).
    // Devolve avisos legíveis para a UI (PNG inválido, template maior que a
    // região etc.). Chamado ao ligar o modo automático e após editar imagens.
    std::vector<std::string> ReloadTemplates(const ProfileData& profile,
                                             const std::vector<WeaponImageEntry>& entries);

    // Comparação isolada de UM template contra uma imagem de região (botão
    // "Testar agora" da aba): não toca no estado do AutoPicker nem troca arma.
    // `pngBase64` é a imagem cadastrada; devolve false se nada pode ser
    // comparado (sem região, PNG inválido ou template maior que a região).
    static bool ScoreTemplate(const ProfileData& profile, const std::string& pngBase64,
                              const ImageRgb& region, float& outConfidence);

    struct CaptureResult {
        bool ok = false;
        ImageRgb image;      // válido se ok
        std::string error;   // motivo legível se !ok (ex.: janela não encontrada)
    };

    // Uma verificação completa: captura -> match -> decisão. `nowMs` é o
    // relógio do app (GetTickCount64). Não faz nada se desligado ou se o
    // intervalo entre verificações ainda não passou (requisito de desempenho).
    // `outSwitched` é true exatamente quando o App deve selecionar a arma
    // `outWeaponIndex` (transição confirmada pelo AutoPicker).
    void Tick(const ProfileData& profile, double nowMs, CaptureResult (*capture)(const ProfileData&),
              bool& outSwitched, int& outWeaponIndex);

    // Versão que lê a configuração direto do estado da UI (AppState::autoVision):
    // evita cópias e mantém uma única fonte de verdade na thread da UI.
    void TickState(const AppState& state, double nowMs, CaptureResult (*capture)(const AppState&),
                   bool& outSwitched, int& outWeaponIndex);

    // Recarrega templates a partir do estado da UI (weaponImages + região).
    std::vector<std::string> ReloadTemplatesState(const AppState& state);

    // Imagem cadastrada para uma arma (índice do enum Weapon) no estado da UI.
    // Devolve nullptr se a arma não existe ou não tem imagem; caso contrário o
    // ponteiro aponta para dentro de `state.weaponImages` (válido enquanto o
    // vetor não for modificado — a UI usa imediatamente após chamar).
    static const WeaponImageEntry* WeaponImageEntryFor(const AppState& state, int weaponIndex);

    const FrameStatus& Status() const { return status_; }
    // Última imagem capturada com sucesso (para prévia na UI). Vazia se nunca.
    const ImageRgb& LastCapture() const { return lastCapture_; }
    bool HasLastCapture() const { return !lastCapture_.Empty(); }

    // Desliga/esquece tudo (chamado ao desativar o modo automático).
    void Reset();

private:
    TemplateMatcher matcher_;
    AutoPicker picker_;
    FrameStatus status_;
    ImageRgb lastCapture_;
    double lastCheckMs_ = -1.0e9;
    bool templatesLoaded_ = false;
};

}  // namespace st
