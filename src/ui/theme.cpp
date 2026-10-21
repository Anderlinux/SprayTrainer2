#include "ui/theme.h"

#include <cstdio>

namespace st::theme {

namespace {
ImVec4 g_accent = kDefaultAccent;
}  // namespace

const ImVec4& Accent() {
    return g_accent;
}

void SetAccent(const ImVec4& color) {
    g_accent = ImVec4(color.x, color.y, color.z, 1.0f);
}

ImU32 U32(const ImVec4& c, float alphaMul) {
    return ImGui::GetColorU32(ImVec4(c.x, c.y, c.z, c.w * alphaMul));
}

void Apply() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 12.0f;
    s.ChildRounding = 12.0f;
    s.FrameRounding = 8.0f;
    s.PopupRounding = 8.0f;
    s.GrabRounding = 8.0f;
    s.TabRounding = 8.0f;
    s.ScrollbarRounding = 8.0f;
    s.WindowBorderSize = 0.0f;
    s.ChildBorderSize = 1.0f;
    s.FrameBorderSize = 1.0f;
    s.PopupBorderSize = 1.0f;
    s.WindowPadding = ImVec2(0.0f, 0.0f);
    s.FramePadding = ImVec2(12.0f, 8.0f);
    s.ItemSpacing = ImVec2(10.0f, 10.0f);
    s.ScrollbarSize = 10.0f;

    const ImVec4 accent = Accent();
    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = kText;
    c[ImGuiCol_TextDisabled] = kTextDim;
    c[ImGuiCol_WindowBg] = kBg;
    c[ImGuiCol_ChildBg] = kPanel;
    c[ImGuiCol_PopupBg] = kPanel;
    c[ImGuiCol_Border] = kBorder;
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = kPanelAlt;
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.14f, 0.14f, 0.14f, 1.0f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.17f, 0.17f, 0.17f, 1.0f);
    c[ImGuiCol_Button] = kPanelAlt;
    c[ImGuiCol_ButtonHovered] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
    c[ImGuiCol_ButtonActive] = ImVec4(accent.x, accent.y, accent.z, 0.55f);
    c[ImGuiCol_Header] = ImVec4(accent.x, accent.y, accent.z, 0.30f);
    c[ImGuiCol_HeaderHovered] = ImVec4(accent.x, accent.y, accent.z, 0.45f);
    c[ImGuiCol_HeaderActive] = ImVec4(accent.x, accent.y, accent.z, 0.60f);
    c[ImGuiCol_CheckMark] = accent;
    c[ImGuiCol_SliderGrab] = accent;
    c[ImGuiCol_SliderGrabActive] = accent;
    c[ImGuiCol_Separator] = kBorder;
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = kPanelAlt;
    c[ImGuiCol_TableHeaderBg] = kPanelAlt;
    c[ImGuiCol_TableBorderStrong] = kBorder;
    c[ImGuiCol_TableBorderLight] = kBorder;
    c[ImGuiCol_TableRowBgAlt] = ImVec4(1, 1, 1, 0.02f);
    c[ImGuiCol_NavCursor] = accent;
    c[ImGuiCol_Tab] = kPanelAlt;
    c[ImGuiCol_TabHovered] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
    c[ImGuiCol_TabSelected] = ImVec4(accent.x, accent.y, accent.z, 0.25f);
    c[ImGuiCol_TabSelectedOverline] = accent;
    c[ImGuiCol_TabDimmed] = kPanelAlt;
    c[ImGuiCol_TabDimmedSelected] = ImVec4(accent.x, accent.y, accent.z, 0.20f);
    c[ImGuiCol_PlotLines] = accent;
    c[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
}

namespace {

// Cada contexto ImGui (janela principal e overlay) tem o próprio atlas de fontes.
struct ContextFonts {
    ImGuiContext* context = nullptr;
    FontSet fonts;
};
ContextFonts g_fonts[4];
const FontSet kNoFonts{};

FontSet& FontsFor(ImGuiContext* ctx) {
    for (ContextFonts& f : g_fonts) {
        if (f.context == ctx || f.context == nullptr) {
            f.context = ctx;
            return f.fonts;
        }
    }
    return g_fonts[0].fonts;
}

bool FileExists(const char* path) {
    FILE* f = nullptr;
#ifdef _MSC_VER
    if (fopen_s(&f, path, "rb") != 0) {
        f = nullptr;
    }
#else
    f = std::fopen(path, "rb");
#endif
    if (f == nullptr) {
        return false;
    }
    std::fclose(f);
    return true;
}

ImFont* AddFont(const char* path, float size) {
    ImGuiIO& io = ImGui::GetIO();
    if (FileExists(path)) {
        return io.Fonts->AddFontFromFileTTF(path, size, nullptr, io.Fonts->GetGlyphRangesDefault());
    }
    ImFontConfig cfg;
    cfg.SizePixels = size;
    return io.Fonts->AddFontDefault(&cfg);
}

}  // namespace

void LoadFonts(float scale) {
    const char* regular = "C:\\Windows\\Fonts\\segoeui.ttf";
    const char* bold = "C:\\Windows\\Fonts\\segoeuib.ttf";
    FontSet& fonts = FontsFor(ImGui::GetCurrentContext());
    fonts.regular = AddFont(regular, 17.0f * scale);  // primeira fonte = padrão do ImGui
    fonts.bold = AddFont(bold, 17.0f * scale);
    fonts.caption = AddFont(bold, 13.0f * scale);
    fonts.title = AddFont(bold, 26.0f * scale);
}

const FontSet& Fonts() {
    ImGuiContext* ctx = ImGui::GetCurrentContext();
    for (const ContextFonts& f : g_fonts) {
        if (f.context == ctx) {
            return f.fonts;
        }
    }
    return kNoFonts;
}

}  // namespace st::theme
