#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace st {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

// Novas armas entram sempre no fim: o índice é usado em perfis e no histórico.
enum class Weapon : int { AK47 = 0, M4A4 = 1, M4A1S = 2, Famas = 3, GalilAR = 4, Mac10 = 5, Mp9 = 6, Mp5sd = 7,
                          Mp7 = 8, P90 = 9, Bizon = 10, Sg553 = 11, Ump45 = 12, Count = 13 };

inline constexpr std::size_t kWeaponCount = static_cast<std::size_t>(Weapon::Count);
inline constexpr std::size_t kPatternPoints = 31;

// Padrão de spray: p[i] é o deslocamento (em graus) que o tiro i soma ao anterior
// se o jogador não mexer o mouse. x > 0 = direita, y > 0 = para cima. p[0] = {0,0}.
using Pattern = std::array<Vec2, kPatternPoints>;

struct WeaponInfo {
    std::string_view id;    // chave usada nos perfis JSON
    std::string_view name;  // nome exibido na UI
    float fireIntervalMs;   // tempo entre tiros
    int magazine;           // tiros usados no treino (<= kPatternPoints)
    bool inFirstVersion;    // existia na v1 dos perfis (as outras podem faltar sem aviso)
};

constexpr std::size_t Index(Weapon w) { return static_cast<std::size_t>(w); }

const WeaponInfo& GetWeaponInfo(Weapon w);

// Padrão de fábrica da arma (perfil "padrão"); editável na aba Ajustes.
Pattern DefaultPattern(Weapon w);

}  // namespace st
