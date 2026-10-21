#include "ui/auto_vision.h"

#include "core/vision/png.h"
#include "core/weapon.h"

#include <algorithm>

namespace st {

// Copia os campos de AutoVisionSettings (estado da UI) para ProfileData, que é
// a moeda usada pelo controller e pela captura. Manter uma única cópia na UI e
// derivar daqui evita dessincronizar perfil x estado em tempo real.
static ProfileData AutoProfileView(const AppState& state) {
    ProfileData p;
    p.autoVisionEnabled = state.autoVision.enabled;
    p.autoRegionX = state.autoVision.regionX;
    p.autoRegionY = state.autoVision.regionY;
    p.autoRegionW = state.autoVision.regionW;
    p.autoRegionH = state.autoVision.regionH;
    p.autoWindowTitle = state.autoVision.windowTitle;
    p.autoConfidenceMin = state.autoVision.confidenceMin;
    p.autoIntervalMs = state.autoVision.intervalMs;
    p.autoConfirmFrames = state.autoVision.confirmFrames;
    return p;
}

std::vector<std::string> AutoVisionController::ReloadTemplatesState(const AppState& state) {
    return ReloadTemplates(AutoProfileView(state), state.weaponImages);
}

const WeaponImageEntry* AutoVisionController::WeaponImageEntryFor(const AppState& state, int weaponIndex) {
    if (weaponIndex < 0 || weaponIndex >= static_cast<int>(Weapon::Count)) {
        return nullptr;
    }
    const std::string_view id = GetWeaponInfo(static_cast<Weapon>(weaponIndex)).id;
    for (const WeaponImageEntry& entry : state.weaponImages) {
        if (entry.weaponId == id && !entry.pngBase64.empty()) {
            return &entry;
        }
    }
    return nullptr;
}

std::vector<std::string> AutoVisionController::ReloadTemplates(
    const ProfileData& profile, const std::vector<WeaponImageEntry>& entries) {
    std::vector<std::string> warnings;
    templatesLoaded_ = false;

    if (profile.autoRegionW <= 0 || profile.autoRegionH <= 0) {
        warnings.push_back("Regiao de reconhecimento nao configurada (defina X/Y/L/A).");
        matcher_.Build(ImageRgb{}, {});
        return warnings;
    }

    // A regiao monitorada define a escala de trabalho: os templates sao
    // recortes cadastrados no tamanho da propria regiao (ver TemplateMatcher).
    ImageRgb referenceSize;
    referenceSize.width = profile.autoRegionW;
    referenceSize.height = profile.autoRegionH;
    referenceSize.rgb.assign(static_cast<std::size_t>(profile.autoRegionW) *
                                 static_cast<std::size_t>(profile.autoRegionH) * 3u,
                             0u);

    std::vector<TemplateMatcher::WeaponTemplate> templates;
    for (const WeaponImageEntry& entry : entries) {
        if (entry.pngBase64.empty()) {
            continue;  // arma sem imagem cadastrada: simplesmente ignorada
        }
        int weaponIndex = -1;
        for (int i = 0; i < static_cast<int>(Weapon::Count); ++i) {
            if (GetWeaponInfo(static_cast<Weapon>(i)).id == entry.weaponId) {
                weaponIndex = i;
                break;
            }
        }
        if (weaponIndex < 0) {
            warnings.push_back("Imagem de referencia descartada: arma desconhecida \"" +
                               entry.weaponId + "\".");
            continue;
        }
        std::vector<unsigned char> pngBytes;
        if (!Base64Decode(entry.pngBase64, pngBytes)) {
            warnings.push_back("Imagem de \"" + entry.weaponId +
                               "\" descartada: base64 invalido.");
            continue;
        }
        ImageRgb decoded;
        if (!DecodePng(pngBytes, decoded) || decoded.Empty()) {
            warnings.push_back("Imagem de \"" + entry.weaponId +
                               "\" descartada: PNG invalido ou corrompido.");
            continue;
        }
        if (decoded.width > profile.autoRegionW || decoded.height > profile.autoRegionH) {
            warnings.push_back("Imagem de \"" + entry.weaponId +
                               "\" descartada: maior que a regiao monitorada.");
            continue;
        }
        TemplateMatcher::WeaponTemplate t;
        t.weaponIndex = weaponIndex;
        t.image = std::move(decoded);
        templates.push_back(std::move(t));
    }
    if (templates.empty()) {
        warnings.push_back("Nenhuma imagem de referencia valida cadastrada.");
    }

    matcher_.Build(referenceSize, templates);
    picker_.Reset();
    lastCheckMs_ = -1.0e9;
    status_ = FrameStatus{};
    templatesLoaded_ = !matcher_.Empty();
    return warnings;
}

void AutoVisionController::Tick(const ProfileData& profile, double nowMs,
                                CaptureResult (*capture)(const ProfileData&), bool& outSwitched,
                                int& outWeaponIndex) {
    outSwitched = false;
    outWeaponIndex = -1;
    status_.enabled = profile.autoVisionEnabled;

    if (!profile.autoVisionEnabled) {
        if (status_.weaponIndex != -1 || status_.status != AutoPicker::Status::Waiting) {
            Reset();
            status_.enabled = false;
        }
        return;
    }
    if (!templatesLoaded_ || matcher_.Empty()) {
        status_.reason = "sem imagens cadastradas";
        return;
    }
    // Intervalo minimo entre verificacoes (requisito de desempenho): nunca
    // captura duas vezes no mesmo tick nem mais rapido que autoIntervalMs.
    if (nowMs - lastCheckMs_ < static_cast<double>(profile.autoIntervalMs)) {
        return;
    }
    lastCheckMs_ = nowMs;

    CaptureResult shot = capture(profile);
    if (!shot.ok || shot.image.Empty()) {
        status_.captureFailed = true;
        status_.status = AutoPicker::Status::Waiting;
        status_.reason = shot.error.empty() ? "falha na captura" : shot.error;
        // Sem leitura nova: mantem a arma atual em espera, sem reexecutar nada.
        return;
    }
    status_.captureFailed = false;
    lastCapture_ = std::move(shot.image);

    std::vector<float> scores(static_cast<std::size_t>(Weapon::Count), 0.0f);
    for (std::size_t i = 0; i < matcher_.Count(); ++i) {
        const TemplateMatcher::Hit hit = matcher_.MatchBySlot(static_cast<int>(i), lastCapture_);
        const int idx = matcher_.WeaponIndexAt(static_cast<int>(i));
        if (idx >= 0 && idx < static_cast<int>(Weapon::Count)) {
            scores[static_cast<std::size_t>(idx)] = TemplateMatcher::Confidence(hit.sad);
        }
    }

    AutoPicker::Params params;
    params.minConfidence = profile.autoConfidenceMin;
    params.confirmations = profile.autoConfirmFrames;
    params.minIntervalMs = static_cast<double>(profile.autoIntervalMs) * 2.0;
    picker_.SetParams(params);

    const AutoPicker::Result decision = picker_.Observe(nowMs, scores);
    status_.status = decision.status;
    status_.confidence = decision.confidence;
    status_.reason = decision.reason;
    status_.weaponIndex = decision.weaponIndex;
    if (decision.switched) {
        status_.lastSwitchMs = nowMs;
        outSwitched = true;
        outWeaponIndex = decision.weaponIndex;
    }
}

void AutoVisionController::TickState(const AppState& state, double nowMs,
                                     CaptureResult (*capture)(const AppState&), bool& outSwitched,
                                     int& outWeaponIndex) {
    const ProfileData profile = AutoProfileView(state);
    // A captura precisa ver o MESMO estado (região/título de janela) usado agora.
    struct Ctx {
        CaptureResult (*fn)(const AppState&);
        const AppState* state;
    };
    static thread_local Ctx ctx{nullptr, nullptr};
    ctx.fn = capture;
    ctx.state = &state;
    Tick(profile, nowMs,
         [](const ProfileData&) -> CaptureResult { return ctx.fn(*ctx.state); }, outSwitched,
         outWeaponIndex);
}

void AutoVisionController::Reset() {
    picker_.Reset();
    status_ = FrameStatus{};
    lastCheckMs_ = -1.0e9;
}

bool AutoVisionController::ScoreTemplate(const ProfileData& profile, const std::string& pngBase64,
                                         const ImageRgb& region, float& outConfidence) {
    outConfidence = 0.0f;
    if (profile.autoRegionW <= 0 || profile.autoRegionH <= 0 || region.Empty()) {
        return false;
    }
    std::vector<unsigned char> pngBytes;
    if (!Base64Decode(pngBase64, pngBytes)) {
        return false;
    }
    ImageRgb decoded;
    if (!DecodePng(pngBytes, decoded) || decoded.Empty()) {
        return false;
    }
    if (decoded.width > profile.autoRegionW || decoded.height > profile.autoRegionH) {
        return false;
    }
    TemplateMatcher matcher;
    ImageRgb referenceSize;
    referenceSize.width = region.width;   // escala da captura real, não da região configurada
    referenceSize.height = region.height;
    referenceSize.rgb.assign(region.SizeBytes(), 0u);
    if (decoded.width > referenceSize.width || decoded.height > referenceSize.height) {
        return false;  // template maior que a imagem realmente capturada
    }
    TemplateMatcher::WeaponTemplate t;
    t.weaponIndex = 0;  // índice irrelevante: só a batida importa aqui
    t.image = std::move(decoded);
    matcher.Build(referenceSize, {std::move(t)});
    const TemplateMatcher::Hit hit = matcher.MatchBySlot(0, region);
    outConfidence = TemplateMatcher::Confidence(hit.sad);
    return true;
}

}  // namespace st
