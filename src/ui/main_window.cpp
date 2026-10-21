#include "ui/main_window.h"

#include "ui/icons.h"
#include "ui/pages.h"
#include "ui/profiles.h"
#include "ui/theme.h"

#include <imgui.h>

#include <algorithm>
#include <array>

namespace st {

namespace {

struct NavItem {
    const char* label;
    icons::Icon icon;
};

constexpr std::array<NavItem, static_cast<std::size_t>(Page::Count)> kNav{{
    {"Principal", icons::Icon::Home},
    {"Ajustes", icons::Icon::Sliders},
    {"Visual", icons::Icon::Palette},
    {"Histórico", icons::Icon::History},
    {"Presets", icons::Icon::Folder},
}};

// ---------------------------------------------------------------- barra de título

enum class TitleIcon { Overlay, OverlayOff, Minimize, Maximize, Close };

bool TitleButton(const char* id, TitleIcon icon) {
    const ImVec2 size(40.0f, 32.0f);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (hovered) {
        const ImVec4 bg = icon == TitleIcon::Close ? theme::kPink : theme::kPanelAlt;
        dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), theme::U32(bg, 0.85f), 6.0f);
    }
    const ImU32 col = theme::U32(theme::kText);
    const ImVec2 c(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
    constexpr float r = 6.0f;
    switch (icon) {
        case TitleIcon::Overlay:
            icons::Draw(dl, icons::Icon::Eye, c, 18.0f, col);
            break;
        case TitleIcon::OverlayOff:
            icons::Draw(dl, icons::Icon::EyeOff, c, 18.0f, col);
            break;
        case TitleIcon::Minimize:
            dl->AddLine(ImVec2(c.x - r, c.y), ImVec2(c.x + r, c.y), col, 2.0f);
            break;
        case TitleIcon::Maximize:
            dl->AddRect(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), col, 2.0f, 0, 2.0f);
            break;
        case TitleIcon::Close:
            dl->AddLine(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), col, 2.0f);
            dl->AddLine(ImVec2(c.x - r, c.y + r), ImVec2(c.x + r, c.y - r), col, 2.0f);
            break;
    }
    return pressed;
}

void RenderTitleBar(AppState& state) {
    const ImVec2 origin = ImGui::GetWindowPos();
    const float width = ImGui::GetWindowWidth();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::PushFont(theme::Fonts().bold);
    dl->AddText(ImVec2(origin.x + 22.0f, origin.y + 14.0f), theme::U32(theme::Accent()), "SPRAY TRAINER");
    ImGui::PopFont();

    // Perfil atual + marcador de alterações não salvas.
    const std::string& current = state.profiles.current;
    const bool dirty = profiles::Dirty(state);
    const std::string label = "Perfil: " + (current.empty() ? std::string("não salvo") : current);
    const float x = origin.x + theme::kSidebarWidth + 28.0f;
    dl->AddText(ImVec2(x, origin.y + 14.0f), theme::U32(theme::kTextDim), label.c_str());
    if (dirty) {
        const float tw = ImGui::CalcTextSize(label.c_str()).x;
        dl->AddCircleFilled(ImVec2(x + tw + 10.0f, origin.y + 14.0f + ImGui::GetFontSize() * 0.55f), 4.0f,
                            theme::U32(theme::kPink));
    }

    ImGui::SetCursorScreenPos(ImVec2(origin.x + width - kTitleButtonsWidth + 16.0f, origin.y + 8.0f));
    if (TitleButton("##overlay", state.overlay.visible ? TitleIcon::Overlay : TitleIcon::OverlayOff)) {
        state.overlay.visible = !state.overlay.visible;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(state.overlay.visible ? "Esconder overlay" : "Mostrar overlay");
    }
    ImGui::SameLine(0.0f, 4.0f);
    if (TitleButton("##min", TitleIcon::Minimize)) {
        state.requestMinimize = true;
    }
    ImGui::SameLine(0.0f, 4.0f);
    if (TitleButton("##max", TitleIcon::Maximize)) {
        state.requestToggleMaximize = true;
    }
    ImGui::SameLine(0.0f, 4.0f);
    if (TitleButton("##close", TitleIcon::Close)) {
        state.requestClose = true;
    }
}

// ---------------------------------------------------------------- sidebar

void RenderSidebar(AppState& state) {
    const ImVec2 origin = ImGui::GetWindowPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float itemH = 85.0f;
    float y = origin.y + theme::kTitleBarHeight + 40.0f;

    for (std::size_t i = 0; i < kNav.size(); ++i) {
        const Page page = static_cast<Page>(i);
        const bool active = state.page == page;
        const ImVec2 pos(origin.x, y);
        ImGui::SetCursorScreenPos(pos);
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::InvisibleButton("##nav", ImVec2(theme::kSidebarWidth, itemH))) {
            state.page = page;
        }
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();

        if (active) {
            dl->AddRectFilled(ImVec2(pos.x + 30.0f, pos.y + 14.0f), ImVec2(pos.x + 35.0f, pos.y + 50.0f),
                              theme::U32(theme::Accent()), 3.0f);
        }
        const ImVec4 iconCol = active ? theme::Accent() : (hovered ? theme::kText : ImVec4(0.75f, 0.75f, 0.75f, 1.0f));
        const ImVec4 textCol = active ? theme::Accent() : theme::kText;
        const float cx = pos.x + theme::kSidebarWidth * 0.5f + 18.0f;
        icons::Draw(dl, kNav[i].icon, ImVec2(cx, pos.y + 30.0f), 26.0f, theme::U32(iconCol));

        ImFont* font = theme::Fonts().caption;
        const ImVec2 ts = font->CalcTextSizeA(font->FontSize, 1000.0f, 0.0f, kNav[i].label);
        dl->AddText(font, font->FontSize, ImVec2(cx - ts.x * 0.5f, pos.y + 54.0f), theme::U32(textCol), kNav[i].label);
        y += itemH;
    }
}

void RenderBanner(AppState& state) {
    ProfileUiState& p = state.profiles;
    if (p.banner.empty()) {
        return;
    }
    if (state.nowMs > p.bannerUntilMs) {
        p.banner.clear();
        return;
    }
    const ImVec4 col = p.bannerError ? theme::kPink : theme::kCyan;
    const float w = ImGui::GetContentRegionAvail().x;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::PushTextWrapPos(pos.x + w - 50.0f);
    const ImVec2 ts = ImGui::CalcTextSize(p.banner.c_str(), nullptr, false, w - 70.0f);
    const float h = ts.y + 20.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), theme::U32(col, 0.14f), 8.0f);
    dl->AddRectFilled(pos, ImVec2(pos.x + 4.0f, pos.y + h), theme::U32(col), 2.0f);
    ImGui::SetCursorScreenPos(ImVec2(pos.x + 16.0f, pos.y + 10.0f));
    ImGui::TextColored(col, "%s", p.banner.c_str());
    ImGui::PopTextWrapPos();
    ImGui::SetCursorScreenPos(ImVec2(pos.x + w - 34.0f, pos.y + (h - 26.0f) * 0.5f));
    if (ImGui::Button("x##banner", ImVec2(26.0f, 26.0f))) {
        p.banner.clear();
    }
    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + h + 10.0f));
}

void CollectFinishedBurst(AppState& state) {
    if (state.session.GetState() != TrainingSession::State::Done) {
        return;
    }
    SessionResult r = state.session.Result();
    state.session.Reset();
    if (r.shots.empty()) {
        return;
    }
    ++state.bursts;
    state.bestScore = std::max(state.bestScore, r.score);
    profiles::RecordBurst(state, r);
    state.lastResult = std::move(r);
}

}  // namespace

void RenderMainWindow(AppState& state, double nowMs) {
    state.nowMs = nowMs;
    CollectFinishedBurst(state);

    const ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false) && state.profiles.store != nullptr) {
        profiles::SaveCurrent(state);
    }

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, state.maximized ? 0.0f : 12.0f);
    ImGui::Begin("##root", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();

    RenderTitleBar(state);
    RenderSidebar(state);

    const ImVec2 origin = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    const float padX = 28.0f;
    const float padTop = 20.0f;
    const float padBottom = 22.0f;
    ImGui::SetCursorScreenPos(ImVec2(origin.x + theme::kSidebarWidth + padX, origin.y + theme::kTitleBarHeight + padTop));
    ImGui::BeginChild("##content",
                      ImVec2(size.x - theme::kSidebarWidth - 2.0f * padX,
                             size.y - theme::kTitleBarHeight - padTop - padBottom),
                      ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
    RenderBanner(state);
    switch (state.page) {
        case Page::Principal:
            pages::Principal(state, nowMs);
            break;
        case Page::Ajustes:
            pages::Ajustes(state);
            break;
        case Page::Visual:
            pages::Visual(state);
            break;
        case Page::Historico:
            pages::Historico(state);
            break;
        case Page::Presets:
            pages::Presets(state);
            break;
        case Page::Count:
            break;
    }
    ImGui::EndChild();
    profiles::RenderModals(state);
    ImGui::End();
}

}  // namespace st
