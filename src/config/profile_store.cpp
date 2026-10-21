#include "config/profile_store.h"

#include "config/history_json.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <memory>

namespace st {

namespace fs = std::filesystem;

namespace {

constexpr std::size_t kMaxNameChars = 40;
constexpr std::uintmax_t kMaxFileBytes = 1024 * 1024;  // perfis reais têm poucos KB
constexpr const char* kForbidden = "<>:\"/\\|?*";

std::size_t CountCodepoints(const std::string& s) {
    return static_cast<std::size_t>(
        std::count_if(s.begin(), s.end(), [](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; }));
}

bool IsForbidden(char c) {
    const auto u = static_cast<unsigned char>(c);
    return u < 0x20 || u == 0x7F || std::string_view(kForbidden).find(c) != std::string_view::npos;
}

std::string Upper(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

bool IsReservedWindowsName(const std::string& name) {
    const std::string u = Upper(name);
    if (u == "CON" || u == "PRN" || u == "AUX" || u == "NUL") {
        return true;
    }
    return u.size() == 4 && (u.rfind("COM", 0) == 0 || u.rfind("LPT", 0) == 0) && u[3] >= '1' && u[3] <= '9';
}

struct FileCloser {
    void operator()(std::FILE* f) const { std::fclose(f); }
};
using FilePtr = std::unique_ptr<std::FILE, FileCloser>;

// Abre pelo caminho nativo. No Windows usa a API "larga" (UTF-16): nomes com acento
// funcionam em qualquer compilador (o fstream do MinGW não aceita caminhos não ASCII).
FilePtr OpenFile(const fs::path& path, const char* mode) {
#ifdef _WIN32
    const std::wstring wmode(mode, mode + std::strlen(mode));
    std::FILE* f = nullptr;
#ifdef _MSC_VER
    if (_wfopen_s(&f, path.c_str(), wmode.c_str()) != 0) {
        f = nullptr;
    }
#else
    f = _wfopen(path.c_str(), wmode.c_str());
#endif
    return FilePtr(f);
#else
    return FilePtr(std::fopen(path.c_str(), mode));
#endif
}

StoreResult ReadText(const fs::path& path, std::string& out) {
    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    if (ec) {
        return StoreResult::Fail("arquivo não encontrado ou sem acesso");
    }
    if (size > kMaxFileBytes) {
        return StoreResult::Fail("arquivo grande demais para ser um perfil");
    }
    const FilePtr in = OpenFile(path, "rb");
    if (!in) {
        return StoreResult::Fail("não foi possível abrir o arquivo");
    }
    out.resize(static_cast<std::size_t>(size));
    const std::size_t read = out.empty() ? 0 : std::fread(out.data(), 1, out.size(), in.get());
    if (read != out.size()) {
        return StoreResult::Fail("erro ao ler o arquivo");
    }
    return {};
}

// Grava num .tmp e renomeia por cima: um crash no meio não deixa o perfil pela metade.
StoreResult WriteTextAtomic(const fs::path& path, const std::string& text) {
    fs::path tmp = path;
    tmp += ".tmp";
    {
        const FilePtr out = OpenFile(tmp, "wb");
        if (!out) {
            return StoreResult::Fail("não foi possível gravar em " + ProfileStore::PathToUtf8(path.parent_path()));
        }
        const bool written = std::fwrite(text.data(), 1, text.size(), out.get()) == text.size() &&
                             std::fflush(out.get()) == 0;
        if (!written) {
            return StoreResult::Fail("erro ao gravar o arquivo (disco cheio?)");
        }
    }
    std::error_code ec;
    fs::rename(tmp, path, ec);
    if (ec) {
        fs::remove(tmp, ec);
        return StoreResult::Fail("não foi possível substituir o arquivo do perfil");
    }
    return {};
}

}  // namespace

ProfileStore::ProfileStore(fs::path root) : root_(std::move(root)) {}

fs::path ProfileStore::PathFromUtf8(const std::string& utf8) {
    return fs::path(std::u8string(utf8.begin(), utf8.end()));
}

std::string ProfileStore::PathToUtf8(const fs::path& path) {
    const std::u8string s = path.u8string();
    return std::string(s.begin(), s.end());
}

fs::path ProfileStore::FileFor(const std::string& name) const {
    return ProfilesDir() / PathFromUtf8(name + ".json");
}

StoreResult ProfileStore::EnsureDirs() const {
    std::error_code ec;
    fs::create_directories(ProfilesDir(), ec);
    if (ec) {
        return StoreResult::Fail("não foi possível criar a pasta " + PathToUtf8(ProfilesDir()));
    }
    return {};
}

std::vector<std::string> ProfileStore::List() const {
    std::vector<std::string> names;
    std::error_code ec;
    for (fs::directory_iterator it(ProfilesDir(), ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec) && it->path().extension() == ".json") {
            names.push_back(PathToUtf8(it->path().stem()));
        }
    }
    std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) { return Upper(a) < Upper(b); });
    return names;
}

bool ProfileStore::Exists(const std::string& name) const {
    std::error_code ec;
    return fs::is_regular_file(FileFor(name), ec);
}

StoreResult ProfileStore::Load(const std::string& name, ProfileData& out, std::vector<std::string>* warnings) const {
    return ReadProfileFile(FileFor(name), out, warnings);
}

StoreResult ProfileStore::Save(const std::string& name, const ProfileData& data) const {
    if (const auto err = ValidateName(name)) {
        return StoreResult::Fail(*err);
    }
    if (StoreResult r = EnsureDirs(); !r.ok) {
        return r;
    }
    return WriteTextAtomic(FileFor(name), ProfileToJson(data));
}

StoreResult ProfileStore::Delete(const std::string& name) const {
    std::error_code ec;
    if (!fs::remove(FileFor(name), ec) || ec) {
        return StoreResult::Fail("não foi possível excluir o perfil \"" + name + "\"");
    }
    if (LastProfile() == name) {
        SetLastProfile("");
    }
    return {};
}

StoreResult ProfileStore::Rename(const std::string& from, const std::string& to) const {
    if (from == to) {
        return {};
    }
    if (const auto err = ValidateName(to)) {
        return StoreResult::Fail(*err);
    }
    std::error_code ec;
    const fs::path src = FileFor(from);
    const fs::path dst = FileFor(to);
    // Em disco que não diferencia maiúsculas, "perfil" -> "Perfil" aponta para o mesmo arquivo.
    const bool sameFile = fs::exists(dst, ec) && fs::equivalent(src, dst, ec);
    if (Exists(to) && !sameFile) {
        return StoreResult::Fail("já existe um perfil chamado \"" + to + "\"");
    }
    fs::rename(src, dst, ec);
    if (ec) {
        return StoreResult::Fail("não foi possível renomear o perfil");
    }
    if (LastProfile() == from) {
        SetLastProfile(to);
    }
    return {};
}

StoreResult ProfileStore::Duplicate(const std::string& from, const std::string& to) const {
    if (Exists(to)) {
        return StoreResult::Fail("já existe um perfil chamado \"" + to + "\"");
    }
    ProfileData data;
    if (StoreResult r = Load(from, data); !r.ok) {
        return r;
    }
    return Save(to, data);
}

std::string ProfileStore::LastProfile() const {
    std::string text;
    if (!ReadText(root_ / "settings.json", text).ok) {
        return {};
    }
    const nlohmann::json j = nlohmann::json::parse(text, nullptr, false);
    if (!j.is_object()) {
        return {};
    }
    const auto it = j.find("lastProfile");
    return it != j.end() && it->is_string() ? it->get<std::string>() : std::string();
}

StoreResult ProfileStore::SetLastProfile(const std::string& name) const {
    std::error_code ec;
    fs::create_directories(root_, ec);
    nlohmann::ordered_json j;
    j["lastProfile"] = name;
    return WriteTextAtomic(root_ / "settings.json", j.dump(2) + "\n");
}

StoreResult ProfileStore::LoadHistory(std::vector<BurstRecord>& out, int* skipped) const {
    const fs::path file = root_ / "history.json";
    std::error_code ec;
    if (!fs::exists(file, ec)) {
        out.clear();
        return {};
    }
    std::string text;
    if (StoreResult r = ReadText(file, text); !r.ok) {
        return r;
    }
    std::string error;
    if (!HistoryFromJson(text, out, error, skipped)) {
        fs::rename(file, root_ / "history.invalid.json", ec);
        out.clear();
        return StoreResult::Fail(error + " (guardado como history.invalid.json)");
    }
    return {};
}

StoreResult ProfileStore::SaveHistory(const std::vector<BurstRecord>& entries) const {
    std::error_code ec;
    fs::create_directories(root_, ec);
    return WriteTextAtomic(root_ / "history.json", HistoryToJson(entries));
}

std::string ProfileStore::UniqueName(const std::string& base) const {
    if (!Exists(base)) {
        return base;
    }
    for (int i = 2; i < 1000; ++i) {
        const std::string candidate = base + " (" + std::to_string(i) + ")";
        if (!Exists(candidate)) {
            return candidate;
        }
    }
    return base + " (novo)";
}

StoreResult ProfileStore::WriteProfileFile(const fs::path& path, const ProfileData& data) {
    return WriteTextAtomic(path, ProfileToJson(data));
}

StoreResult ProfileStore::ReadProfileFile(const fs::path& path, ProfileData& out, std::vector<std::string>* warnings) {
    std::string text;
    if (StoreResult r = ReadText(path, text); !r.ok) {
        return r;
    }
    const ParseResult parsed = ProfileFromJson(text, out);
    if (!parsed.ok) {
        return StoreResult::Fail(parsed.error);
    }
    if (warnings != nullptr) {
        *warnings = parsed.warnings;
    }
    return {};
}

std::optional<std::string> ProfileStore::ValidateName(const std::string& name) {
    if (name.empty()) {
        return "o nome não pode ficar vazio";
    }
    if (CountCodepoints(name) > kMaxNameChars) {
        return "o nome pode ter no máximo 40 caracteres";
    }
    if (std::any_of(name.begin(), name.end(), IsForbidden)) {
        return "o nome não pode conter < > : \" / \\ | ? *";
    }
    if (name.front() == ' ' || name.back() == ' ' || name.back() == '.') {
        return "o nome não pode começar/terminar com espaço nem terminar com ponto";
    }
    if (IsReservedWindowsName(name)) {
        return "\"" + name + "\" é um nome reservado do Windows";
    }
    return std::nullopt;
}

std::string ProfileStore::SanitizeName(const std::string& text) {
    std::string out;
    for (char c : text) {
        out.push_back(IsForbidden(c) ? '-' : c);
    }
    // Corta em 40 caracteres sem quebrar um caractere UTF-8 no meio.
    std::size_t count = 0;
    for (std::size_t i = 0; i < out.size(); ++i) {
        if ((static_cast<unsigned char>(out[i]) & 0xC0) != 0x80 && ++count > kMaxNameChars) {
            out.resize(i);
            break;
        }
    }
    while (!out.empty() && (out.back() == ' ' || out.back() == '.')) {
        out.pop_back();
    }
    while (!out.empty() && out.front() == ' ') {
        out.erase(out.begin());
    }
    if (out.empty() || IsReservedWindowsName(out)) {
        return "Perfil importado";
    }
    return out;
}

}  // namespace st
