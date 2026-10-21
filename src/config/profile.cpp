#include "config/profile.h"

#include "core/repetition.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace st {

namespace {

using Json = nlohmann::json;
using OrderedJson = nlohmann::ordered_json;

constexpr const char* kAppId = "SprayTrainer";
constexpr float kMaxPatternDegrees = 10.0f;
constexpr const char* kCornerIds[] = {"topLeft", "topRight", "bottomLeft", "bottomRight"};
constexpr const char* kGuideIds[] = {"always", "next", "hidden"};
constexpr const char* kPlayerColorIds[] = {"cyan", "orange", "green"};

// float -> double com a representação decimal mais curta (0.65f vira 0.65, não 0.6499999761...).
// Ler de volta e converter para float devolve exatamente o mesmo valor.
double Pretty(float value) {
    char buf[32];
    const auto res = std::to_chars(buf, buf + sizeof(buf), value);
    *res.ptr = '\0';
    return std::strtod(buf, nullptr);
}

std::string ToHex(const std::array<float, 3>& rgb) {
    char buf[8];
    const auto c = [](float v) { return static_cast<int>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f)); };
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", c(rgb[0]), c(rgb[1]), c(rgb[2]));
    return buf;
}

bool FromHex(const std::string& text, std::array<float, 3>& rgb) {
    if (text.size() != 7 || text[0] != '#') {
        return false;
    }
    for (std::size_t i = 0; i < 3; ++i) {
        int value = 0;
        const char* first = text.data() + 1 + i * 2;
        const auto res = std::from_chars(first, first + 2, value, 16);
        if (res.ec != std::errc() || res.ptr != first + 2) {
            return false;
        }
        rgb[i] = static_cast<float>(value) / 255.0f;
    }
    return true;
}

bool WeaponFromId(const std::string& id, Weapon& out) {
    for (std::size_t i = 0; i < kWeaponCount; ++i) {
        const auto w = static_cast<Weapon>(i);
        if (GetWeaponInfo(w).id == id) {
            out = w;
            return true;
        }
    }
    return false;
}

// Lê campos de um objeto JSON acumulando erros/avisos com o caminho do campo.
class Reader {
public:
    explicit Reader(ParseResult& result) : r_(result) {}

    bool Failed() const { return failed_; }

    void Error(const std::string& path, const std::string& what) {
        if (!failed_) {
            r_.error = path + ": " + what;
        }
        failed_ = true;
    }

    void Warn(const std::string& msg) { r_.warnings.push_back(msg); }

    // Devolve o objeto `key` de `parent`, ou nullptr se ausente (com aviso) ou inválido (erro).
    // Seções/campos opcionais (adicionados depois da versão 1) não geram aviso quando ausentes.
    const Json* Section(const Json& parent, const char* key, bool optional = false) {
        const auto it = parent.find(key);
        if (it == parent.end()) {
            if (!optional) {
                Warn(std::string("seção \"") + key + "\" ausente; usando valores padrão");
            }
            return nullptr;
        }
        if (!it->is_object()) {
            Error(key, "deveria ser um objeto");
            return nullptr;
        }
        return &*it;
    }

    void Float(const Json& obj, const std::string& section, const char* key, float& out, float lo, float hi,
               bool optional = false) {
        const auto it = obj.find(key);
        const std::string path = section + "." + key;
        if (it == obj.end()) {
            if (!optional) {
                Warn(path + " ausente; usando padrão");
            }
            return;
        }
        if (!it->is_number()) {
            Error(path, "deveria ser um número");
            return;
        }
        const double v = it->get<double>();
        if (!std::isfinite(v)) {
            Error(path, "número inválido");
            return;
        }
        const float f = static_cast<float>(v);
        out = std::clamp(f, lo, hi);
        if (out != f) {
            Warn(path + " fora da faixa; ajustado");
        }
    }

    void Int(const Json& obj, const std::string& section, const char* key, int& out, int lo, int hi) {
        const auto it = obj.find(key);
        const std::string path = section + "." + key;
        if (it == obj.end()) {
            Warn(path + " ausente; usando padrão");
            return;
        }
        if (!it->is_number_integer()) {
            Error(path, "deveria ser um número inteiro");
            return;
        }
        const long long v = it->get<long long>();
        out = static_cast<int>(std::clamp<long long>(v, lo, hi));
        if (out != v) {
            Warn(path + " fora da faixa; ajustado");
        }
    }

    void Bool(const Json& obj, const std::string& section, const char* key, bool& out) {
        const auto it = obj.find(key);
        const std::string path = section + "." + key;
        if (it == obj.end()) {
            Warn(path + " ausente; usando padrão");
            return;
        }
        if (!it->is_boolean()) {
            Error(path, "deveria ser true/false");
            return;
        }
        out = it->get<bool>();
    }

    // Como Bool, mas para campos de seções adicionadas depois da versão 1:
    // ausente = mantém o padrão, sem gerar aviso.
    void BoolOpt(const Json& obj, const std::string& section, const char* key, bool& out) {
        const auto it = obj.find(key);
        if (it == obj.end()) {
            return;
        }
        if (!it->is_boolean()) {
            Error(section + "." + key, "deveria ser true/false");
            return;
        }
        out = it->get<bool>();
    }

    // Como Int, mas para campos opcionais de seções novas: ausente = padrão, sem aviso.
    void IntOpt(const Json& obj, const std::string& section, const char* key, int& out, int lo, int hi) {
        const auto it = obj.find(key);
        if (it == obj.end()) {
            return;
        }
        if (!it->is_number_integer()) {
            Error(section + "." + key, "deveria ser um número inteiro");
            return;
        }
        const long long v = it->get<long long>();
        out = static_cast<int>(std::clamp<long long>(v, lo, hi));
        if (out != v) {
            Warn(section + "." + key + " fora da faixa; ajustado");
        }
    }

    // Texto opcional (seções novas): ausente = mantém o padrão, sem aviso.
    void StrOpt(const Json& obj, const std::string& section, const char* key, std::string& out) {
        const auto it = obj.find(key);
        if (it == obj.end()) {
            return;
        }
        if (!it->is_string()) {
            Error(section + "." + key, "deveria ser um texto");
            return;
        }
        out = it->get<std::string>();
    }

    // Nome de tecla virtual ("F", "LBUTTON"...); aceita maiúsculas/minúsculas.
    // `optional` = seções adicionadas depois da versão 1 (ausente não gera aviso).
    void KeyField(const Json& obj, const std::string& section, const char* key, int& out, bool optional = false) {
        const auto it = obj.find(key);
        const std::string path = section + "." + key;
        if (it == obj.end()) {
            if (!optional) {
                Warn(path + " ausente; usando padrão");
            }
            return;
        }
        if (!it->is_string()) {
            Error(path, "deveria ser o nome de uma tecla");
            return;
        }
        const int vk = VirtualKeyFromName(it->get<std::string>().c_str());
        if (vk == 0) {
            Error(path, "tecla desconhecida \"" + it->get<std::string>() + "\"");
            return;
        }
        out = vk;
    }

    // Texto que precisa ser um de `options`; devolve o índice ou -1.
    int Choice(const Json& obj, const std::string& section, const char* key, const char* const* options, int count) {
        const auto it = obj.find(key);
        const std::string path = section + "." + key;
        if (it == obj.end()) {
            Warn(path + " ausente; usando padrão");
            return -1;
        }
        if (!it->is_string()) {
            Error(path, "deveria ser um texto");
            return -1;
        }
        const std::string value = it->get<std::string>();
        for (int i = 0; i < count; ++i) {
            if (value == options[i]) {
                return i;
            }
        }
        Error(path, "valor desconhecido \"" + value + "\"");
        return -1;
    }

    void ReadPattern(const Json& arr, const std::string& path, Pattern& out) {
        if (!arr.is_array() || arr.size() != kPatternPoints) {
            Error(path, "deveria ter exatamente 31 pontos [x, y]");
            return;
        }
        bool clamped = false;
        for (std::size_t i = 0; i < kPatternPoints; ++i) {
            const Json& pt = arr[i];
            if (!pt.is_array() || pt.size() != 2 || !pt[0].is_number() || !pt[1].is_number()) {
                Error(path + "[" + std::to_string(i) + "]", "deveria ser [x, y] numérico");
                return;
            }
            const double x = pt[0].get<double>();
            const double y = pt[1].get<double>();
            if (!std::isfinite(x) || !std::isfinite(y)) {
                Error(path + "[" + std::to_string(i) + "]", "número inválido");
                return;
            }
            const float fx = std::clamp(static_cast<float>(x), -kMaxPatternDegrees, kMaxPatternDegrees);
            const float fy = std::clamp(static_cast<float>(y), -kMaxPatternDegrees, kMaxPatternDegrees);
            clamped |= fx != static_cast<float>(x) || fy != static_cast<float>(y);
            out[i] = {fx, fy};
        }
        if (clamped) {
            Warn(path + ": valores acima de 10° foram limitados");
        }
    }

private:
    ParseResult& r_;
    bool failed_ = false;
};

bool SamePattern(const Pattern& a, const Pattern& b) {
    for (std::size_t i = 0; i < kPatternPoints; ++i) {
        if (a[i].x != b[i].x || a[i].y != b[i].y) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool ProfileData::operator==(const ProfileData& o) const {
    for (std::size_t i = 0; i < kWeaponCount; ++i) {
        if (!SamePattern(patterns[i], o.patterns[i])) {
            return false;
        }
    }
    return weapon == o.weapon && sensitivity == o.sensitivity && fireButton == o.fireButton &&
           scoreGoal == o.scoreGoal && scoreTolerance == o.scoreTolerance && overlayVisible == o.overlayVisible &&
           overlayCorner == o.overlayCorner && overlayScale == o.overlayScale &&
           overlayOpacity == o.overlayOpacity && overlayRefreshMs == o.overlayRefreshMs && accent == o.accent &&
           windowOpacity == o.windowOpacity && uiScale == o.uiScale && visualGuide == o.visualGuide &&
           visualGuideAhead == o.visualGuideAhead && visualPlayerColor == o.visualPlayerColor &&
           visualLineWidth == o.visualLineWidth && visualGrid == o.visualGrid &&
           visualShotNumbers == o.visualShotNumbers && repetitionEnabled == o.repetitionEnabled &&
           repeatKeyVk == o.repeatKeyVk && repeatToggle == o.repeatToggle &&
           stopKeysVk == o.stopKeysVk && repeatSubsteps == o.repeatSubsteps &&
           repeatAutoScore == o.repeatAutoScore &&
           repeatReturnBack == o.repeatReturnBack &&
           repeatRestartFromZero == o.repeatRestartFromZero &&
           cycleWeaponKeyVk == o.cycleWeaponKeyVk &&
           cycleBackKeyVk == o.cycleBackKeyVk &&
           autoVisionEnabled == o.autoVisionEnabled && autoRegionX == o.autoRegionX &&
           autoRegionY == o.autoRegionY && autoRegionW == o.autoRegionW && autoRegionH == o.autoRegionH &&
           autoWindowTitle == o.autoWindowTitle && autoConfidenceMin == o.autoConfidenceMin &&
           autoIntervalMs == o.autoIntervalMs && autoConfirmFrames == o.autoConfirmFrames;
}

ProfileData DefaultProfile() {
    ProfileData d;
    for (std::size_t i = 0; i < kWeaponCount; ++i) {
        d.patterns[i] = DefaultPattern(static_cast<Weapon>(i));
    }
    return d;
}

std::string ProfileToJson(const ProfileData& d) {
    OrderedJson j;
    j["app"] = kAppId;
    j["version"] = kProfileVersion;

    OrderedJson& t = j["training"];
    t["weapon"] = std::string(GetWeaponInfo(d.weapon).id);
    t["sensitivity"] = Pretty(d.sensitivity);
    t["fireButton"] = d.fireButton == 1 ? "right" : "left";
    t["scoreGoal"] = d.scoreGoal;
    t["scoreTolerance"] = Pretty(d.scoreTolerance);

    OrderedJson& p = j["patterns"];
    for (std::size_t i = 0; i < kWeaponCount; ++i) {
        OrderedJson arr = OrderedJson::array();
        for (const Vec2& v : d.patterns[i]) {
            arr.push_back(OrderedJson::array({Pretty(v.x), Pretty(v.y)}));
        }
        p[std::string(GetWeaponInfo(static_cast<Weapon>(i)).id)] = std::move(arr);
    }

    OrderedJson& o = j["overlay"];
    o["visible"] = d.overlayVisible;
    o["corner"] = kCornerIds[std::clamp(d.overlayCorner, 0, 3)];
    o["scale"] = Pretty(d.overlayScale);
    o["opacity"] = Pretty(d.overlayOpacity);
    o["refreshMs"] = d.overlayRefreshMs;

    OrderedJson& v = j["visual"];
    v["guide"] = kGuideIds[std::clamp(d.visualGuide, 0, 2)];
    v["guideAhead"] = d.visualGuideAhead;
    v["playerColor"] = kPlayerColorIds[std::clamp(d.visualPlayerColor, 0, 2)];
    v["lineWidth"] = Pretty(d.visualLineWidth);
    v["grid"] = d.visualGrid;
    v["shotNumbers"] = d.visualShotNumbers;

    OrderedJson& ui = j["interface"];
    ui["accent"] = ToHex(d.accent);
    ui["windowOpacity"] = Pretty(d.windowOpacity);
    ui["scale"] = Pretty(d.uiScale);

    // "repetition" entrou depois da versão 1: perfis antigos não têm a seção (opcional).
    OrderedJson& rp = j["repetition"];
    rp["enabled"] = d.repetitionEnabled;
    rp["repeatKey"] = std::string(VirtualKeyName(d.repeatKeyVk));
    OrderedJson stops = OrderedJson::array();
    for (const int vk : d.stopKeysVk) {
        stops.push_back(std::string(VirtualKeyName(vk)));
    }
    rp["stopKeys"] = std::move(stops);
    rp["substeps"] = d.repeatSubsteps;
    rp["autoScore"] = d.repeatAutoScore;
    rp["returnBack"] = d.repeatReturnBack;
    // Campos que entraram depois: gravados sempre, lidos como opcionais.
    rp["restartFromZero"] = d.repeatRestartFromZero;
    rp["toggle"] = d.repeatToggle;
    if (d.cycleWeaponKeyVk != 0) {
        rp["cycleWeaponKey"] = std::string(VirtualKeyName(d.cycleWeaponKeyVk));
    } else {
        rp["cycleWeaponKey"] = "OFF";
    }
    if (d.cycleBackKeyVk != 0) {
        rp["cycleBackKey"] = std::string(VirtualKeyName(d.cycleBackKeyVk));
    } else {
        rp["cycleBackKey"] = "OFF";
    }

    // "autoVision" entrou depois da versão 1: perfis antigos não têm a seção.
    OrderedJson& av = j["autoVision"];
    av["enabled"] = d.autoVisionEnabled;
    av["windowTitle"] = d.autoWindowTitle;
    OrderedJson region = OrderedJson::array();
    region.push_back(d.autoRegionX);
    region.push_back(d.autoRegionY);
    region.push_back(d.autoRegionW);
    region.push_back(d.autoRegionH);
    av["region"] = std::move(region);
    av["confidenceMin"] = Pretty(d.autoConfidenceMin);
    av["intervalMs"] = d.autoIntervalMs;
    av["confirmFrames"] = d.autoConfirmFrames;

    return j.dump(2) + "\n";
}

ParseResult ProfileFromJson(const std::string& text, ProfileData& out) {
    ParseResult result;
    const Json j = Json::parse(text, nullptr, false);
    if (j.is_discarded()) {
        result.error = "o arquivo não é um JSON válido";
        return result;
    }
    if (!j.is_object()) {
        result.error = "o conteúdo deveria ser um objeto JSON";
        return result;
    }
    const auto app = j.find("app");
    if (app == j.end() || !app->is_string() || app->get<std::string>() != kAppId) {
        result.error = "não é um perfil do Spray Trainer (campo \"app\")";
        return result;
    }
    const auto version = j.find("version");
    if (version == j.end() || !version->is_number_integer()) {
        result.error = "campo \"version\" ausente ou inválido";
        return result;
    }
    if (version->get<long long>() > kProfileVersion) {
        result.error = "perfil criado por uma versão mais nova do app (versão " +
                       std::to_string(version->get<long long>()) + ")";
        return result;
    }

    ProfileData d = DefaultProfile();
    Reader rd(result);

    if (const Json* t = rd.Section(j, "training")) {
        const auto w = t->find("weapon");
        if (w == t->end()) {
            rd.Warn("training.weapon ausente; usando padrão");
        } else if (!w->is_string() || !WeaponFromId(w->get<std::string>(), d.weapon)) {
            rd.Error("training.weapon", "arma desconhecida");
        }
        rd.Float(*t, "training", "sensitivity", d.sensitivity, 0.05f, 20.0f);
        const char* buttons[] = {"left", "right"};
        const int fire = rd.Choice(*t, "training", "fireButton", buttons, 2);
        if (fire >= 0) {
            d.fireButton = fire;
        }
        rd.Int(*t, "training", "scoreGoal", d.scoreGoal, 0, 100);
        rd.Float(*t, "training", "scoreTolerance", d.scoreTolerance, 0.5f, 6.0f);
    }

    if (const Json* p = rd.Section(j, "patterns")) {
        for (std::size_t i = 0; i < kWeaponCount && !rd.Failed(); ++i) {
            const std::string id(GetWeaponInfo(static_cast<Weapon>(i)).id);
            const auto it = p->find(id);
            if (it == p->end()) {
                // Armas adicionadas depois da v1 podem faltar em perfis antigos: sem aviso.
                if (GetWeaponInfo(static_cast<Weapon>(i)).inFirstVersion) {
                    rd.Warn("patterns." + id + " ausente; usando o padrão de fábrica");
                }
                continue;
            }
            rd.ReadPattern(*it, "patterns." + id, d.patterns[i]);
        }
    }

    if (const Json* o = rd.Section(j, "overlay")) {
        rd.Bool(*o, "overlay", "visible", d.overlayVisible);
        const int corner = rd.Choice(*o, "overlay", "corner", kCornerIds, 4);
        if (corner >= 0) {
            d.overlayCorner = corner;
        }
        rd.Float(*o, "overlay", "scale", d.overlayScale, 0.75f, 1.5f);
        rd.Float(*o, "overlay", "opacity", d.overlayOpacity, 0.3f, 1.0f);
        rd.Int(*o, "overlay", "refreshMs", d.overlayRefreshMs, 16, 250);
    }

    if (const Json* ui = rd.Section(j, "interface")) {
        const auto accent = ui->find("accent");
        if (accent == ui->end()) {
            rd.Warn("interface.accent ausente; usando padrão");
        } else if (!accent->is_string() || !FromHex(accent->get<std::string>(), d.accent)) {
            rd.Error("interface.accent", "cor deveria estar no formato \"#RRGGBB\"");
        }
        rd.Float(*ui, "interface", "windowOpacity", d.windowOpacity, 0.6f, 1.0f);
        rd.Float(*ui, "interface", "scale", d.uiScale, 0.8f, 1.5f, true);
    }

    // "visual" entrou depois da versão 1: ausente = padrões, sem aviso.
    if (const Json* v = rd.Section(j, "visual", true)) {
        const int guide = rd.Choice(*v, "visual", "guide", kGuideIds, 3);
        if (guide >= 0) {
            d.visualGuide = guide;
        }
        rd.Int(*v, "visual", "guideAhead", d.visualGuideAhead, 1, 10);
        const int color = rd.Choice(*v, "visual", "playerColor", kPlayerColorIds, 3);
        if (color >= 0) {
            d.visualPlayerColor = color;
        }
        rd.Float(*v, "visual", "lineWidth", d.visualLineWidth, 1.0f, 4.0f);
        rd.Bool(*v, "visual", "grid", d.visualGrid);
        rd.Bool(*v, "visual", "shotNumbers", d.visualShotNumbers);
    }

    // "repetition" entrou depois da versão 1: ausente = padrões, sem aviso.
    if (const Json* rp = rd.Section(j, "repetition", true)) {
        rd.Bool(*rp, "repetition", "enabled", d.repetitionEnabled);
        rd.KeyField(*rp, "repetition", "repeatKey", d.repeatKeyVk, true);
        const auto stops = rp->find("stopKeys");
        if (stops != rp->end()) {
            if (!stops->is_array() || stops->size() > kMaxStopKeys) {
                rd.Error("repetition.stopKeys",
                         stops->is_array() ? ("máximo de " + std::to_string(kMaxStopKeys) + " teclas")
                                           : std::string("deveria ser uma lista de teclas"));
            } else {
                d.stopKeysVk.clear();
                for (std::size_t i = 0; i < stops->size() && !rd.Failed(); ++i) {
                    const Json& name = (*stops)[i];
                    if (!name.is_string()) {
                        rd.Error("repetition.stopKeys", "deveria ser uma lista de nomes de teclas");
                        break;
                    }
                    const int vk = VirtualKeyFromName(name.get<std::string>().c_str());
                    if (vk == 0) {
                        rd.Error("repetition.stopKeys", "tecla desconhecida \"" + name.get<std::string>() + "\"");
                        break;
                    }
                    // Migração silenciosa: perfis antigos salvos antes da separação
                    // LBUTTON/KEY1 tinham "1" na lista de desativar — isso fazia o
                    // toque em "1" apenas RELIGAR a trava e nunca ligar o replay.
                    // "1" agora é exclusivamente a tecla de religar, então some da lista.
                    if (vk == kDefaultEnableKeyVk) {
                        continue;
                    }
                    d.stopKeysVk.push_back(vk);
                }
            }
        } else {
            // Ausente = teclas padrão (2..8, B, G, H); sem aviso para não acusar
            // perfis antigos que simplesmente não tinham a lista.
            d.stopKeysVk.assign(kDefaultStopKeysVk, kDefaultStopKeysVk + kDefaultStopKeysCount);
        }
        rd.Int(*rp, "repetition", "substeps", d.repeatSubsteps, 1, PatternRepeater::kMaxSubsteps);
        // "autoScore" entrou depois: ausente = ligado, sem aviso.
        rd.BoolOpt(*rp, "repetition", "autoScore", d.repeatAutoScore);
        rd.Bool(*rp, "repetition", "returnBack", d.repeatReturnBack);
        // Campos opcionais que entraram depois: ausentes = padrões (perfis antigos ok).
        rd.BoolOpt(*rp, "repetition", "restartFromZero", d.repeatRestartFromZero);
        rd.BoolOpt(*rp, "repetition", "toggle", d.repeatToggle);
        if (const auto cyc = rp->find("cycleWeaponKey"); cyc != rp->end()) {
            if (!cyc->is_string()) {
                rd.Error("repetition.cycleWeaponKey", "deveria ser um nome de tecla ou \"OFF\"");
            } else if (const std::string name = cyc->get<std::string>(); name == "OFF") {
                d.cycleWeaponKeyVk = 0;
            } else {
                const int vk = VirtualKeyFromName(name.c_str());
                if (vk == 0) {
                    rd.Error("repetition.cycleWeaponKey", "tecla desconhecida \"" + name + "\"");
                } else {
                    d.cycleWeaponKeyVk = vk;
                }
            }
        }
        // "cycleBackKey" entrou depois: ausente = padrão (F2), sem aviso.
        if (const auto cbk = rp->find("cycleBackKey"); cbk != rp->end()) {
            if (!cbk->is_string()) {
                rd.Error("repetition.cycleBackKey", "deveria ser um nome de tecla ou \"OFF\"");
            } else if (const std::string name = cbk->get<std::string>(); name == "OFF") {
                d.cycleBackKeyVk = 0;
            } else {
                const int vk = VirtualKeyFromName(name.c_str());
                if (vk == 0) {
                    rd.Error("repetition.cycleBackKey", "tecla desconhecida \"" + name + "\"");
                } else {
                    d.cycleBackKeyVk = vk;
                }
            }
        }
    }

    // "autoVision" entrou depois da versão 1: seção ausente = tudo desligado/padrão,
    // sem aviso (perfis antigos continuam carregando normalmente).
    if (const Json* av = rd.Section(j, "autoVision", true)) {
        rd.BoolOpt(*av, "autoVision", "enabled", d.autoVisionEnabled);
        rd.StrOpt(*av, "autoVision", "windowTitle", d.autoWindowTitle);
        if (const auto reg = av->find("region"); reg != av->end()) {
            if (!reg->is_array() || reg->size() != 4) {
                rd.Error("autoVision.region", "deveria ser uma lista [x, y, w, h]");
            } else {
                int vals[4];
                bool bad = false;
                for (std::size_t i = 0; i < 4; ++i) {
                    const Json& v = (*reg)[i];
                    if (!v.is_number_integer()) {
                        rd.Error("autoVision.region", "deveria conter quatro números inteiros");
                        bad = true;
                        break;
                    }
                    vals[i] = static_cast<int>(std::clamp<long long>(v.get<long long>(), -32768, 32767));
                }
                if (!bad) {
                    d.autoRegionX = vals[0];
                    d.autoRegionY = vals[1];
                    d.autoRegionW = std::clamp(vals[2], 0, kMaxAutoRegionSize);
                    d.autoRegionH = std::clamp(vals[3], 0, kMaxAutoRegionSize);
                    if (vals[2] != d.autoRegionW || vals[3] != d.autoRegionH) {
                        rd.Warn("autoVision.region fora da faixa; ajustado");
                    }
                }
            }
        }
        rd.Float(*av, "autoVision", "confidenceMin", d.autoConfidenceMin, 0.0f, 1.0f, true);
        rd.IntOpt(*av, "autoVision", "intervalMs", d.autoIntervalMs, 100, 2000);
        rd.IntOpt(*av, "autoVision", "confirmFrames", d.autoConfirmFrames, 1, 10);
    }

    if (rd.Failed()) {
        return result;
    }
    out = d;
    result.ok = true;
    return result;
}

}  // namespace st
