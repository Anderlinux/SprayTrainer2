#pragma once

#include "core/training_session.h"
#include "ui/app_state.h"

#include <imgui.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace st {

struct PlotOptions {
    int highlight = -1;                   // índice do ponto ideal destacado (editor)
    std::size_t activeCount = SIZE_MAX;   // pontos a partir deste índice ficam esmaecidos
    std::size_t guideCount = SIZE_MAX;    // quantos pontos da linha ideal mostrar (0 = escondida)
    bool grid = false;                    // grade de 1° (ou 2°/5° se ficar apertada)
    bool shotNumbers = false;             // numera os tiros 1, 5, 10, 15...
    float lineWidth = 2.0f;
    int playerColor = 0;                  // índice em theme::kPlayerColors
};

// Opções de desenho vindas da aba Visual (sem a regra da guia, que depende da rajada).
PlotOptions PlotOptionsFrom(const VisualSettings& visual);

// Legenda: amostra da cor da série + texto neutro (a cor nunca fica sozinha no texto).
void PlotLegend(const VisualSettings& visual, const char* idealLabel = "Ideal", const char* playerLabel = "Você");

// Desenha a trajetória ideal (roxo) e a do jogador (ciano) dentro do retângulo [a, b].
// `live` é a posição atual do jogador durante a rajada (opcional).
void DrawSprayPlot(ImDrawList* dl, ImVec2 a, ImVec2 b, const std::vector<Vec2>& ideal,
                   const std::vector<ShotResult>& shots, const Vec2* live, const PlotOptions& options = {});

}  // namespace st
