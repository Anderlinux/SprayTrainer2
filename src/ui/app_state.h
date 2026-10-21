#pragma once

#include "config/profile.h"
#include "core/defaults.h"
#include "core/history.h"
#include "core/pattern_store.h"
#include "core/repetition.h"
#include "core/training_session.h"
#include "core/vision/weapon_images.h"
// textures.h NÃO entra aqui: ele puxa <d3d11.h>/<imgui.h>, indisponíveis para o
// núcleo portável (st_core/st_config/testes em Linux). Os campos de textura da
// UI usam ponteiro opaco, preenchido apenas pelo lado Windows.
struct ID3D11ShaderResourceView;
// Mesmo padrão acima: <d3d11.h> indisponível no núcleo portável; declaração
// prévia basta para o campo de ponteiro (preenchido só pelo lado Windows).
struct ID3D11Device;

// ImGui também NÃO entra aqui: app_state.h é incluído por st_core (via
// ui/auto_vision.h), que não tem os include dirs do third_party/imgui. As
// cores da interface usam esta struct portável; a conversão para ImVec4
// acontece apenas nos arquivos .cpp da UI (que já incluem <imgui.h>).
namespace st {
struct Rgba4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};
}  // namespace st

#include <array>
#include <cstdint>  // std::uint8_t (MSVC: não garantido pelos headers acima)
#include <optional>
#include <string>
#include <vector>

namespace st {

enum class Page { Principal, Ajustes, Visual, Historico, Presets, Count };

enum class FireButton { Left = 0, Right = 1 };

enum class OverlayCorner { TopLeft = 0, TopRight = 1, BottomLeft = 2, BottomRight = 3 };

// Configurações do overlay (editáveis na aba Ajustes a partir da Etapa 4).
struct OverlaySettings {
    bool visible = true;
    OverlayCorner corner = OverlayCorner::BottomRight;
    float scale = 1.0f;      // 0.75 .. 1.5
    float opacity = 0.92f;   // 0.3 .. 1.0
    int refreshMs = 33;      // intervalo de atualização no modo compacto
};

// Aparência (aba Ajustes > Interface).
struct InterfaceSettings {
    Rgba4 accent{157.0f / 255.0f, 78.0f / 255.0f, 221.0f / 255.0f, 1.0f};  // #9D4EDD
    float windowOpacity = 1.0f;                    // 0.6 .. 1.0
    float scale = 1.0f;                            // 0.8 .. 1.5, multiplica a escala do monitor
    float scaleDraft = -1.0f;                      // valor no slider enquanto arrasta (aplica ao soltar)
};

enum class GuideMode { Always = 0, Next = 1, Hidden = 2 };

// Gráfico de treino (aba Visual).
struct VisualSettings {
    GuideMode guide = GuideMode::Always;
    int guideAhead = 3;     // modo "próximos tiros"
    int playerColor = 0;    // índice em theme::kPlayerColors
    float lineWidth = 2.0f;
    bool grid = true;
    bool shotNumbers = false;
};

// Gravação de um padrão desenhando com o mouse (editor em Ajustes > Padrões de recuo).
enum class RecorderMode { Off, Ready, Review };
struct PatternRecorder {
    RecorderMode mode = RecorderMode::Off;
    Weapon weapon = Weapon::AK47;
    TrainingSession session;     // só mede o movimento e o tempo de cada tiro
    std::vector<Vec2> path;      // caminho gravado (modo Review)
    int smoothing = 1;           // 0 a 3
};

// Reconhecimento visual automático (mesmos campos da seção "autoVision" do perfil).
// Ligado, o app escolhe a arma sozinho pela imagem na região monitorada — sem F1/F2.
// Desligado (padrão), tudo continua exatamente como hoje.
struct AutoVisionSettings {
    bool enabled = false;
    int regionX = 0;
    int regionY = 0;
    int regionW = 0;
    int regionH = 0;
    std::string windowTitle;      // vazio = região fixa em coordenadas de tela
    float confidenceMin = 0.85f;  // 0..1
    int intervalMs = 200;         // 100..2000
    int confirmFrames = 3;        // 1..10
};

// Configuração da repetição automática (aba Ajustes > Repetição). Os VK são códigos
// virtuais do Windows; nomes e faixa válida vêm de core/repetition.h.
struct RepetitionSettings {
    bool enabled = true;
    int repeatKeyVk = kDefaultRepeatKeyVk;   // botão que dispara a repetição (padrão: LBUTTON)
    // Padrão: "enquanto pressionada" — segura o botão de atirar, o replay faz a
    // rajada; solta, para. A tecla fixa "1" (arma principal) é quem religa depois
    // de uma tecla de desativar ter cortado o replay.
    RepeatActivation activation = RepeatActivation::Hold;  // segurar ou alternar (toque)
    // Padrão: teclas das armas secundárias no CS2 (2..9, B, G, H): um toque corta
    // o replay na hora; só "1" religa.
    std::vector<int> stopKeysVk{kDefaultStopKeysVk, kDefaultStopKeysVk + kDefaultStopKeysCount};
    int substeps = 2;                        // suavização 1..kMaxSubsteps
    bool autoScore = true;                   // o replay avalia a si mesmo (nota no histórico)
    bool returnBack = false;                 // devolver o mouse à origem ao parar
    bool restartFromZero = true;             // cada clique novo do replay recomeça do início do padrão
    int cycleWeaponKeyVk = kDefaultCycleWeaponKeyVk;  // tecla para a próxima arma (0 = desligado)
    int cycleBackKeyVk = kDefaultCycleBackKeyVk;      // tecla para a arma anterior (0 = desligado)
};

class ProfileStore;

enum class NamePrompt { None, SaveAs, New, Rename, Duplicate };
enum class FileAction { None, Import, Export };

// Perfis: o que está carregado, a última versão salva e os diálogos em aberto.
struct ProfileUiState {
    ProfileStore* store = nullptr;     // dono: App
    std::string current;               // vazio = ainda não salvo em nenhum perfil
    ProfileData savedSnapshot;         // como o perfil atual está em disco
    std::vector<std::string> list;     // cache da pasta de perfis
    double listRefreshedMs = -1.0e9;
    std::string selected;              // linha selecionada na aba Presets

    NamePrompt prompt = NamePrompt::None;
    char promptBuffer[128] = {};
    std::string promptError;
    std::string promptTarget;          // perfil alvo de Renomear/Duplicar
    bool confirmDelete = false;
    bool exitAfterSave = false;        // "Salvar e sair" sem perfil atual
    FileAction pendingFile = FileAction::None;
    bool requestOpenFolder = false;

    // Aviso no topo do conteúdo (sucesso some sozinho; erro fica até fechar).
    std::string banner;
    bool bannerError = false;
    double bannerUntilMs = 0.0;
};

// Estado da aplicação. Vive na thread da UI e não é recriado ao trocar de aba;
// o que é compartilhado com outras threads (padrões) fica no PatternStore.
struct AppState {
    Page page = Page::Principal;
    Weapon weapon = kDefaultWeapon;
    float sensitivity = kDefaultSensitivity;

    // Device DX11 para as texturas da prévia do reconhecimento (injetado pelo
    // App no RenderMain, antes de desenhar as páginas). Nulo fora do Windows.
    ID3D11Device* dx11Device = nullptr;

    // Tela Principal
    bool trainingEnabled = false;          // interruptor: a área de treino só aceita rajadas ligada
    FireButton fireButton = FireButton::Left;
    int scoreGoal = kDefaultScoreGoal;     // 0 = sem meta
    float scoreTolerance = TrainingSession::kScoreTolerance;  // erro médio (graus) que dá nota 0
    bool mouseDetected = false;

    // Estatísticas da sessão (desde que o app abriu)
    double appStartMs = 0.0;
    int bursts = 0;
    float bestScore = 0.0f;

    PatternStore patterns;
    TrainingSession session;
    std::optional<SessionResult> lastResult;

    // Repetição automática do padrão (hotkey global; ver App::TickRepetition)
    RepetitionSettings repetition;
    // Reconhecimento visual automático (aba Ajustes > Auto-Reconhecimento).
    // As imagens de referência ficam em state_.weaponImages (seção "weaponImages").
    AutoVisionSettings autoVision;
    std::vector<WeaponImageEntry> weaponImages;
    // Pedido da aba "Auto-Reconhecimento" para o App recarregar os templates do
    // AutoVisionController no próximo tick (desenhado na thread da UI, aplicado
    // fora dela — padrão do projeto).
    bool autoVisionReloadRequest = false;
    // Status publicado pelo App::TickAutoVision a cada frame; lido pela aba
    // "Auto-Reconhecimento" durante o render (copia simples, mesma thread).
    int autoVisionStatus = 0;              // valor de AutoPicker::Status como int
    float autoVisionConfidence = 0.0f;     // confiança da última decisão
    int autoVisionRecognized = -1;         // arma reconhecida agora (-1 = nenhuma)
    double autoVisionLastSwitchMs = -1.0e9;  // relógio do app na última troca confirmada
    bool autoVisionCaptureFailed = false;  // última captura falhou (janela fechada etc.)
    std::string autoVisionLastReason;      // motivo legível da última decisão/rejeição
    // Prévia "Capturar agora" (aba Auto-Reconhecimento): o App preenche com a
    // imagem capturada e as dimensões no pedido feito pela UI (thread da UI).
    int autoVisionPreviewW = 0;
    int autoVisionPreviewH = 0;
    std::vector<std::uint8_t> autoVisionPreviewRgb;  // RGB24, tamanho w*h*3 (vazio = sem prévia)
    // Prévia do template cadastrado: id da arma ("ak47") + bytes PNG originais.
    std::string autoVisionTemplateId;
    std::vector<std::uint8_t> autoVisionTemplatePng;
    // Resultado do teste manual "Testar agora" (aba Auto-Reconhecimento):
    // confiança da melhor batida e nome da arma correspondente (vazio = nenhuma).
    float autoVisionTestConfidence = -1.0f;
    std::string autoVisionTestWeaponName;
    bool repeatArmed = true;                 // false até a tecla de atalho ser solta
    int repeatHotkeyId = 0;                  // hotkey global registrada (0 = nenhuma)
    std::vector<int> registeredStopKeys{};   // VKs realmente registrados no Windows
    bool repeatToggledOn = false;            // modo alternar: rajada contínua ativa
    bool cycleHotkeyDown = false;            // última leitura da tecla de próxima arma
    bool cycleBackHotkeyDown = false;        // última leitura da tecla de arma anterior
    // Estado da repetição em andamento, copiado do repeater a cada frame pelo App.
    // O overlay usa para mostrar "REPLAY ATIVO" e o progresso dos tiros.
    bool repetitionRunning = false;          // rajada ou retorno à origem em andamento
    int repetitionShot = 0;                  // tiros já iniciados no ciclo atual
    int repetitionShotsTotal = 0;            // total de tiros do padrão da arma

    // Texturas das armas: mantidas como ponteiro opaco (ID3D11ShaderResourceView)
    // para que este header compile no núcleo portável sem d3d11/imgui. As
    // dimensões ficam em arrays paralelos; o ciclo de vida é gerenciado pelo App
    // via ReleaseTexture no lado Win32 (ver app.cpp, que usa st::Texture*).
    std::array<ID3D11ShaderResourceView*, kWeaponCount> weaponIcons{};
    std::array<float, kWeaponCount> weaponIconW{};  // largura do ícone (px)
    std::array<float, kWeaponCount> weaponIconH{};  // altura do ícone (px)

    OverlaySettings overlay;
    bool overlayExpanded = true;     // painel aberto por padrão: o replay é o foco principal
    // Aba inicial do overlay expandido. O replay é a função principal, então a aba
    // "Replay" (0) é a padrão; "Padrão" (editor) fica como segunda opção.
    bool overlayShowEditor = false;  // aba "Padrão" do overlay expandido

    InterfaceSettings ui;
    VisualSettings visual;

    History history;
    int historyFilter = 0;      // 0 = todas, 1.. = arma (índice + 1)
    int historySelected = -1;   // índice em history.Entries()
    bool confirmClearHistory = false;

    // Editor de padrões: versão "salva" de cada arma (para desfazer e marcar alterações).
    std::array<Pattern, kWeaponCount> savedPatterns{};
    Weapon editorWeapon = kDefaultWeapon;
    int editorRow = -1;  // linha selecionada (destacada no gráfico)
    int settingsTabRequest = -1;  // força uma aba de Ajustes no próximo frame
    PatternRecorder recorder;

    ProfileUiState profiles;
    double nowMs = 0.0;  // relógio do frame atual (para avisos com prazo)

    // Pedidos da UI para a janela nativa, tratados pelo App após o frame.
    bool requestMinimize = false;
    bool requestToggleMaximize = false;
    bool requestClose = false;
    bool requestShowMain = false;  // botão "abrir app" do overlay
    bool confirmClose = false;     // fechar com alterações não salvas
    bool allowClose = false;
    bool captureMouse = false;  // true enquanto o botão de treino está pressionado
    bool maximized = false;
};

// Ações usadas tanto na tela Principal quanto no overlay.
inline const char* FireButtonName(FireButton b) {
    return b == FireButton::Left ? "Botão esquerdo" : "Botão direito";
}

inline void ToggleFireButton(AppState& s) {
    s.fireButton = s.fireButton == FireButton::Left ? FireButton::Right : FireButton::Left;
}

inline void CycleScoreGoal(AppState& s) {
    constexpr int kGoals[] = {0, 60, 70, 80, 90};
    constexpr int kCount = static_cast<int>(sizeof(kGoals) / sizeof(kGoals[0]));
    int next = 0;
    for (int i = 0; i < kCount; ++i) {
        if (kGoals[i] == s.scoreGoal) {
            next = (i + 1) % kCount;
        }
    }
    s.scoreGoal = kGoals[next];
}

inline void SelectWeapon(AppState& s, Weapon w) {
    if (s.session.Running() || s.weapon == w) {
        return;
    }
    s.weapon = w;
    s.lastResult.reset();
}

}  // namespace st
