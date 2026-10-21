#pragma once

#include "ui/app_state.h"

namespace st {

struct PatternEditorLayout {
    bool stacked = false;       // gráfico em cima e tabela embaixo (overlay)
    float plotHeight = 180.0f;  // usado só no modo empilhado
    float scale = 1.0f;
};

// Tabela editável (31 linhas × X/Y) + gráfico da trajetória + Salvar/Desfazer/Restaurar.
// As edições valem na hora: a próxima rajada já usa o padrão novo.
void PatternEditor(AppState& state, Weapon weapon, const PatternEditorLayout& layout);

// true se o padrão da arma difere da última versão salva.
bool PatternDirty(const AppState& state, Weapon weapon);

}  // namespace st
