#pragma once

#include "core/vision/image.h"

#include <cstddef>
#include <vector>

namespace st {

// Comparação de imagens por similaridade: SAD (soma das diferenças absolutas) média
// por canal, 0..255, convertida em confiança 0..1 (1 = idêntica). Núcleo puro, sem
// dependências externas — o reconhecimento pode ser testado em qualquer plataforma
// com imagens sintéticas. O template desliza sobre a região: a arma é reconhecida
// mesmo que apareça deslocada dentro da área monitorada.
class TemplateMatcher {
public:
    // Lado máximo após a redução (potência de 2 ajuda o downscale por 2x2).
    // 96 mantém templates de HUD legíveis e o custo por verificação baixo.
    static constexpr int kMaxSide = 96;

    struct WeaponTemplate {
        int weaponIndex = -1;  // índice no enum Weapon
        ImageRgb image;        // imagem de referência da arma (qualquer tamanho <= região)
    };

    // Escala comum: as imagens de referência são cadastradas NO TAMANHO DA REGIÃO
    // (recorte exato da área monitorada), então basta reduzir tudo pelo mesmo fator
    // para caber em kMaxSide. Regiões capturadas mantêm o mesmo tamanho da região
    // configurada — qualquer outro tamanho é rejeitado (a UI trata como erro).
    // Templates que não couberem na região reduzida são descartados (a UI avisa).
    void Build(const ImageRgb& referenceSize, const std::vector<WeaponTemplate>& templates);

    bool Empty() const { return templates_.empty(); }
    std::size_t Count() const { return templates_.size(); }

    struct Hit {
        float sad = 255.0f;      // menor SAD médio por canal encontrado (0 = idêntica)
        int x = 0;               // posição da melhor batida, na região ORIGINAL
        int y = 0;
        int templateWidth = 0;   // tamanho do template já na escala de trabalho
        int templateHeight = 0;
    };

    // Melhor batida de um template específico dentro da região. Devolve sad=255 se
    // não houver correspondência possível (região incompatível ou template ausente).
    Hit Match(int weaponIndex, const ImageRgb& region) const;

    // Converte SAD em confiança. Escala logarítmica: a confiança cai gradualmente
    // conforme a diferença média por canal aumenta (diferença 0 -> 1.0; ~2 -> 0.94;
    // ~8 -> 0.75; ~32 -> 0.5; ~128 -> 0.25). Com a fórmula linear 1-sad/255, armas
    // com fundos idênticos e emblemas diferentes ficariam todas acima de 0.9 — o
    // discriminador real do HUD é a área do emblema, não o fundo.
    static float Confidence(float sad);

    // Distância de Hamming entre duas imagens (mesmo tamanho): número de pixels
    // que diferem em qualquer canal. Usado pela UI para avisar templates quase
    // idênticos (risco de ambiguidade no modo automático).
    static std::size_t PixelDiffCount(const ImageRgb& a, const ImageRgb& b);

    // Acesso por SLOT interno (ordem dos templates passados em Build), usado
    // pelo AutoVisionController para varrer todas as armas cadastradas.
    // MatchBySlot devolve sad=255 se o slot não existir.
    int WeaponIndexAt(int slot) const {
        return (slot >= 0 && static_cast<std::size_t>(slot) < templates_.size())
                   ? templates_[static_cast<std::size_t>(slot)].weaponIndex
                   : -1;
    }
    Hit MatchBySlot(int slot, const ImageRgb& region) const;

private:
    struct Entry {
        int weaponIndex = -1;
        ImageRgb image;
    };

    // Laço SAD compartilhado por Match/MatchBySlot (evita duplicação).
    Hit MatchImpl(const Entry& entry, const ImageRgb& region) const;

    int factor_ = 1;             // divisor linear aplicado a região e templates
    int refW_ = 0;               // lado da região reduzida (tamanho de trabalho)
    int refH_ = 0;
    std::vector<Entry> templates_;
};

}  // namespace st
