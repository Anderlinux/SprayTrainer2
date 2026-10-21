#pragma once

#include "core/weapon.h"

namespace st {

// Configurações de fábrica do treino (perfil "padrão"). Os padrões de recuo de fábrica
// ficam em DefaultPattern() (weapon.cpp). Perfil e estado do app leem daqui, para a primeira
// abertura não aparecer como "alterações não salvas".
inline constexpr Weapon kDefaultWeapon = Weapon::Ump45;
inline constexpr float kDefaultSensitivity = 1.35f;
inline constexpr int kDefaultScoreGoal = 60;

}  // namespace st
