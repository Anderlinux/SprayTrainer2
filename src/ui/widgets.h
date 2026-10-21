#pragma once

#include "ui/icons.h"
#include "core/weapon.h"
#include "ui/textures.h"

#include <imgui.h>

#include <array>

namespace st::widgets {

// Interruptor grande: rosa = desligado, ciano = ligado (com animação).
bool Toggle(const char* id, bool* value, float height = 40.0f);

// Botão com contorno roxo, ícone opcional e texto em negrito (estilo da referência).
bool OutlineButton(const char* label, const icons::Icon* icon = nullptr, ImVec2 size = ImVec2(0.0f, 0.0f));

// Botão quadrado só com ícone.
bool IconButton(const char* id, icons::Icon icon, float size = 40.0f, bool framed = true);

// Cartão: painel #0F0F0F com borda e cantos arredondados.
void BeginCard(const char* id, ImVec2 size);
void EndCard();

// Silhueta da arma encaixada numa caixa fixa (mantém a proporção, centraliza).
void WeaponIcon(ImDrawList* dl, const Texture& tex, ImVec2 boxPos, ImVec2 boxSize);

// Linha de botões mutuamente exclusivos; devolve true se a seleção mudou.
bool Segmented(const char* id, const char* const* labels, int count, int* current, ImVec2 size);

// Três blocos (ícone + nome) para escolher a arma; devolve true se mudou.
bool WeaponTiles(const char* id, const std::array<Texture, kWeaponCount>& icons, Weapon* current, float width,
                 float scale, bool disabled, int columns = 4);

// Lista suspensa de armas com silhueta + nome (rolável). Devolve true se a escolha mudou.
bool WeaponDropdown(const char* id, const std::array<Texture, kWeaponCount>& icons, Weapon* current, float width,
                    float scale, bool disabled);

// Linha de configuração em tabela de 2 colunas: rótulo em negrito + ajuda à esquerda,
// controle à direita. Usar entre BeginSettings/EndSettings.
bool BeginSettings(const char* id, float labelWidth = 300.0f);
void SettingLabel(const char* label, const char* help);
void EndSettings();

// Texto pequeno em caixa alta, usado como rótulo de seção.
void Caption(const char* text, const ImVec4& color);

}  // namespace st::widgets
