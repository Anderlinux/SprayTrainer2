#pragma once

#include "core/training_session.h"
#include "core/weapon.h"

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <vector>

namespace st {

// Repetição automática do padrão de recuo (inversão do movimento ideal).
// A classe é pura: recebe o instante atual e devolve os deltas de mouse a enviar,
// sem tocar em APIs do Windows — assim pode ser testada em qualquer plataforma.
class PatternRepeater {
public:
    enum class State { Idle, Running, Returning };

    // Divide cada passo do caminho ideal em `substeps` micro-movimentos (1..20).
    // Menor = mais "rígido"; maior = movimento mais suave.
    static constexpr int kMaxSubsteps = 20;

    void Configure(Weapon weapon, const Pattern& pattern, float sensitivity, int substeps);

    // Duração média do frame da UI (ms), medida em tempo real pelo App::Run. Usada na
    // compensação de atraso da fila (dead-time): o movimento enviado só conta para a
    // nota quando DrainMouse o processa (~1 frame depois, entre SendInput e WM_INPUT).
    // 0 = sem compensação (comportamento nominal antigo; usado pelos testes determinísticos).
    void SetUiFrameMs(double frameMs);

    // Subpassos realmente usados na próxima Start() (pode ser maior que o valor dado
    // em Configure: há um clamp interno para ~1 micro-movimento por frame da UI).
    int EffectiveSubsteps() const { return effSubsteps_; }

    // Começa a reproduzir o padrão da arma configurada. `nowMs` é o relógio do app (NowMs).
    void Start(double nowMs);
    // Interrompe agora; se `returnBack`, compensa o que já foi enviado antes de parar.
    void Stop(double nowMs, bool returnBack);

    // Chame a cada frame com o tempo atual. Devolve os deltas INTEIROS de mouse
    // que devem ser enviados nesta chamada (pode ser vazio). Nunca bloqueia:
    // o tempo é controlado pelo relógio, não por sleep.
    std::vector<MouseSample> Tick(double nowMs);

    State GetState() const { return state_; }
    bool Running() const { return state_ != State::Idle; }
    std::size_t ShotsDone() const { return step_; }
    std::size_t ShotsTotal() const { return path_.size(); }

private:
    struct SubMove {
        float x = 0.0f;  // counts acumulados deste submovimento
        float y = 0.0f;
        double atMs = 0.0;
    };

    void BuildQueue(double nowMs);
    void BuildReturn(double nowMs);

    State state_ = State::Idle;
    Weapon weapon_ = Weapon::AK47;
    int substeps_ = 1;
    int effSubsteps_ = 1;  // subpassos após o clamp de cadência (usado na fila/Tick)
    float degPerCount_ = TrainingSession::kDegreesPerCount;
    double intervalMs_ = 100.0;
    double uiFrameMs_ = 16.7;  // duração média do frame da UI (compensação de atraso)
    std::vector<Vec2> path_;      // movimento ideal acumulado (graus), como no treino
    std::vector<SubMove> queue_;  // micro-movimentos ainda não enviados
    std::size_t cursor_ = 0;      // próxima entrada de queue_
    std::size_t step_ = 0;        // tiros já iniciados (para o retorno parcial)
    long long sentX_ = 0;         // total enviado desde o início (para devolver tudo)
    long long sentY_ = 0;
};

// ------------------------------------------------------------------ Teclas da repetição
// Códigos virtuais do Windows (VK_*). Os números são os mesmos em qualquer plataforma,
// então ficam no núcleo portável e testável; só a entrega das mensagens usa Win32.
inline constexpr int kVkLButton = 0x01;
inline constexpr int kVkRButton = 0x02;
inline constexpr int kVkMButton = 0x04;
inline constexpr int kVkXButton1 = 0x05;
inline constexpr int kVkXButton2 = 0x06;
inline constexpr int kVkBackspace = 0x08;
inline constexpr int kVkTab = 0x09;
inline constexpr int kVkEnter = 0x0D;
inline constexpr int kVkShift = 0x10;
inline constexpr int kVkControl = 0x11;
inline constexpr int kVkAlt = 0x12;
inline constexpr int kVkCapsLock = 0x14;
inline constexpr int kVkEscape = 0x1B;
inline constexpr int kVkSpace = 0x20;
inline constexpr int kVkPrior = 0x21;   // Page Up
inline constexpr int kVkNext = 0x22;    // Page Down
inline constexpr int kVkEnd = 0x23;
inline constexpr int kVkHome = 0x24;
inline constexpr int kVkLeft = 0x25;
inline constexpr int kVkUp = 0x26;
inline constexpr int kVkRight = 0x27;
inline constexpr int kVkDown = 0x28;
inline constexpr int kVkInsert = 0x2D;
inline constexpr int kVkDelete = 0x2E;
inline constexpr int kVkF1 = 0x70;      // F1..F12 consecutivos
inline constexpr int kVkNumPad0 = 0x60; // 0..9 consecutivos
inline constexpr int kVkKeyF = 0x46;    // tecla de repetição padrão
inline constexpr int kVkDigit1 = 0x31;  // fileira superior: 1..9 consecutivos

// Tecla padrão para DISPARAR a repetição: botão esquerdo do mouse — o mesmo
// clique de atirar no CS2 dispara o replay (segurar = rajada, no modo padrão
// "enquanto pressionada"). A tecla "1" ficou reservada para RELIGAR o replay
// depois de uma tecla de desativar (ver kDefaultEnableKeyVk): como "1" é a
// slot da arma principal no jogo, tocar nela troca para a primária e religa o
// macro, pronto para o próximo clique de atirar.
inline constexpr int kDefaultRepeatKeyVk = kVkLButton;
// Tecla fixa que religa o replay após ele ter sido desativado por uma tecla de
// desativar. Não é configurável — em jogo ela também troca para a arma
// principal, exatamente quando o jogador quer o replay de volta.
inline constexpr int kDefaultEnableKeyVk = kVkDigit1;
// Teclas padrão para desligar/pausar a repetição com um único clique:
// números 2..9 (fileira superior, perto do F1) e as letras B, G, H.
// São também as teclas de armas secundárias no CS2 — ao tocar qualquer uma
// delas o replay desliga sozinho; só a tecla de ativar ("1", arma principal)
// religa. O "9" não é tecla de arma, mas ajuda quando a lista cresce.
inline constexpr int kDefaultStopKeysVk[] = {
    0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,  // DIGIT2..DIGIT9
    0x42,                                            // B
    0x47,                                            // G
    0x48,                                            // H
};
inline constexpr std::size_t kDefaultStopKeysCount =
    sizeof(kDefaultStopKeysVk) / sizeof(kDefaultStopKeysVk[0]);
// Teclas padrão para alternar armas: F1 avança para a próxima arma, F2 volta
// para a anterior. São livres no CS2 (não colidem com as slots 1-9 nem com a
// tecla fixa de religar "1"). 0 = desligado.
inline constexpr int kDefaultCycleWeaponKeyVk = kVkF1;   // F1: próxima arma
inline constexpr int kDefaultCycleBackKeyVk = 0x71;       // F2: arma anterior
inline constexpr std::size_t kMaxStopKeys = 11;     // comporta as 11 teclas padrão

// Modos de ativação da repetição automática.
enum class RepeatActivation {
    Hold = 0,     // enquanto a tecla estiver pressionada (padrão; funciona com botões do mouse)
    Toggle = 1,   // um toque liga/desliga a repetição contínua
};

// Lista de teclas que a UI oferece para repetir/desligar (nomes iguais aos gravados no perfil).
struct KeyOption {
    int vk;
    const char* name;
};

constexpr KeyOption kKeyOptions[] = {
    {kVkLButton, "LBUTTON"},   {kVkRButton, "RBUTTON"},     {kVkMButton, "MBUTTON"},
    {kVkXButton1, "X1BUTTON"}, {kVkXButton2, "X2BUTTON"},   {kVkBackspace, "BACKSPACE"},
    {kVkTab, "TAB"},           {kVkEnter, "ENTER"},         {kVkShift, "SHIFT"},
    {kVkControl, "CTRL"},      {kVkAlt, "ALT"},             {kVkCapsLock, "CAPSLOCK"},
    {kVkEscape, "ESC"},        {kVkSpace, "SPACE"},         {kVkPrior, "PAGEUP"},
    {kVkNext, "PAGEDOWN"},     {kVkEnd, "END"},             {kVkHome, "HOME"},
    {kVkLeft, "LEFT"},         {kVkUp, "UP"},               {kVkRight, "RIGHT"},
    {kVkDown, "DOWN"},         {kVkInsert, "INSERT"},       {kVkDelete, "DELETE"},
    // Fileira superior de números (VK 0x30='0', 0x31='1' ... 0x39='9').
    {0x30, "KEY0"}, {0x31, "KEY1"}, {0x32, "KEY2"}, {0x33, "KEY3"}, {0x34, "KEY4"},
    {0x35, "KEY5"}, {0x36, "KEY6"}, {0x37, "KEY7"}, {0x38, "KEY8"}, {0x39, "KEY9"},
    {0x41, "A"},               {0x42, "B"},                 {0x43, "C"},
    {0x44, "D"},               {0x45, "E"},                 {0x46, "F"},
    {0x47, "G"},               {0x48, "H"},                 {0x49, "I"},
    {0x4A, "J"},               {0x4B, "K"},                 {0x4C, "L"},
    {0x4D, "M"},               {0x4E, "N"},                 {0x4F, "O"},
    {0x50, "P"},               {0x51, "Q"},                 {0x52, "R"},
    {0x53, "S"},               {0x54, "T"},                 {0x55, "U"},
    {0x56, "V"},               {0x57, "W"},                 {0x58, "X"},
    {0x59, "Y"},               {0x5A, "Z"},
    {kVkF1, "F1"},   {0x71, "F2"},   {0x72, "F3"},   {0x73, "F4"},   {0x74, "F5"},   {0x75, "F6"},
    {0x76, "F7"},    {0x77, "F8"},   {0x78, "F9"},   {0x79, "F10"},  {0x7A, "F11"},  {0x7B, "F12"},
    {kVkNumPad0, "NUM0"}, {0x61, "NUM1"}, {0x62, "NUM2"}, {0x63, "NUM3"}, {0x64, "NUM4"},
    {0x65, "NUM5"},  {0x66, "NUM6"}, {0x67, "NUM7"}, {0x68, "NUM8"}, {0x69, "NUM9"},
};

inline constexpr std::size_t kKeyOptionCount = sizeof(kKeyOptions) / sizeof(kKeyOptions[0]);

// Nome legível de um código virtual ("F", "LBUTTON"...); desconhecidas viram "VK<hex>".
std::string_view VirtualKeyName(int vk);

// Nome -> código virtual, ignorando maiúsculas/minúsculas. Devolve 0 se não existir.
int VirtualKeyFromName(std::string_view name);

}  // namespace st
