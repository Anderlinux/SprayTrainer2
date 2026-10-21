#pragma once

#include "core/vision/image.h"

#include <cstddef>
#include <string>
#include <vector>

namespace st {

// Imagens de referência do reconhecimento automático, UMA POR ARMA (o "Replay"
// do app é o conjunto de armas; cada arma tem seu template visual).
//
// Armazenamento: PNG compactado em base64 dentro do próprio perfil JSON
// (seção "weaponImages"), ao lado das outras configurações. Isso evita
// caminhos externos que quebram quando o perfil muda de pasta — requisito de
// persistência do recurso. O limite por imagem impede perfis gigantes e
// captura acidental da tela inteira.
inline constexpr std::size_t kMaxWeaponImageB64 = 512 * 1024;  // ~375 KB de PNG real

struct WeaponImageEntry {
    std::string weaponId;              // chave igual à dos padrões ("ak47", "m4a4"...)
    std::string pngBase64;             // PNG da referência; vazio = sem imagem cadastrada
};

// Codificação base64 padrão (RFC 4648, com padding). Sem dependências novas.
std::string Base64Encode(const unsigned char* data, std::size_t size);
std::string Base64Encode(const std::vector<unsigned char>& data);
// Devolve false se `text` contiver caracteres inválidos ou padding errado.
bool Base64Decode(std::string_view text, std::vector<unsigned char>& out);

// Converte uma imagem RGB para PNG (sem compressão pesada: zlib "stored" via
// filtro zero — suficiente para referências pequenas e determinístico).
void EncodePngRgb(const ImageRgb& image, std::vector<unsigned char>& outBytes);

// Carrega/salva a lista no perfil. Armas desconhecidas ou imagens acima do
// limite são descartadas com aviso; PNG inválido descarta só aquela entrada.
// Ausência total da seção não gera erro nem aviso (perfis antigos ok).
struct WeaponImagesParseResult {
    bool ok = true;
    std::vector<std::string> warnings;
};
WeaponImagesParseResult WeaponImagesFromJson(const std::string& jsonText,
                                             std::vector<WeaponImageEntry>& out);
std::string WeaponImagesToJson(const std::vector<WeaponImageEntry>& entries);

// ---- Integração com o JSON do perfil (texto já serializado). ----
// O perfil é salvo por ProfileToJson (config), que não conhece imagens para
// manter a camada de configuração livre de dependências de visão. Estas duas
// funções colam/cavam a seção "weaponImages" no texto do perfil na borda da UI:
//   - EmbedWeaponImages: insere/substitui a seção no objeto JSON do perfil.
//     Devolve false se o texto não for um objeto JSON válido.
//   - ExtractWeaponImages: lê a seção do texto do perfil (ausente = lista
//     vazia, sem aviso — perfis antigos continuam válidos).
bool EmbedWeaponImages(std::string& profileJsonText, const std::vector<WeaponImageEntry>& entries);
WeaponImagesParseResult ExtractWeaponImages(const std::string& profileJsonText,
                                            std::vector<WeaponImageEntry>& out);

}  // namespace st
