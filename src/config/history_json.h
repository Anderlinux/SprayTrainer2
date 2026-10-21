#pragma once

#include "core/history.h"

#include <string>
#include <vector>

namespace st {

std::string HistoryToJson(const std::vector<BurstRecord>& entries);

// Lê o histórico. Entradas individuais inválidas são ignoradas (contadas em `skipped`);
// só falha se o arquivo inteiro for inválido.
bool HistoryFromJson(const std::string& text, std::vector<BurstRecord>& out, std::string& error, int* skipped = nullptr);

}  // namespace st
