#pragma once

#include "config/profile.h"
#include "core/history.h"
#include "core/vision/weapon_images.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace st {

struct StoreResult {
    bool ok = true;
    std::string error;

    static StoreResult Fail(std::string message) { return {false, std::move(message)}; }
};

// Perfis em disco: <raiz>/profiles/<nome>.json e <raiz>/settings.json (último perfil usado).
// Nomes são UTF-8; gravação é atômica (arquivo temporário + renomear).
class ProfileStore {
public:
    explicit ProfileStore(std::filesystem::path root);

    const std::filesystem::path& Root() const { return root_; }
    std::filesystem::path ProfilesDir() const { return root_ / "profiles"; }

    // Cria as pastas se não existirem.
    StoreResult EnsureDirs() const;

    std::vector<std::string> List() const;  // nomes em ordem alfabética
    bool Exists(const std::string& name) const;

    StoreResult Load(const std::string& name, ProfileData& out, std::vector<std::string>* warnings = nullptr) const;
    StoreResult Save(const std::string& name, const ProfileData& data) const;
    StoreResult Delete(const std::string& name) const;
    StoreResult Rename(const std::string& from, const std::string& to) const;
    StoreResult Duplicate(const std::string& from, const std::string& to) const;

    // Imagens de referência do reconhecimento automático (seção "weaponImages").
    // Ficam num arquivo separado por perfil (<raiz>/profiles/<nome>.images.json),
    // gravado atomicamente como o próprio perfil. Separado porque a camada de
    // configuração não conhece visão; a UI chama estas funções junto com
    // Save/Load/Duplicate/Rename/Delete. Perfil sem arquivo = lista vazia.
    StoreResult SaveWeaponImages(const std::string& name, const std::vector<WeaponImageEntry>& entries) const;
    StoreResult LoadWeaponImages(const std::string& name, std::vector<WeaponImageEntry>& out,
                                 std::vector<std::string>* warnings = nullptr) const;

private:
    std::filesystem::path ImagesFileFor(const std::string& name) const;

public:

    // Último perfil usado (settings.json). Vazio se não houver.
    std::string LastProfile() const;
    StoreResult SetLastProfile(const std::string& name) const;

    // Histórico de rajadas (<raiz>/history.json). Se o arquivo estiver inválido, ele é
    // renomeado para history.invalid.json (para não ser sobrescrito) e o erro é devolvido.
    StoreResult LoadHistory(std::vector<BurstRecord>& out, int* skipped = nullptr) const;
    StoreResult SaveHistory(const std::vector<BurstRecord>& entries) const;

    // "Base", "Base (2)", "Base (3)"... o primeiro que ainda não existe.
    std::string UniqueName(const std::string& base) const;

    // Arquivos avulsos (exportar/importar).
    static StoreResult WriteProfileFile(const std::filesystem::path& path, const ProfileData& data);
    static StoreResult ReadProfileFile(const std::filesystem::path& path, ProfileData& out,
                                       std::vector<std::string>* warnings = nullptr);

    // Valida um nome de perfil; devolve a mensagem de erro, ou nada se for válido.
    static std::optional<std::string> ValidateName(const std::string& name);
    // Ajusta um texto qualquer (ex.: nome de arquivo importado) para um nome válido.
    static std::string SanitizeName(const std::string& text);

    static std::filesystem::path PathFromUtf8(const std::string& utf8);
    static std::string PathToUtf8(const std::filesystem::path& path);

private:
    std::filesystem::path FileFor(const std::string& name) const;

    std::filesystem::path root_;
};

}  // namespace st
