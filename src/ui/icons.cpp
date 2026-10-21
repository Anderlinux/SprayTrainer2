#include "ui/icons.h"

#include "ui/theme.h"

#include <initializer_list>

namespace st::icons {

namespace {

ImVec2 P(ImVec2 c, float r, float x, float y) {
    return ImVec2(c.x + x * r, c.y + y * r);
}

}  // namespace

void Draw(ImDrawList* dl, Icon icon, ImVec2 c, float size, ImU32 col) {
    const float r = size * 0.5f;
    const float t = size >= 22.0f ? 2.0f : 1.6f;
    const ImU32 hole = theme::U32(theme::kBg);

    switch (icon) {
        case Icon::Home: {
            dl->AddTriangleFilled(P(c, r, -0.95f, -0.05f), P(c, r, 0.0f, -0.9f), P(c, r, 0.95f, -0.05f), col);
            dl->AddRectFilled(P(c, r, -0.65f, -0.1f), P(c, r, 0.65f, 0.85f), col, 2.0f);
            dl->AddRectFilled(P(c, r, -0.18f, 0.3f), P(c, r, 0.18f, 0.85f), hole, 1.0f);
            break;
        }
        case Icon::Sliders: {
            const float ys[3] = {-0.6f, 0.0f, 0.6f};
            const float knobs[3] = {0.35f, -0.4f, 0.15f};
            for (int i = 0; i < 3; ++i) {
                dl->AddLine(P(c, r, -0.9f, ys[i]), P(c, r, 0.9f, ys[i]), col, t);
                dl->AddCircleFilled(P(c, r, knobs[i], ys[i]), r * 0.22f, col);
                dl->AddCircleFilled(P(c, r, knobs[i], ys[i]), r * 0.1f, hole);
            }
            break;
        }
        case Icon::Palette: {
            dl->AddCircleFilled(c, r * 0.9f, col);
            dl->AddCircleFilled(P(c, r, -0.35f, -0.35f), r * 0.15f, hole);
            dl->AddCircleFilled(P(c, r, 0.15f, -0.5f), r * 0.15f, hole);
            dl->AddCircleFilled(P(c, r, 0.5f, -0.1f), r * 0.15f, hole);
            dl->AddCircleFilled(P(c, r, -0.45f, 0.2f), r * 0.15f, hole);
            dl->AddCircleFilled(P(c, r, 0.2f, 0.45f), r * 0.24f, hole);
            break;
        }
        case Icon::History:
        case Icon::Clock: {
            dl->AddCircle(c, r * 0.85f, col, 0, t);
            dl->AddLine(c, P(c, r, 0.0f, -0.5f), col, t);
            dl->AddLine(c, P(c, r, 0.4f, 0.2f), col, t);
            if (icon == Icon::History) {
                dl->AddTriangleFilled(P(c, r, -1.0f, -0.25f), P(c, r, -0.65f, -0.25f), P(c, r, -0.82f, 0.05f), col);
            }
            break;
        }
        case Icon::Folder: {
            dl->AddRectFilled(P(c, r, -0.95f, -0.75f), P(c, r, -0.1f, -0.35f), col, 2.0f);
            dl->AddRectFilled(P(c, r, -0.95f, -0.5f), P(c, r, 0.95f, 0.75f), col, 2.5f);
            dl->AddLine(P(c, r, -0.8f, -0.25f), P(c, r, 0.8f, -0.25f), hole, 1.5f);
            break;
        }
        case Icon::Trash: {
            dl->AddLine(P(c, r, -0.85f, -0.6f), P(c, r, 0.85f, -0.6f), col, t);
            dl->AddRectFilled(P(c, r, -0.3f, -0.9f), P(c, r, 0.3f, -0.6f), col, 1.0f);
            dl->AddRectFilled(P(c, r, -0.65f, -0.45f), P(c, r, 0.65f, 0.9f), col, 2.0f);
            for (float x : {-0.3f, 0.0f, 0.3f}) {
                dl->AddLine(P(c, r, x, -0.25f), P(c, r, x, 0.65f), hole, 1.5f);
            }
            break;
        }
        case Icon::Pencil: {
            dl->AddLine(P(c, r, -0.55f, 0.55f), P(c, r, 0.6f, -0.6f), col, r * 0.45f);
            dl->AddTriangleFilled(P(c, r, -0.9f, 0.9f), P(c, r, -0.75f, 0.35f), P(c, r, -0.35f, 0.75f), col);
            break;
        }
        case Icon::Mouse: {
            dl->AddRect(P(c, r, -0.55f, -0.9f), P(c, r, 0.55f, 0.9f), col, r * 0.55f, 0, t);
            dl->AddLine(P(c, r, 0.0f, -0.6f), P(c, r, 0.0f, -0.2f), col, t);
            break;
        }
        case Icon::Target: {
            dl->AddCircle(c, r * 0.85f, col, 0, t);
            dl->AddCircle(c, r * 0.45f, col, 0, t);
            dl->AddCircleFilled(c, r * 0.14f, col);
            break;
        }
        case Icon::Eye:
        case Icon::EyeOff: {
            dl->PathArcTo(P(c, r, 0.0f, 0.9f), r * 1.25f, -2.35f, -0.79f, 16);
            dl->PathStroke(col, 0, t);
            dl->PathArcTo(P(c, r, 0.0f, -0.9f), r * 1.25f, 0.79f, 2.35f, 16);
            dl->PathStroke(col, 0, t);
            dl->AddCircleFilled(c, r * 0.3f, col);
            if (icon == Icon::EyeOff) {
                dl->AddLine(P(c, r, -0.85f, -0.85f), P(c, r, 0.85f, 0.85f), col, t);
            }
            break;
        }
        case Icon::ChevronUp:
            dl->AddLine(P(c, r, -0.6f, 0.3f), P(c, r, 0.0f, -0.3f), col, t);
            dl->AddLine(P(c, r, 0.0f, -0.3f), P(c, r, 0.6f, 0.3f), col, t);
            break;
        case Icon::ChevronDown:
            dl->AddLine(P(c, r, -0.6f, -0.3f), P(c, r, 0.0f, 0.3f), col, t);
            dl->AddLine(P(c, r, 0.0f, 0.3f), P(c, r, 0.6f, -0.3f), col, t);
            break;
        case Icon::Window:
            dl->AddRect(P(c, r, -0.85f, -0.7f), P(c, r, 0.85f, 0.7f), col, 2.0f, 0, t);
            dl->AddLine(P(c, r, -0.85f, -0.35f), P(c, r, 0.85f, -0.35f), col, t);
            break;
        case Icon::Refresh: {
            dl->PathArcTo(c, r * 0.7f, 0.6f, 5.4f, 20);
            dl->PathStroke(col, 0, t);
            dl->AddTriangleFilled(P(c, r, 0.35f, -0.85f), P(c, r, 0.95f, -0.55f), P(c, r, 0.4f, -0.2f), col);
            break;
        }
    }
}

}  // namespace st::icons
