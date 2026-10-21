// Testes de perfis: formato JSON, validação e armazenamento em disco.
#include "config/profile.h"
#include "config/history_json.h"
#include "config/profile_store.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void Check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FALHOU: %s\n", what);
        ++g_failures;
    }
}

std::string Replace(std::string s, const std::string& from, const std::string& to) {
    const auto pos = s.find(from);
    if (pos != std::string::npos) {
        s.replace(pos, from.size(), to);
    }
    return s;
}

void TestRoundTrip() {
    st::ProfileData d = st::DefaultProfile();
    st::ProfileData back;
    st::ParseResult r = st::ProfileFromJson(st::ProfileToJson(d), back);
    Check(r.ok && r.warnings.empty(), "perfil padrão lê sem erros nem avisos");
    Check(back == d, "perfil padrão volta igual");

    d.weapon = st::Weapon::M4A1S;
    d.sensitivity = 0.65f;
    d.fireButton = 1;
    d.scoreGoal = 85;
    d.scoreTolerance = 3.3f;
    d.patterns[1][7] = {-1.234f, 0.005f};
    d.overlayVisible = false;
    d.overlayCorner = 0;
    d.overlayScale = 1.25f;
    d.overlayOpacity = 0.5f;
    d.overlayRefreshMs = 100;
    d.accent = {41.0f / 255.0f, 121.0f / 255.0f, 1.0f};
    d.windowOpacity = 0.8f;
    d.uiScale = 1.25f;
    d.visualGuide = 2;
    d.visualGuideAhead = 5;
    d.visualPlayerColor = 1;
    d.visualLineWidth = 3.0f;
    d.visualGrid = false;
    d.visualShotNumbers = true;
    r = st::ProfileFromJson(st::ProfileToJson(d), back);
    Check(r.ok && back == d, "todos os campos sobrevivem a salvar e ler");

    const std::string json = st::ProfileToJson(d);
    Check(json.find("\"sensitivity\": 0.65,") != std::string::npos, "floats gravados de forma legível");
    Check(json.find("\"#2979FF\"") != std::string::npos, "cor gravada em #RRGGBB");
}

void TestValidation() {
    st::ProfileData out = st::DefaultProfile();
    const std::string good = st::ProfileToJson(st::DefaultProfile());

    Check(!st::ProfileFromJson("{ isso não é json", out).ok, "JSON quebrado é rejeitado");
    Check(!st::ProfileFromJson("[1,2,3]", out).ok, "JSON que não é objeto é rejeitado");
    Check(!st::ProfileFromJson(Replace(good, "\"SprayTrainer\"", "\"OutroApp\""), out).ok, "app errado é rejeitado");
    Check(!st::ProfileFromJson(Replace(good, "\"version\": 1", "\"version\": 99"), out).ok,
          "versão mais nova é rejeitada");
    nlohmann::json unknownWeapon = nlohmann::json::parse(good);
    unknownWeapon["training"]["weapon"] = "awp";
    Check(!st::ProfileFromJson(unknownWeapon.dump(), out).ok, "arma desconhecida é rejeitada");
    Check(!st::ProfileFromJson(Replace(good, "\"accent\": \"#9D4EDD\"", "\"accent\": \"roxo\""), out).ok,
          "cor fora do formato é rejeitada");
    Check(!st::ProfileFromJson(Replace(good, "\"sensitivity\": 1.35", "\"sensitivity\": \"alta\""), out).ok,
          "tipo errado é rejeitado");

    // Padrão com 30 pontos em vez de 31.
    nlohmann::json shortPattern = nlohmann::json::parse(good);
    shortPattern["patterns"]["m4a4"].erase(30);
    const st::ParseResult rs = st::ProfileFromJson(shortPattern.dump(), out);
    Check(!rs.ok && rs.error.find("31") != std::string::npos, "padrão com tamanho errado é rejeitado");

    // Ponto que não é [x, y].
    nlohmann::json badPoint = nlohmann::json::parse(good);
    badPoint["patterns"]["ak47"][3] = "x";
    Check(!st::ProfileFromJson(badPoint.dump(), out).ok, "ponto malformado é rejeitado");

    // Fora da faixa: ajusta e avisa.
    st::ProfileData clamped;
    const st::ParseResult rc =
        st::ProfileFromJson(Replace(good, "\"sensitivity\": 1.35", "\"sensitivity\": 500"), clamped);
    Check(rc.ok && clamped.sensitivity == 20.0f && !rc.warnings.empty(), "valor fora da faixa é ajustado com aviso");

    // Seção ausente: usa padrão e avisa.
    st::ProfileData partial;
    const st::ParseResult rp = st::ProfileFromJson(R"({"app":"SprayTrainer","version":1})", partial);
    Check(rp.ok && rp.warnings.size() >= 4 && partial == st::DefaultProfile(), "seções ausentes viram padrão");

    // Falha não altera o destino.
    st::ProfileData untouched = st::DefaultProfile();
    untouched.sensitivity = 3.0f;
    st::ProfileFromJson("{}", untouched);
    Check(untouched.sensitivity == 3.0f, "perfil inválido não altera os dados atuais");
}

void TestNames() {
    using S = st::ProfileStore;
    Check(!S::ValidateName("Meu perfil").has_value(), "nome comum é válido");
    Check(!S::ValidateName("Configuração ção").has_value(), "acentos são válidos");
    Check(S::ValidateName("").has_value(), "nome vazio é inválido");
    Check(S::ValidateName("a/b").has_value(), "barra é inválida");
    Check(S::ValidateName("fim.").has_value(), "ponto no fim é inválido");
    Check(S::ValidateName(" x").has_value(), "espaço no início é inválido");
    Check(S::ValidateName("con").has_value() && S::ValidateName("COM3").has_value(), "nomes reservados");
    Check(S::ValidateName(std::string(41, 'a')).has_value(), "mais de 40 caracteres é inválido");
    Check(!S::ValidateName(std::string(20, 'a') + "çççççççççççççççççççç").has_value(),
          "40 caracteres com acento contam como 40");

    Check(S::SanitizeName("a/b:c") == "a-b-c", "sanitiza caracteres proibidos");
    Check(S::SanitizeName("  nome.. ") == "nome", "sanitiza espaços e pontos nas pontas");
    Check(S::SanitizeName("NUL") == "Perfil importado", "sanitiza nome reservado");
    Check(!S::ValidateName(S::SanitizeName(std::string(60, 'x'))).has_value(), "sanitiza tamanho");
}

void TestStore(const fs::path& root) {
    st::ProfileStore store(root);
    Check(store.List().empty(), "pasta nova sem perfis");
    Check(store.LastProfile().empty(), "sem último perfil no início");

    st::ProfileData d = st::DefaultProfile();
    d.sensitivity = 2.5f;
    Check(store.Save("Principal", d).ok, "salvar perfil");
    Check(store.Save("Ação rápida", st::DefaultProfile()).ok, "salvar perfil com acento");
    Check(!store.Save("in/valido", d).ok, "salvar com nome inválido falha");
    Check(store.List().size() == 2 && store.Exists("Ação rápida"), "listar perfis");

    st::ProfileData loaded;
    Check(store.Load("Principal", loaded).ok && loaded == d, "carregar perfil salvo");

    Check(store.SetLastProfile("Principal").ok && store.LastProfile() == "Principal", "último perfil");
    Check(store.Rename("Principal", "Treino diário").ok, "renomear");
    Check(store.LastProfile() == "Treino diário", "renomear atualiza o último perfil");
    Check(!store.Rename("Treino diário", "Ação rápida").ok, "renomear para nome existente falha");
    Check(store.Duplicate("Treino diário", store.UniqueName("Treino diário")).ok, "duplicar");
    Check(store.Exists("Treino diário (2)"), "duplicar usa nome único");
    Check(store.Delete("Treino diário").ok && store.LastProfile().empty(), "excluir limpa o último perfil");
    Check(!store.Delete("não existe").ok, "excluir inexistente falha");

    // Arquivo corrompido: erro claro, sem exceção.
    std::ofstream(root / "profiles" / "Quebrado.json") << "{ \"app\": \"SprayTrainer\", ";
    st::ProfileData keep = d;
    const st::StoreResult bad = store.Load("Quebrado", keep);
    Check(!bad.ok && !bad.error.empty() && keep == d, "perfil corrompido falha sem alterar dados");

    // Exportar/importar arquivo avulso.
    const fs::path exported = root / "export" / "meu perfil.json";
    fs::create_directories(exported.parent_path());
    Check(st::ProfileStore::WriteProfileFile(exported, d).ok, "exportar");
    st::ProfileData imported;
    Check(st::ProfileStore::ReadProfileFile(exported, imported).ok && imported == d, "importar");
    Check(!fs::exists(exported.string() + ".tmp"), "gravação não deixa .tmp para trás");
}

void TestOptionalSections() {
    // Perfil da Etapa 5 (sem "visual" nem "interface.scale") abre sem avisos.
    nlohmann::json old = nlohmann::json::parse(st::ProfileToJson(st::DefaultProfile()));
    old.erase("visual");
    old["interface"].erase("scale");
    st::ProfileData d;
    const st::ParseResult r = st::ProfileFromJson(old.dump(), d);
    Check(r.ok && r.warnings.empty() && d == st::DefaultProfile(), "seções novas são opcionais e silenciosas");

    nlohmann::json bad = nlohmann::json::parse(st::ProfileToJson(st::DefaultProfile()));
    bad["visual"]["guide"] = "às vezes";
    Check(!st::ProfileFromJson(bad.dump(), d).ok, "valor desconhecido em visual é rejeitado");
}

void TestRepetitionSection() {
    // Round-trip completo das configurações de repetição (inclusive várias teclas de parar).
    st::ProfileData p = st::DefaultProfile();
    p.repetitionEnabled = true;
    p.repeatKeyVk = st::kVkLButton;
    p.stopKeysVk = {st::VirtualKeyFromName("X"), st::VirtualKeyFromName("F12")};
    p.repeatSubsteps = 5;
    p.repeatReturnBack = false;
    const std::string json = st::ProfileToJson(p);
    st::ProfileData back;
    const st::ParseResult r = st::ProfileFromJson(json, back);
    Check(r.ok && back == p, "seção repetition sobrevive ao save/load");

    // Perfil antigo sem a seção abre com os padrões, silenciosamente.
    nlohmann::json old = nlohmann::json::parse(st::ProfileToJson(st::DefaultProfile()));
    old.erase("repetition");
    st::ProfileData d;
    const st::ParseResult legacy = st::ProfileFromJson(old.dump(), d);
    Check(legacy.ok && legacy.warnings.empty() && d == st::DefaultProfile(),
          "perfis sem repetition usam o padrão sem avisos");

    // Tecla desconhecida ou lista acima do limite são erros claros.
    nlohmann::json bad = nlohmann::json::parse(json);
    bad["repetition"]["repeatKey"] = "NAOTECLA";
    Check(!st::ProfileFromJson(bad.dump(), d).ok, "tecla desconhecida em repetition é rejeitada");
    nlohmann::json many = nlohmann::json::parse(json);
    many["repetition"]["stopKeys"] = {"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L"};
    Check(!st::ProfileFromJson(many.dump(), d).ok, "mais de 11 teclas de parar é rejeitado");
}

st::BurstRecord SampleBurst(std::int64_t t, st::Weapon w, float score) {
    st::BurstRecord b;
    b.timestamp = t;
    b.weapon = w;
    b.score = score;
    b.meanError = 0.5f;
    b.goal = 70;
    b.ideal = {{0.0f, 0.0f}, {-0.04f, 0.65f}};
    b.player = {{0.0f, 0.0f}, {-0.1f, 0.6123f}};
    return b;
}

void TestHistory(const fs::path& root) {
    st::History h;
    for (int i = 0; i < 305; ++i) {
        h.Add(SampleBurst(i, i % 2 == 0 ? st::Weapon::AK47 : st::Weapon::M4A4, static_cast<float>(i % 100)));
    }
    Check(h.Entries().size() == st::History::kMaxEntries, "histórico limitado a 300");
    Check(h.Entries().front().timestamp == 5, "descarta as mais antigas");

    st::History small;
    small.Add(SampleBurst(1, st::Weapon::AK47, 50.0f));
    small.Add(SampleBurst(2, st::Weapon::M4A4, 90.0f));
    small.Add(SampleBurst(3, st::Weapon::AK47, 70.0f));
    const st::HistoryStats all = small.Stats(std::nullopt, 2);
    Check(all.count == 3 && all.best == 90.0f && all.mean == 70.0f && all.recentMean == 80.0f, "estatísticas gerais");
    const st::HistoryStats ak = small.Stats(st::Weapon::AK47, 10);
    Check(ak.count == 2 && ak.mean == 60.0f && ak.best == 70.0f, "estatísticas por arma");
    Check(small.Stats(st::Weapon::M4A1S).count == 0, "arma sem rajadas");

    // JSON: ida e volta, entradas ruins ignoradas.
    std::vector<st::BurstRecord> back;
    std::string error;
    Check(st::HistoryFromJson(st::HistoryToJson(small.Entries()), back, error) && back.size() == 3, "histórico ida e volta");
    Check(back[1].weapon == st::Weapon::M4A4 && back[1].player[1].y == 0.612f, "histórico guarda 3 casas");

    nlohmann::json j = nlohmann::json::parse(st::HistoryToJson(small.Entries()));
    j["bursts"][0]["score"] = 500;
    j["bursts"][1]["weapon"] = "awp";
    int skipped = 0;
    Check(st::HistoryFromJson(j.dump(), back, error, &skipped) && back.size() == 1 && skipped == 2,
          "entradas inválidas são puladas");
    Check(!st::HistoryFromJson("lixo", back, error), "histórico ilegível falha");

    // Armazenamento: arquivo inválido vira history.invalid.json.
    st::ProfileStore store(root);
    Check(store.SaveHistory(small.Entries()).ok, "salvar histórico");
    std::vector<st::BurstRecord> loaded;
    Check(store.LoadHistory(loaded).ok && loaded.size() == 3, "carregar histórico");
    std::ofstream(root / "history.json") << "{ quebrado";
    Check(!store.LoadHistory(loaded).ok && loaded.empty(), "histórico corrompido devolve erro");
    Check(fs::exists(root / "history.invalid.json") && !fs::exists(root / "history.json"),
          "histórico corrompido é preservado à parte");
    Check(store.LoadHistory(loaded).ok && loaded.empty(), "sem arquivo: histórico vazio");
}

// O perfil de fábrica tem que ser exatamente o "padrao.json" escolhido pelo usuário.
void TestFactoryDefaultsMatchUserProfile() {
    std::ifstream in(fs::path(ST_TEST_DATA_DIR) / "padrao.json", std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    Check(!text.empty(), "padrao.json encontrado");
    st::ProfileData userDefault;
    const st::ParseResult r = st::ProfileFromJson(text, userDefault);
    Check(r.ok && r.warnings.empty(), "padrao.json lê sem avisos");
    Check(userDefault == st::DefaultProfile(), "padrão de fábrica == padrao.json (todos os campos e as 13 armas)");
    const st::ProfileData d = st::DefaultProfile();
    Check(d.weapon == st::Weapon::Ump45 && d.sensitivity == 1.35f && d.scoreGoal == 60,
          "treino de fábrica: UMP-45, 1.35, meta 60");
}

}  // namespace

int main() {
    TestRoundTrip();
    TestValidation();
    TestNames();

    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() / ("spraytrainer_tests_" + std::to_string(stamp));
    TestStore(root);
    TestOptionalSections();
    TestRepetitionSection();
    TestHistory(root);
    TestFactoryDefaultsMatchUserProfile();
    std::error_code ec;
    fs::remove_all(root, ec);

    if (g_failures == 0) {
        std::printf("Todos os testes de perfil passaram.\n");
        return 0;
    }
    return 1;
}
