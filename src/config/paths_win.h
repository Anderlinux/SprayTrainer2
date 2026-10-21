#pragma once

#include <filesystem>

namespace st {

// Pasta de dados do app:
// - modo portátil (existe "portable.txt" ao lado do .exe): <pasta do exe>\data
// - normal: %APPDATA%\SprayTrainer
std::filesystem::path DataDir();

}  // namespace st
