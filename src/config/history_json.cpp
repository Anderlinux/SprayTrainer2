#include "config/history_json.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>

namespace st {

namespace {

using Json = nlohmann::json;

// Histórico não precisa de precisão total: 3 casas (0,001°) deixam o arquivo bem menor.
double Round3(float v) {
    return std::round(static_cast<double>(v) * 1000.0) / 1000.0;
}

Json Points(const std::vector<Vec2>& pts) {
    Json arr = Json::array();
    for (const Vec2& p : pts) {
        arr.push_back(Json::array({Round3(p.x), Round3(p.y)}));
    }
    return arr;
}

bool ReadPoints(const Json& arr, std::vector<Vec2>& out) {
    if (!arr.is_array() || arr.size() > kPatternPoints) {
        return false;
    }
    out.clear();
    for (const Json& p : arr) {
        if (!p.is_array() || p.size() != 2 || !p[0].is_number() || !p[1].is_number()) {
            return false;
        }
        const double x = p[0].get<double>();
        const double y = p[1].get<double>();
        if (!std::isfinite(x) || !std::isfinite(y)) {
            return false;
        }
        out.push_back({static_cast<float>(x), static_cast<float>(y)});
    }
    return true;
}

bool ReadEntry(const Json& e, BurstRecord& r) {
    if (!e.is_object()) {
        return false;
    }
    const auto t = e.find("t");
    const auto weapon = e.find("weapon");
    const auto score = e.find("score");
    const auto err = e.find("meanError");
    if (t == e.end() || !t->is_number_integer() || weapon == e.end() || !weapon->is_string() || score == e.end() ||
        !score->is_number() || err == e.end() || !err->is_number()) {
        return false;
    }
    bool found = false;
    for (std::size_t i = 0; i < kWeaponCount; ++i) {
        if (GetWeaponInfo(static_cast<Weapon>(i)).id == weapon->get<std::string>()) {
            r.weapon = static_cast<Weapon>(i);
            found = true;
        }
    }
    const double s = score->get<double>();
    const double m = err->get<double>();
    if (!found || !std::isfinite(s) || s < 0.0 || s > 100.0 || !std::isfinite(m) || m < 0.0) {
        return false;
    }
    r.timestamp = t->get<std::int64_t>();
    r.score = static_cast<float>(s);
    r.meanError = static_cast<float>(m);
    const auto goal = e.find("goal");
    r.goal = goal != e.end() && goal->is_number_integer()
                 ? static_cast<int>(std::clamp<long long>(goal->get<long long>(), 0, 100))
                 : 0;
    const auto ideal = e.find("ideal");
    const auto player = e.find("player");
    if (ideal == e.end() || player == e.end() || !ReadPoints(*ideal, r.ideal) || !ReadPoints(*player, r.player) ||
        r.ideal.size() != r.player.size()) {
        return false;
    }
    return true;
}

}  // namespace

std::string HistoryToJson(const std::vector<BurstRecord>& entries) {
    nlohmann::ordered_json j;
    j["app"] = "SprayTrainer";
    j["kind"] = "history";
    j["version"] = 1;
    nlohmann::ordered_json& list = j["bursts"] = nlohmann::ordered_json::array();
    for (const BurstRecord& r : entries) {
        nlohmann::ordered_json e;
        e["t"] = r.timestamp;
        e["weapon"] = std::string(GetWeaponInfo(r.weapon).id);
        e["score"] = Round3(r.score);
        e["meanError"] = Round3(r.meanError);
        e["goal"] = r.goal;
        e["ideal"] = Points(r.ideal);
        e["player"] = Points(r.player);
        list.push_back(std::move(e));
    }
    return j.dump() + "\n";  // compacto: o histórico não é feito para editar à mão
}

bool HistoryFromJson(const std::string& text, std::vector<BurstRecord>& out, std::string& error, int* skipped) {
    const Json j = Json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) {
        error = "o arquivo não é um JSON válido";
        return false;
    }
    const auto kind = j.find("kind");
    const auto list = j.find("bursts");
    if (kind == j.end() || *kind != "history" || list == j.end() || !list->is_array()) {
        error = "não é um histórico do Spray Trainer";
        return false;
    }
    std::vector<BurstRecord> entries;
    int bad = 0;
    for (const Json& e : *list) {
        BurstRecord r;
        if (ReadEntry(e, r)) {
            entries.push_back(std::move(r));
        } else {
            ++bad;
        }
    }
    if (skipped != nullptr) {
        *skipped = bad;
    }
    out = std::move(entries);
    return true;
}

}  // namespace st
