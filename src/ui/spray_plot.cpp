#include "ui/spray_plot.h"

#include "ui/theme.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace st {

namespace {

struct PlotMap {
    float minX;
    float minY;
    float scale;
    ImVec2 offset;

    ImVec2 operator()(Vec2 p) const {
        return ImVec2(offset.x + (p.x - minX) * scale, offset.y + (p.y - minY) * scale);
    }
};

PlotMap FitPlot(ImVec2 a, ImVec2 b, const std::vector<Vec2>& pts) {
    float minX = 0.0f, maxX = 0.0f, minY = 0.0f, maxY = 0.0f;
    for (const Vec2& p : pts) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }
    const float pad = 24.0f;
    const float rangeX = std::max(maxX - minX, 1.0f);
    const float rangeY = std::max(maxY - minY, 1.0f);
    const float scale =
        std::max(std::min((b.x - a.x - 2.0f * pad) / rangeX, (b.y - a.y - 2.0f * pad) / rangeY), 1.0f);
    const ImVec2 offset(a.x + ((b.x - a.x) - rangeX * scale) * 0.5f, a.y + ((b.y - a.y) - rangeY * scale) * 0.5f);
    return {minX, minY, scale, offset};
}

void DrawGrid(ImDrawList* dl, ImVec2 a, ImVec2 b, const PlotMap& map) {
    float step = 1.0f;  // graus
    while (map.scale * step < 14.0f) {
        step = step < 2.0f ? 2.0f : step * 2.5f;
    }
    const float x0 = map.minX + (a.x - map.offset.x) / map.scale;
    const float x1 = map.minX + (b.x - map.offset.x) / map.scale;
    const float y0 = map.minY + (a.y - map.offset.y) / map.scale;
    const float y1 = map.minY + (b.y - map.offset.y) / map.scale;
    const ImU32 col = theme::U32(theme::kBorder, 0.55f);
    for (float x = std::ceil(x0 / step) * step; x <= x1; x += step) {
        const float sx = map({x, 0.0f}).x;
        dl->AddLine(ImVec2(sx, a.y), ImVec2(sx, b.y), col, 1.0f);
    }
    for (float y = std::ceil(y0 / step) * step; y <= y1; y += step) {
        const float sy = map({0.0f, y}).y;
        dl->AddLine(ImVec2(a.x, sy), ImVec2(b.x, sy), col, 1.0f);
    }
}

}  // namespace

PlotOptions PlotOptionsFrom(const VisualSettings& visual) {
    PlotOptions o;
    o.grid = visual.grid;
    o.shotNumbers = visual.shotNumbers;
    o.lineWidth = visual.lineWidth;
    o.playerColor = visual.playerColor;
    return o;
}

void PlotLegend(const VisualSettings& visual, const char* idealLabel, const char* playerLabel) {
    const auto item = [](const ImVec4& color, const char* text) {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float cy = p.y + ImGui::GetTextLineHeight() * 0.5f;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddLine(ImVec2(p.x, cy), ImVec2(p.x + 18.0f, cy), theme::U32(color), 2.0f);
        dl->AddCircleFilled(ImVec2(p.x + 9.0f, cy), 4.0f, theme::U32(color));
        ImGui::Dummy(ImVec2(24.0f, ImGui::GetTextLineHeight()));
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::TextUnformatted(text);
    };
    item(theme::kChartIdeal, idealLabel);
    item(theme::kPlayerColors[std::clamp(visual.playerColor, 0, theme::kPlayerColorCount - 1)].color, playerLabel);
}

void DrawSprayPlot(ImDrawList* dl, ImVec2 a, ImVec2 b, const std::vector<Vec2>& ideal,
                   const std::vector<ShotResult>& shots, const Vec2* live, const PlotOptions& options) {
    if (ideal.empty()) {
        return;
    }
    // A escala usa o padrão inteiro mesmo com a guia escondida (o enquadramento não muda no meio).
    std::vector<Vec2> all = ideal;
    for (const ShotResult& s : shots) {
        all.push_back(s.player);
    }
    if (live != nullptr) {
        all.push_back(*live);
    }
    const PlotMap map = FitPlot(a, b, all);
    const float lw = options.lineWidth;
    const ImU32 idealCol = theme::U32(theme::kChartIdeal);
    const ImVec4 playerVec = theme::kPlayerColors[std::clamp(options.playerColor, 0, theme::kPlayerColorCount - 1)].color;
    const ImU32 playerCol = theme::U32(playerVec);

    dl->PushClipRect(a, b, true);
    if (options.grid) {
        DrawGrid(dl, a, b, map);
    }

    // Mira inicial
    const ImVec2 o = map({0.0f, 0.0f});
    dl->AddLine(ImVec2(o.x - 8.0f, o.y), ImVec2(o.x + 8.0f, o.y), theme::U32(theme::kTextDim), 1.0f);
    dl->AddLine(ImVec2(o.x, o.y - 8.0f), ImVec2(o.x, o.y + 8.0f), theme::U32(theme::kTextDim), 1.0f);

    std::vector<ImVec2> line;
    line.reserve(ideal.size());
    for (const Vec2& p : ideal) {
        line.push_back(map(p));
    }
    const std::size_t shown = std::min(options.guideCount, line.size());
    const std::size_t active = std::min(options.activeCount, shown);
    if (active < shown) {
        // Trecho fora do pente (ex.: M4A1-S usa 20 de 31 pontos).
        const std::size_t from = active > 0 ? active - 1 : 0;
        dl->AddPolyline(line.data() + from, static_cast<int>(shown - from), theme::U32(theme::kTextDim, 0.4f), 0,
                        lw * 0.75f);
        for (std::size_t i = active; i < shown; ++i) {
            dl->AddCircleFilled(line[i], 2.5f, theme::U32(theme::kTextDim, 0.5f));
        }
    }
    dl->AddPolyline(line.data(), static_cast<int>(active), theme::U32(theme::kChartIdeal, 0.85f), 0, lw);
    for (std::size_t i = 0; i < active; ++i) {
        dl->AddCircleFilled(line[i], 1.5f + lw * 0.75f, idealCol);
    }
    if (options.shotNumbers) {
        // Rótulos seletivos: 1, 5, 10, 15... (nunca um número em cada ponto).
        ImGui::PushFont(theme::Fonts().caption);
        for (std::size_t i = 0; i < shown; ++i) {
            if (i == 0 || (i + 1) % 5 == 0) {
                char label[8];
                std::snprintf(label, sizeof(label), "%zu", i + 1);
                dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(line[i].x + 7.0f, line[i].y - 7.0f),
                            theme::U32(theme::kTextDim), label);
            }
        }
        ImGui::PopFont();
    }
    if (options.highlight >= 0 && static_cast<std::size_t>(options.highlight) < shown) {
        const ImVec2 h = line[static_cast<std::size_t>(options.highlight)];
        dl->AddCircleFilled(h, 6.0f, theme::U32(theme::kChartIdeal, 0.35f));
        dl->AddCircle(h, 7.0f, theme::U32(theme::kText), 0, 2.0f);
    }

    if (!shots.empty() || live != nullptr) {
        line.clear();
        for (const ShotResult& s : shots) {
            line.push_back(map(s.player));
        }
        if (live != nullptr) {
            line.push_back(map(*live));
        }
        if (line.size() >= 2) {
            dl->AddPolyline(line.data(), static_cast<int>(line.size()), playerCol, 0, lw);
        }
        // Anel na cor do fundo separa os marcadores que se sobrepõem à linha ideal.
        for (const ShotResult& s : shots) {
            const ImVec2 p = map(s.player);
            dl->AddCircleFilled(p, 2.5f + lw * 0.75f, theme::U32(theme::kBg));
            dl->AddCircleFilled(p, 1.5f + lw * 0.75f, playerCol);
        }
    }
    if (live != nullptr) {
        dl->AddCircle(map(*live), 7.0f, playerCol, 0, 2.0f);
    }
    dl->PopClipRect();
}

}  // namespace st
