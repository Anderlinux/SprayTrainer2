#include "core/vision/weapon_images.h"

#include "core/vision/png.h"
#include "core/weapon.h"

#include <nlohmann/json.hpp>

#include <array>
#include <cstdint>

namespace st {
namespace {

constexpr char kBase64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Tabela reversa: -1 = inválido.
signed char Base64Value(char c) {
    if (c >= 'A' && c <= 'Z') return static_cast<signed char>(c - 'A');
    if (c >= 'a' && c <= 'z') return static_cast<signed char>(c - 'a' + 26);
    if (c >= '0' && c <= '9') return static_cast<signed char>(c - '0' + 52);
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

// ---- CRC32 e Adler32 (RFC 1950/1951), para escrever PNG com zlib "stored". ----
std::uint32_t Crc32(const unsigned char* data, std::size_t size) {
    static std::array<std::uint32_t, 256> table = [] {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            t[i] = c;
        }
        return t;
    }();
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i) {
        crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

void AppendU16Le(std::vector<unsigned char>& v, std::uint16_t x) {
    // RFC 1951: LEN/NLEN de blocos "stored" são little-endian (diferente do PNG).
    v.push_back(static_cast<unsigned char>(x & 0xFFu));
    v.push_back(static_cast<unsigned char>(x >> 8));
}

void AppendU16Be(std::vector<unsigned char>& v, std::uint16_t x) {
    v.push_back(static_cast<unsigned char>(x >> 8));
    v.push_back(static_cast<unsigned char>(x & 0xFFu));
}

void AppendU32Be(std::vector<unsigned char>& v, std::uint32_t x) {
    v.push_back(static_cast<unsigned char>(x >> 24));
    v.push_back(static_cast<unsigned char>((x >> 16) & 0xFFu));
    v.push_back(static_cast<unsigned char>((x >> 8) & 0xFFu));
    v.push_back(static_cast<unsigned char>(x & 0xFFu));
}

void AppendPngChunk(std::vector<unsigned char>& png, const char* type,
                    const std::vector<unsigned char>& payload) {
    AppendU32Be(png, static_cast<std::uint32_t>(payload.size()));
    const std::size_t typeStart = png.size();
    for (int i = 0; i < 4; ++i) png.push_back(static_cast<unsigned char>(type[i]));
    png.insert(png.end(), payload.begin(), payload.end());
    const std::uint32_t crc =
        Crc32(png.data() + typeStart, png.size() - typeStart);
    AppendU32Be(png, crc);
}

}  // namespace

std::string Base64Encode(const unsigned char* data, std::size_t size) {
    std::string out;
    out.reserve(((size + 2) / 3) * 4);
    std::size_t i = 0;
    while (i + 3 <= size) {
        const std::uint32_t v = (static_cast<std::uint32_t>(data[i]) << 16) |
                                (static_cast<std::uint32_t>(data[i + 1]) << 8) |
                                static_cast<std::uint32_t>(data[i + 2]);
        out.push_back(kBase64Chars[(v >> 18) & 63]);
        out.push_back(kBase64Chars[(v >> 12) & 63]);
        out.push_back(kBase64Chars[(v >> 6) & 63]);
        out.push_back(kBase64Chars[v & 63]);
        i += 3;
    }
    const std::size_t rem = size - i;
    if (rem == 1) {
        const std::uint32_t v = static_cast<std::uint32_t>(data[i]) << 16;
        out.push_back(kBase64Chars[(v >> 18) & 63]);
        out.push_back(kBase64Chars[(v >> 12) & 63]);
        out.push_back('=');
        out.push_back('=');
    } else if (rem == 2) {
        const std::uint32_t v = (static_cast<std::uint32_t>(data[i]) << 16) |
                                (static_cast<std::uint32_t>(data[i + 1]) << 8);
        out.push_back(kBase64Chars[(v >> 18) & 63]);
        out.push_back(kBase64Chars[(v >> 12) & 63]);
        out.push_back(kBase64Chars[(v >> 6) & 63]);
        out.push_back('=');
    }
    return out;
}

std::string Base64Encode(const std::vector<unsigned char>& data) {
    return Base64Encode(data.data(), data.size());
}

bool Base64Decode(std::string_view text, std::vector<unsigned char>& out) {
    out.clear();
    // Remove espaços em branco (o JSON pode quebrar linhas longas).
    std::string clean;
    clean.reserve(text.size());
    for (char c : text) {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') clean.push_back(c);
    }
    if (clean.size() % 4 != 0) return false;
    std::size_t pads = 0;
    if (!clean.empty()) {
        if (clean[clean.size() - 1] == '=') ++pads;
        if (clean.size() >= 2 && clean[clean.size() - 2] == '=') ++pads;
    }
    if (pads > 2) return false;
    out.reserve(clean.size() / 4 * 3);
    for (std::size_t i = 0; i < clean.size(); i += 4) {
        std::uint32_t v = 0;
        int padInGroup = 0;
        for (std::size_t k = 0; k < 4; ++k) {
            const char c = clean[i + k];
            if (c == '=') {
                // Padding só permitido no último grupo, nas duas últimas posições.
                if (i + 4 != clean.size() || k < 2) return false;
                ++padInGroup;
                v <<= 6;
            } else {
                if (padInGroup != 0) return false;  // caractere após '='
                const signed char d = Base64Value(c);
                if (d < 0) return false;
                v = (v << 6) | static_cast<std::uint32_t>(d);
            }
        }
        // Bytes reais deste grupo: 3 sem padding, 2 com um '=', 1 com dois '=='.
        const int take = 3 - padInGroup;
        if (take > 2) out.push_back(static_cast<unsigned char>((v >> 16) & 0xFFu));
        if (take > 1) out.push_back(static_cast<unsigned char>((v >> 8) & 0xFFu));
        // Com padding, os bits úteis estão ALINHADOS À ESQUERDA (os zeros vieram
        // dos '=' deslocados). Ex.: '/w==' -> v>>16 == 0xFF. Sem shift correto,
        // o último byte decodificado saía 0 — bug que quebrava round-trips e PNGs.
        if (take == 3) out.push_back(static_cast<unsigned char>(v & 0xFFu));
        else if (take == 2) out.push_back(static_cast<unsigned char>((v >> 8) & 0xFFu));
        else out.push_back(static_cast<unsigned char>((v >> 16) & 0xFFu));
    }
    return true;
}

void EncodePngRgb(const ImageRgb& image, std::vector<unsigned char>& outBytes) {
    outBytes.clear();
    if (image.Empty()) return;

    // IDAT: cada scanline prefixed with filtro 0, depois zlib "stored" (sem compressão).
    const std::size_t stride = static_cast<std::size_t>(image.width) * 3;
    std::vector<unsigned char> raw;
    raw.reserve(static_cast<std::size_t>(image.height) * (stride + 1));
    for (int y = 0; y < image.height; ++y) {
        raw.push_back(0);  // filtro None
        const std::size_t off = static_cast<std::size_t>(y) * stride;
        raw.insert(raw.end(), image.rgb.begin() + static_cast<std::ptrdiff_t>(off),
                   image.rgb.begin() + static_cast<std::ptrdiff_t>(off + stride));
    }

    // Envelope zlib: CMF/FLG (deflate, 32K window) + blocos stored + Adler32.
    std::vector<unsigned char> idat;
    idat.push_back(0x78);
    idat.push_back(0x01);
    std::size_t pos = 0;
    while (pos < raw.size() || raw.empty()) {
        const std::size_t block = std::min<std::size_t>(raw.size() - pos, 0xFFFFu);
        const bool finalBlock = (pos + block >= raw.size());
        idat.push_back(finalBlock ? 0x01 : 0x00);
        // LEN/NLEN little-endian (RFC 1951) — o decoder Stored() lê little-endian.
        AppendU16Le(idat, static_cast<std::uint16_t>(block));
        AppendU16Le(idat, static_cast<std::uint16_t>(~block & 0xFFFFu));
        idat.insert(idat.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos),
                    raw.begin() + static_cast<std::ptrdiff_t>(pos + block));
        pos += block;
        if (raw.empty()) break;
    }
    // Adler32 dos dados brutos.
    std::uint32_t a = 1, b = 0;
    for (unsigned char byte : raw) {
        a = (a + byte) % 65521u;
        b = (b + a) % 65521u;
    }
    AppendU32Be(idat, (b << 16) | a);

    // Assinatura PNG. '\\x1A' vira char negativo em plataformas onde char é signed;
    // o cast explícito evita a narrowing conversion (C++20) e mantém o byte 0x1A.
    outBytes.insert(outBytes.end(),
                    {static_cast<unsigned char>(0x89), 'P', 'N', 'G', '\r', '\n',
                     static_cast<unsigned char>(0x1A), '\n'});
    std::vector<unsigned char> ihdr;
    AppendU32Be(ihdr, static_cast<std::uint32_t>(image.width));
    AppendU32Be(ihdr, static_cast<std::uint32_t>(image.height));
    ihdr.push_back(8);   // profundidade
    ihdr.push_back(2);   // truecolor RGB
    ihdr.push_back(0);   // compressão
    ihdr.push_back(0);   // filtro
    ihdr.push_back(0);   // interlace
    AppendPngChunk(outBytes, "IHDR", ihdr);
    AppendPngChunk(outBytes, "IDAT", idat);
    AppendPngChunk(outBytes, "IEND", {});
}

WeaponImagesParseResult WeaponImagesFromJson(const std::string& jsonText,
                                             std::vector<WeaponImageEntry>& out) {
    WeaponImagesParseResult res;
    out.clear();
    const nlohmann::json j = nlohmann::json::parse(jsonText, nullptr, false);
    if (j.is_discarded() || !j.is_object()) {
        res.ok = false;
        res.warnings.push_back("weaponImages: JSON inválido");
        return res;
    }
    const auto it = j.find("weaponImages");
    if (it == j.end()) return res;  // ausente = sem imagens, sem aviso (perfis antigos)
    if (!it->is_array()) {
        res.ok = false;
        res.warnings.push_back("weaponImages: deveria ser uma lista");
        return res;
    }
    for (const nlohmann::json& item : *it) {
        if (!item.is_object() || !item.contains("weapon") || !item["weapon"].is_string() ||
            !item.contains("png") || !item["png"].is_string()) {
            res.warnings.push_back("weaponImages: entrada sem \"weapon\"/\"png\" ignorada");
            continue;
        }
        WeaponImageEntry e;
        e.weaponId = item["weapon"].get<std::string>();
        e.pngBase64 = item["png"].get<std::string>();
        // Arma desconhecida? Ignora com aviso (padrão do projeto para ids novos/velhos).
        bool known = false;
        for (std::size_t i = 0; i < kWeaponCount; ++i) {
            if (GetWeaponInfo(static_cast<Weapon>(i)).id == e.weaponId) {
                known = true;
                break;
            }
        }
        if (!known) {
            res.warnings.push_back("weaponImages: arma desconhecida \"" + e.weaponId + "\" ignorada");
            continue;
        }
        if (e.pngBase64.size() > kMaxWeaponImageB64) {
            res.warnings.push_back("weaponImages: imagem de \"" + e.weaponId +
                                   "\" acima do limite; removida");
            e.pngBase64.clear();
        }
        // Valida o PNG agora (falha aqui = campo limpo, não um erro do perfil inteiro).
        if (!e.pngBase64.empty()) {
            std::vector<unsigned char> bytes;
            ImageRgb img;
            if (!Base64Decode(e.pngBase64, bytes) || !DecodePng(bytes, img)) {
                res.warnings.push_back("weaponImages: PNG de \"" + e.weaponId +
                                       "\" inválido; imagem removida");
                e.pngBase64.clear();
            }
        }
        out.push_back(std::move(e));
    }
    return res;
}

std::string WeaponImagesToJson(const std::vector<WeaponImageEntry>& entries) {
    nlohmann::ordered_json j;
    nlohmann::ordered_json arr = nlohmann::ordered_json::array();
    for (const WeaponImageEntry& e : entries) {
        if (e.pngBase64.empty()) continue;  // sem imagem = não grava
        nlohmann::ordered_json item;
        item["weapon"] = e.weaponId;
        item["png"] = e.pngBase64;
        arr.push_back(std::move(item));
    }
    j["weaponImages"] = std::move(arr);
    return j.dump();
}

bool EmbedWeaponImages(std::string& profileJsonText, const std::vector<WeaponImageEntry>& entries) {
    nlohmann::ordered_json j = nlohmann::ordered_json::parse(profileJsonText, nullptr, false);
    if (j.is_discarded() || !j.is_object()) {
        return false;  // texto do perfil inválido: não mexe em nada
    }
    nlohmann::ordered_json arr = nlohmann::ordered_json::array();
    for (const WeaponImageEntry& e : entries) {
        if (e.pngBase64.empty()) continue;  // sem imagem = não grava
        nlohmann::ordered_json item;
        item["weapon"] = e.weaponId;
        item["png"] = e.pngBase64;
        arr.push_back(std::move(item));
    }
    j["weaponImages"] = std::move(arr);
    profileJsonText = j.dump(2) + "\n";
    return true;
}

WeaponImagesParseResult ExtractWeaponImages(const std::string& profileJsonText,
                                            std::vector<WeaponImageEntry>& out) {
    // Reaproveita o parser da seção (mesma validação de arma/PNG/limite).
    return WeaponImagesFromJson(profileJsonText, out);
}

}  // namespace st
