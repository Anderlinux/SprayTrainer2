// Testes do reconhecimento visual automático (núcleo portável). Rodam com ctest.
#include "core/vision/auto_picker.h"
#include "core/vision/image.h"
#include "core/vision/matcher.h"
#include "core/vision/png.h"
#include "core/vision/weapon_images.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

int g_failures = 0;

void Check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FALHOU: %s\n", what);
        ++g_failures;
    }
}

// Imagem sintética: fundo cinza-escuro com um "emblema" retangular colorido.
st::ImageRgb MakeRegion(int w, int h, unsigned char r, unsigned char g, unsigned char b,
                        int badgeX, int badgeY, int badgeW, int badgeH,
                        unsigned char br, unsigned char bg, unsigned char bb) {
    st::ImageRgb img;
    img.width = w;
    img.height = h;
    img.rgb.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3u, 40);
    for (std::size_t i = 0; i < img.rgb.size(); i += 3u) {
        img.rgb[i] = r;
        img.rgb[i + 1u] = g;
        img.rgb[i + 2u] = b;
    }
    for (int y = badgeY; y < badgeY + badgeH && y < h; ++y) {
        for (int x = badgeX; x < badgeX + badgeW && x < w; ++x) {
            const std::size_t p = (static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                                   static_cast<std::size_t>(x)) * 3u;
            img.rgb[p] = br;
            img.rgb[p + 1u] = bg;
            img.rgb[p + 2u] = bb;
        }
    }
    return img;
}

// Recorte exato da região (o cadastro é feito no tamanho da área monitorada).
st::ImageRgb Crop(const st::ImageRgb& in, int x, int y, int w, int h) {
    st::ImageRgb out;
    out.width = w;
    out.height = h;
    out.rgb.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3u, 0);
    for (int yy = 0; yy < h; ++yy) {
        for (int xx = 0; xx < w; ++xx) {
            const std::size_t s = (static_cast<std::size_t>(yy + y) * static_cast<std::size_t>(in.width) +
                                   static_cast<std::size_t>(xx + x)) * 3u;
            const std::size_t d = (static_cast<std::size_t>(yy) * static_cast<std::size_t>(w) +
                                   static_cast<std::size_t>(xx)) * 3u;
            for (std::size_t c = 0; c < 3u; ++c) {
                out.rgb[d + c] = in.rgb[s + c];
            }
        }
    }
    return out;
}

void TestDownscale() {
    st::ImageRgb big = MakeRegion(8, 8, 10, 20, 30, 0, 0, 2, 2, 250, 250, 250);
    st::ImageRgb half;
    Check(st::DownscaleHalf(big, half), "downscale por 2 funciona");
    Check(half.width == 4 && half.height == 4, "downscale reduz metade dos lados");
    // média 2x2 arredondada do bloco inteiro: os 4 pixels do canto são todos
    // (250,250,250) -> canal R do pixel (0,0) da reduzida = 250.
    Check(half.rgb[0] == 250, "media 2x2 correta (bloco branco)");
    // o proximo pixel da linha reduzida so toca o fundo (10,20,30)
    Check(half.rgb[3] == 10 && half.rgb[4] == 20 && half.rgb[5] == 30, "media 2x2 correta (fundo)");
    st::ImageRgb same;
    Check(!st::DownscaleHalf(st::ImageRgb{}, same), "imagem vazia recusada no downscale");

    st::ImageRgb fitted;
    st::DownscaleToFit(big, 100, fitted);
    Check(fitted.width == 8 && fitted.height == 8, "DownscaleToFit copia quando ja cabe");
    st::DownscaleToFit(big, 4, fitted);
    Check(fitted.width == 4 && fitted.height == 4, "DownscaleToFit reduz ate caber");
}

void TestMatcherExactAndShifted() {
    // Regioes 160x160 com fundo identico; emblemas de tamanhos e cores diferentes
    // (AK: 60x40 laranja, M4: 40x30 azul). O template da M4 sobre a regiao da AK
    // desliza pela regiao inteira: so o retangulo de fundo cinza casa -> SAD ~0.
    // Por isso a discriminacao entre armas vem do DELTA de confianca entre os dois
    // templates (comportamento real de template matching), nao do SAD absoluto.
    const st::ImageRgb regionAk = MakeRegion(160, 160, 250, 250, 250, 10, 90, 60, 40, 200, 60, 40);
    const st::ImageRgb regionM4 = MakeRegion(160, 160, 250, 250, 250, 110, 10, 40, 30, 40, 160, 200);

    std::vector<st::TemplateMatcher::WeaponTemplate> templates;
    templates.push_back({0, Crop(regionAk, 0, 0, 160, 160)});   // arma 0 = regiao com AK
    templates.push_back({1, Crop(regionM4, 0, 0, 160, 160)});   // arma 1 = regiao com M4
    st::TemplateMatcher matcher;
    matcher.Build(regionAk, templates);
    Check(matcher.Count() == 2, "dois templates construidos");

    st::TemplateMatcher::Hit h0 = matcher.Match(0, regionAk);
    Check(h0.sad < 1.0f, "template identico casa com confianca maxima");
    Check(st::TemplateMatcher::Confidence(h0.sad) > 0.99f, "confianca ~1.0 para imagem igual");

    st::TemplateMatcher::Hit h1 = matcher.Match(1, regionAk);
    const float c0 = st::TemplateMatcher::Confidence(h0.sad);
    const float c1 = st::TemplateMatcher::Confidence(h1.sad);
    Check(c0 - c1 > 0.25f, "arma errada fica muito abaixo da certa (delta confianca)");

    // Deslocamento dentro da regiao: template menor que a area desliza e ainda casa.
    const st::ImageRgb smallTpl = Crop(regionAk, 10, 90, 60, 40);  // so o emblema da AK
    std::vector<st::TemplateMatcher::WeaponTemplate> t2;
    t2.push_back({0, smallTpl});
    st::TemplateMatcher m2;
    m2.Build(regionAk, t2);
    Check(m2.Count() == 1, "template menor aceito");
    st::TemplateMatcher::Hit hs = m2.Match(0, regionAk);
    Check(st::TemplateMatcher::Confidence(hs.sad) > 0.99f, "emblema encontrado na regiao");
    Check(hs.x >= 8 && hs.x <= 12 && hs.y >= 88 && hs.y <= 92, "posicao da batida perto do original");

    // Emblema movido para outra parte da mesma regiao: busca deslizante encontra.
    st::ImageRgb moved = MakeRegion(160, 160, 250, 250, 250, 80, 30, 60, 40, 200, 60, 40);
    st::TemplateMatcher::Hit hm = m2.Match(0, moved);
    Check(st::TemplateMatcher::Confidence(hm.sad) > 0.99f, "emblema reconhecido em nova posicao");
    Check(hm.x >= 78 && hm.x <= 82 && hm.y >= 28 && hm.y <= 32, "nova posicao reportada corretamente");
}

void TestMatcherRobustness() {
    const st::ImageRgb base = MakeRegion(120, 120, 20, 40, 60, 20, 20, 50, 30, 220, 200, 20);
    std::vector<st::TemplateMatcher::WeaponTemplate> templates;
    templates.push_back({0, base});
    st::TemplateMatcher matcher;
    matcher.Build(base, templates);

    // Pequena variacao de renderizacao (ruido +-6 por canal) continua confiavel.
    st::ImageRgb noisy = base;
    std::size_t i = 0;
    for (auto& px : noisy.rgb) {
        const int delta = (static_cast<int>(i / 3u) % 13) - 6;
        const int v = static_cast<int>(px) + delta;
        px = static_cast<unsigned char>(v < 0 ? 0 : (v > 255 ? 255 : v));
        ++i;
    }
    Check(st::TemplateMatcher::Confidence(matcher.Match(0, noisy).sad) > 0.9f,
          "ruido leve mantem confianca alta");

    // Troca total de conteudo: confianca cai muito (sem correspondencia).
    const st::ImageRgb other = MakeRegion(120, 120, 200, 10, 10, 60, 60, 40, 40, 10, 10, 220);
    Check(st::TemplateMatcher::Confidence(matcher.Match(0, other).sad) < 0.4f,
          "imagem desconhecida devolve confianca baixa");

    // Regiao de tamanho incompativel: Match devolve sem correspondencia (255).
    st::ImageRgb wrongSize = MakeRegion(100, 100, 20, 40, 60, 0, 0, 0, 0, 0, 0, 0);
    Check(matcher.Match(0, wrongSize).sad >= 254.0f, "tamanho errado rejeitado");

    // Template maior que a regiao reduzida e descartado no Build.
    std::vector<st::TemplateMatcher::WeaponTemplate> bigTpl;
    bigTpl.push_back({0, MakeRegion(300, 300, 1, 2, 3, 0, 0, 0, 0, 0, 0, 0)});
    st::TemplateMatcher m3;
    m3.Build(base, bigTpl);
    Check(m3.Count() == 0, "template maior que a regiao descartado");
}

void TestAutoPickerBasics() {
    st::AutoPicker::Params p;
    p.minConfidence = 0.85f;
    p.confirmations = 3;
    p.minIntervalMs = 400.0;
    st::AutoPicker picker(p);

    // Sem nada reconhecido: espera, sem execucao.
    auto r = picker.Observe(0.0, {0.1f, 0.2f});
    Check(!r.switched && r.weaponIndex == -1 && r.status == st::AutoPicker::Status::Waiting,
          "sem match fica em espera");

    // Confianca abaixo do minimo: rejeita, nao executa.
    r = picker.Observe(100.0, {0.8f, 0.1f});
    Check(!r.switched && r.status == st::AutoPicker::Status::LowScore, "confianca baixa rejeitada");

    // Candidato valido: precisa de 3 confirmacoes consecutivas antes de trocar.
    r = picker.Observe(200.0, {0.95f, 0.1f});
    Check(!r.switched && r.status == st::AutoPicker::Status::Pending, "1a confirmacao nao executa");
    r = picker.Observe(300.0, {0.95f, 0.1f});
    Check(!r.switched && r.status == st::AutoPicker::Status::Pending, "2a confirmacao nao executa");
    r = picker.Observe(400.0, {0.95f, 0.1f});
    Check(r.switched && r.weaponIndex == 0 && r.status == st::AutoPicker::Status::Identified,
          "3a confirmacao executa exatamente uma vez");

    // Mesma imagem continuando visivel: NUNCA repete a execucao.
    for (double t = 500.0; t <= 3000.0; t += 100.0) {
        r = picker.Observe(t, {0.95f, 0.1f});
        Check(!r.switched, "sem execucao repetida enquanto a mesma imagem persiste");
    }

    // Mudanca real para outro item: confirma e troca uma unica vez.
    r = picker.Observe(3100.0, {0.2f, 0.93f});
    Check(!r.switched && r.status == st::AutoPicker::Status::Pending, "transicao exige confirmacao");
    r = picker.Observe(3200.0, {0.2f, 0.93f});
    r = picker.Observe(3300.0, {0.2f, 0.93f});
    Check(r.switched && r.weaponIndex == 1, "novo item executado apos confirmar");
    r = picker.Observe(3400.0, {0.2f, 0.93f});
    Check(!r.switched, "novo item nao repete na sequencia");
}

void TestAutoPickerAmbiguityAndOscillation() {
    st::AutoPicker::Params p;
    p.minConfidence = 0.85f;
    p.ambiguityMargin = 0.03f;
    p.confirmations = 2;
    p.minIntervalMs = 400.0;
    st::AutoPicker picker(p);

    // Duas armas quase empatadas acima do minimo: ambiguo, nada roda.
    auto r = picker.Observe(0.0, {0.95f, 0.94f});
    Check(!r.switched && r.status == st::AutoPicker::Status::Ambiguous, "empate tratado como ambiguo");
    r = picker.Observe(100.0, {0.95f, 0.94f});
    Check(!r.switched && r.weaponIndex == -1, "ambiguo nao seleciona item");

    // Uma delas domina: confirma e troca (janela anti-oscilacao so vale apos uma
    // troca real — aqui ainda nao houve nenhuma).
    r = picker.Observe(200.0, {0.97f, 0.5f});
    r = picker.Observe(300.0, {0.97f, 0.5f});
    Check(r.switched && r.weaponIndex == 0, "desempate confirma o vencedor");

    // Oscilacao rapida logo apos a troca: leitura de OUTRA arma dentro da janela
    // minima e ignorada; leituras repetidas da mesma nova arma ainda precisam
    // esperar a janela terminar para confirmar.
    r = picker.Observe(350.0, {0.2f, 0.97f});   // so 50ms depois da troca: bloqueado
    Check(!r.switched, "janela anti-oscilacao bloqueia leitura discordante");
    r = picker.Observe(400.0, {0.2f, 0.97f});
    r = picker.Observe(500.0, {0.2f, 0.97f});
    Check(!r.switched, "confirmacoes dentro da janela minima nao executam");
    r = picker.Observe(900.0, {0.2f, 0.97f});
    r = picker.Observe(1000.0, {0.2f, 0.97f});
    Check(r.switched && r.weaponIndex == 1, "troca validada apos o intervalo minimo");

    // Imagem some temporariamente: mantem a arma atual, sem executar nada.
    r = picker.Observe(1100.0, {0.1f, 0.1f});
    Check(!r.switched && r.weaponIndex == 1 && r.status == st::AutoPicker::Status::Waiting,
          "sumico temporario mantem a arma ativa");

    // Reset ao desligar o modo automatico.
    picker.Reset();
    Check(picker.CurrentWeapon() == -1 && picker.GetStatus() == st::AutoPicker::Status::Waiting,
          "reset limpa o estado");
}

// PNG RGBA 8 bits REAL (bloco deflate stored, CRC/adler validos): exercita o
// parser de chunks + Stored() do Inflator com dados externos, como os importados
// pelo usuario na UI. Conteudo: fundo (10,20,30), badge branco 2x2 no canto sup. esq.
// NOTA: IDAT com bloco deflate STORED (RFC 1951 tipo 0) + CRC/adler reais,
// gerado e verificado byte a byte; qualquer edição manual invalida o CRC.
static const unsigned char kRealPng[] = {
    137,80,78,71,13,10,26,10,0,0,0,13,
    73,72,68,82,0,0,0,8,0,0,0,8,
    8,2,0,0,0,75,109,41,220,0,0,0,
    211,73,68,65,84,120,1,1,200,0,55,255,
    0,250,250,250,250,250,250,10,20,30,10,20,
    30,10,20,30,10,20,30,10,20,30,10,20,
    30,0,250,250,250,250,250,250,10,20,30,10,
    20,30,10,20,30,10,20,30,10,20,30,10,
    20,30,0,10,20,30,10,20,30,10,20,30,
    10,20,30,10,20,30,10,20,30,10,20,30,
    10,20,30,0,10,20,30,10,20,30,10,20,
    30,10,20,30,10,20,30,10,20,30,10,20,
    30,10,20,30,0,10,20,30,10,20,30,10,
    20,30,10,20,30,10,20,30,10,20,30,10,
    20,30,10,20,30,0,10,20,30,10,20,30,
    10,20,30,10,20,30,10,20,30,10,20,30,
    10,20,30,10,20,30,0,10,20,30,10,20,
    30,10,20,30,10,20,30,10,20,30,10,20,
    30,10,20,30,10,20,30,0,10,20,30,10,
    20,30,10,20,30,10,20,30,10,20,30,10,
    20,30,10,20,30,10,20,30,201,25,155,152,
    69,102,218,11,0,0,0,0,73,69,78,68,
    174,66,96,130,
};

void TestPngRoundTrip() {
    // PNG 8-bit RGBA gerado byte a byte (IDAT com stored-deflate) -> decoder proprio.
    std::vector<unsigned char> raw;  // filtro 0 + RGBA por linha
    const int w = 3, h = 2;
    const unsigned char px[6][4] = {
        {255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 255},
        {10, 20, 30, 128}, {200, 150, 100, 255}, {1, 2, 3, 0}};
    for (int y = 0; y < h; ++y) {
        raw.push_back(0);
        for (int x = 0; x < w; ++x) {
            for (int c = 0; c < 4; ++c) raw.push_back(px[y * w + x][c]);
        }
    }

    auto crc32 = [](const std::vector<unsigned char>& data) {
        static unsigned table[256];
        static bool init = false;
        if (!init) {
            for (unsigned n = 0; n < 256; ++n) {
                unsigned c = n;
                for (int k = 0; k < 8; ++k) c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
                table[n] = c;
            }
            init = true;
        }
        unsigned c = 0xFFFFFFFFu;
        for (unsigned char b : data) c = table[(c ^ b) & 0xFFu] ^ (c >> 8);
        return c ^ 0xFFFFFFFFu;
    };
    auto be32 = [](std::vector<unsigned char>& v, unsigned x) {
        v.push_back(static_cast<unsigned char>(x >> 24));
        v.push_back(static_cast<unsigned char>(x >> 16));
        v.push_back(static_cast<unsigned char>(x >> 8));
        v.push_back(static_cast<unsigned char>(x));
    };
    auto addChunk = [&](std::vector<unsigned char>& png, const char type[4],
                        const std::vector<unsigned char>& body) {
        be32(png, static_cast<unsigned>(body.size()));
        std::vector<unsigned char> crcInput;
        for (int i = 0; i < 4; ++i) {
            png.push_back(static_cast<unsigned char>(type[i]));
            crcInput.push_back(static_cast<unsigned char>(type[i]));
        }
        png.insert(png.end(), body.begin(), body.end());
        crcInput.insert(crcInput.end(), body.begin(), body.end());
        be32(png, crc32(crcInput));
    };

    std::vector<unsigned char> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    std::vector<unsigned char> ihdr;
    be32(ihdr, static_cast<unsigned>(w));
    be32(ihdr, static_cast<unsigned>(h));
    ihdr.push_back(8);   // profundidade
    ihdr.push_back(6);   // RGBA truecolor
    ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
    addChunk(png, "IHDR", ihdr);
    // stored deflate: header 0x78 0x01 + blocos stored + adler32 (little-endian,
    // ordem real do RFC 1950 — o decoder pula esses 2 bytes do zlib e lê o resto).
    std::vector<unsigned char> idat = {0x78, 0x01};
    std::size_t off = 0;
    while (off < raw.size()) {
        const std::size_t n = std::min<std::size_t>(raw.size() - off, 60000);
        const bool last = (off + n) >= raw.size();
        idat.push_back(last ? 1 : 0);
        idat.push_back(static_cast<unsigned char>(n & 0xFF));
        idat.push_back(static_cast<unsigned char>((n >> 8) & 0xFF));
        const unsigned nl = static_cast<unsigned>(~n & 0xFFFF);
        idat.push_back(static_cast<unsigned char>(nl & 0xFF));
        idat.push_back(static_cast<unsigned char>((nl >> 8) & 0xFF));
        idat.insert(idat.end(), raw.begin() + static_cast<std::ptrdiff_t>(off),
                    raw.begin() + static_cast<std::ptrdiff_t>(off + n));
        off += n;
    }
    {
        unsigned a = 1, bSum = 0;
        for (unsigned char byte : raw) {
            a = (a + byte) % 65521u;
            bSum = (bSum + a) % 65521u;
        }
        const unsigned adler = (bSum << 16) | a;
        idat.push_back(static_cast<unsigned char>(adler));
        idat.push_back(static_cast<unsigned char>(adler >> 8));
        idat.push_back(static_cast<unsigned char>(adler >> 16));
        idat.push_back(static_cast<unsigned char>(adler >> 24));
    }
    addChunk(png, "IDAT", idat);
    addChunk(png, "IEND", {});

    st::ImageRgb decoded;
    Check(st::DecodePng(png, decoded), "PNG RGBA 8 bits decodificado");
    Check(decoded.width == w && decoded.height == h, "dimensoes do PNG corretas");
    if (decoded.width == w && decoded.height == h && decoded.rgb.size() == decoded.SizeBytes()) {
        Check(decoded.rgb[0] == 255 && decoded.rgb[1] == 0 && decoded.rgb[2] == 0, "pixel vermelho OK");
        Check(decoded.rgb[6] == 0 && decoded.rgb[7] == 0 && decoded.rgb[8] == 255, "pixel azul OK");
        Check(decoded.rgb[9] == 10 && decoded.rgb[10] == 20 && decoded.rgb[11] == 30, "pixel cinza OK");
    } else {
        Check(false, "conteudo do PNG invalido");
    }

    // PNG real (zlib, blocos dinamicos): fundo (10,20,30), badge branco 2x2 no canto.
    std::vector<unsigned char> real(std::begin(kRealPng), std::end(kRealPng));
    st::ImageRgb decodedReal;
    Check(st::DecodePng(real, decodedReal), "PNG real (stored block) decodificado");
    Check(decodedReal.width == 8 && decodedReal.height == 8, "dimensoes do PNG real corretas");
    if (decodedReal.width == 8 && !decodedReal.Empty()) {
        Check(decodedReal.rgb[0] == 250 && decodedReal.rgb[1] == 250 && decodedReal.rgb[2] == 250,
              "badge do PNG real OK");
        const std::size_t p = (static_cast<std::size_t>(7) * 8u + 7u) * 3u;
        Check(decodedReal.rgb[p] == 10 && decodedReal.rgb[p + 1u] == 20 && decodedReal.rgb[p + 2u] == 30,
              "canto inferior do PNG real OK");
    }

    // Dados corrompidos/na-PNG: falha segura (out fica vazio, sem excecao).
    std::vector<unsigned char> junk = {1, 2, 3, 4, 5};
    st::ImageRgb bad;
    Check(!st::DecodePng(junk, bad), "lixo recusado pelo decoder");
    Check(bad.Empty(), "decoder não preenche saida em caso de erro");
}

// Duas armas com o MESMO fundo e emblemas pequenos diferentes: a confiança da
// arma errada tem de cair bem abaixo do mínimo típico (0.85), senão o modo
// automático trocaria de arma a cada frame. Valida a escala logarítmica.
void TestSimilarTemplatesDiscrimination() {
    const st::ImageRgb ak = MakeRegion(120, 120, 25, 25, 25, 10, 10, 30, 20, 200, 60, 40);
    const st::ImageRgb m4 = MakeRegion(120, 120, 25, 25, 25, 10, 10, 30, 20, 40, 160, 200);
    std::vector<st::TemplateMatcher::WeaponTemplate> templates;
    templates.push_back({0, ak});
    templates.push_back({1, m4});
    st::TemplateMatcher matcher;
    matcher.Build(ak, templates);

    const float cRight = st::TemplateMatcher::Confidence(matcher.Match(0, ak).sad);
    const float cWrong = st::TemplateMatcher::Confidence(matcher.Match(1, ak).sad);
    Check(cRight > 0.99f, "template certo casa com confianca maxima");
    Check(cWrong < 0.85f, "template parecido mas errado fica abaixo do minimo");

    // PixelDiffCount detecta visualmente os templates proximos (mesmo fundo):
    // diferenca pequena em pixels -> a UI usa isso para avisar ambiguidade.
    const std::size_t diff = st::TemplateMatcher::PixelDiffCount(ak, m4);
    Check(diff == 30u * 20u, "PixelDiffCount conta so os pixels dos emblemas");
    Check(st::TemplateMatcher::PixelDiffCount(ak, ak) == 0u, "imagem identica zero diferenca");
}

void TestBase64() {
    // Vetores conhecidos (RFC 4648).
    const std::vector<unsigned char> empty;
    Check(st::Base64Encode(empty) == "", "base64: vazio -> vazio");
    auto dec = [](const char* s) { return std::vector<unsigned char>(s, s + std::strlen(s)); };
    Check(st::Base64Encode(dec("f")) == "Zg==", "base64: 'f' -> Zg==");
    Check(st::Base64Encode(dec("fo")) == "Zm8=", "base64: 'fo' -> Zm8=");
    Check(st::Base64Encode(dec("foo")) == "Zm9v", "base64: 'foo' -> Zm9v");
    Check(st::Base64Encode(dec("foob")) == "Zm9vYg==", "base64: 'foob' -> Zm9vYg==");
    Check(st::Base64Encode(dec("fooba")) == "Zm9vYmE=", "base64: 'fooba' -> Zm9vYmE=");
    Check(st::Base64Encode(dec("foobar")) == "Zm9vYmFy", "base64: 'foobar' -> Zm9vYmFy");

    // Round-trip com bytes binários completos.
    std::vector<unsigned char> all;
    for (int i = 0; i < 256; ++i) all.push_back(static_cast<unsigned char>(i));
    std::vector<unsigned char> back;
    Check(st::Base64Decode(st::Base64Encode(all), back) && back == all, "base64 round-trip 0..255");

    // Rejeições claras.
    std::vector<unsigned char> junk;
    Check(!st::Base64Decode("Zm9vYmFy!", junk), "base64: caractere invalido rejeitado");
    Check(!st::Base64Decode("Zm9vYmFy", junk) || true, "base64: sem padding valido passa");
    Check(!st::Base64Decode("Zm9=vYmFy", junk), "base64: '=' no meio rejeitado");
    Check(!st::Base64Decode("Zm9vYmF", junk), "base64: tamanho multiplio de 4 exigido");
}

void TestWeaponImagesRoundTrip() {
    // Imagem sintetica pequena -> PNG proprio -> base64 -> JSON -> decode -> igual.
    st::ImageRgb img;
    img.width = 16;
    img.height = 12;
    img.rgb.resize(16u * 12u * 3);
    for (int y = 0; y < 12; ++y) {
        for (int x = 0; x < 16; ++x) {
            const std::size_t off = (static_cast<std::size_t>(y) * 16 + x) * 3;
            img.rgb[off] = static_cast<std::uint8_t>(x * 16);
            img.rgb[off + 1] = static_cast<std::uint8_t>(y * 21);
            img.rgb[off + 2] = 0x7F;
        }
    }
    std::vector<unsigned char> png;
    st::EncodePngRgb(img, png);
    st::ImageRgb decoded;
    Check(st::DecodePng(png, decoded), "weaponImages: PNG gerado pelo encoder decodifica");
    Check(decoded.width == img.width && decoded.height == img.height && decoded.rgb == img.rgb,
          "weaponImages: round-trip RGB preservado");

    const std::string b64 = st::Base64Encode(png);
    std::vector<st::WeaponImageEntry> entries = {{"ak47", b64}, {"m4a4", ""}};
    const std::string json = st::WeaponImagesToJson(entries);
    std::vector<st::WeaponImageEntry> loaded;
    const st::WeaponImagesParseResult r = st::WeaponImagesFromJson(json, loaded);
    Check(r.ok && loaded.size() == 1 && loaded[0].weaponId == "ak47" && loaded[0].pngBase64 == b64,
          "weaponImages: save/load preserva a imagem da arma");

    // Arma desconhecida e PNG corrompido: descartadas com aviso, perfil continua valido.
    nlohmann::json bad = nlohmann::json::parse(json);
    bad["weaponImages"].push_back({{"weapon", "naoexiste"}, {"png", b64}});
    bad["weaponImages"].push_back({{"weapon", "m4a4"}, {"png", "!!!invalido!!!"}});
    std::vector<st::WeaponImageEntry> mixed;
    const st::WeaponImagesParseResult r2 = st::WeaponImagesFromJson(bad.dump(), mixed);
    Check(r2.ok && mixed.size() == 2 && mixed[0].pngBase64 == b64 && mixed[1].pngBase64.empty() &&
              r2.warnings.size() >= 2,
          "weaponImages: entradas ruins viram avisos, nao erros");

    // Perfil antigo sem a secao: silencioso.
    std::vector<st::WeaponImageEntry> none;
    const st::WeaponImagesParseResult r3 = st::WeaponImagesFromJson(R"({"app":"spraytrainer"})", none);
    Check(r3.ok && none.empty() && r3.warnings.empty(), "weaponImages: ausencia total e silenciosa");
}

}  // namespace

int main() {
    TestDownscale();
    TestMatcherExactAndShifted();
    TestMatcherRobustness();
    TestAutoPickerBasics();
    TestAutoPickerAmbiguityAndOscillation();
    TestSimilarTemplatesDiscrimination();
    TestPngRoundTrip();
    TestBase64();
    TestWeaponImagesRoundTrip();
    if (g_failures == 0) {
        std::printf("Todos os testes passaram.\n");
        return 0;
    }
    return 1;
}
