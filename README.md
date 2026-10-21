# Spray Trainer2

**Funcionalidades:** treino com nota e meta, modo desafio, 13 armas com silhueta, editor de
padrões (31 pontos X/Y) com gravação desenhando com o mouse, overlay sempre no topo, histórico
com gráfico e replay, perfis em JSON (carrega o último ao abrir), tema escuro com cor de
destaque e escala da interface.

---

## Sumário

1. [Linguagens e tecnologias](#1-linguagens-e-tecnologias)
2. [Organização do diretório](#2-organização-do-diretório)
3. [O que precisa para rodar e para compilar](#3-o-que-precisa-para-rodar-e-para-compilar)
4. [Como compilar](#4-como-compilar)
5. [Como a compilação funciona](#5-como-a-compilação-funciona)
6. [Testes](#6-testes)
7. [Onde o app guarda os dados](#7-onde-o-app-guarda-os-dados)
8. [Problemas comuns](#8-problemas-comuns)

---

## 1. Linguagens e tecnologias

### Linguagens

| Linguagem | Onde | Para quê |
|---|---|---|
| **C++20** | `src/`, `tests/` | Todo o programa: lógica de treino, interface, overlay, perfis. Usa recursos do C++17/20 como `std::filesystem`, `std::u8string` (caminhos com acento) e `std::to_chars`. |
| **CMake** | `CMakeLists.txt`, `CMakePresets.json` | Script de build: baixa as bibliotecas, define os alvos, as flags de compilação e os testes. |
| **Resource Script do Windows (`.rc`)** | `resources/app.rc` | Embute no `.exe` o ícone, as 13 silhuetas das armas (PNG) e as informações de versão. |
| **JSON** | `tests/data/`, perfis e histórico do usuário | Formato dos perfis (`profiles/*.json`), do histórico (`history.json`) e das configurações (`settings.json`). |
| **HLSL** *(indireto)* | dentro do Dear ImGui | Os shaders do DirectX 11 vêm prontos no backend do ImGui e são compilados na hora pelo `d3dcompiler`. Nenhum shader foi escrito neste projeto. |

O código e os comentários estão em português; todos os arquivos são **UTF-8** (o MSVC compila
com `/utf-8`).

### Bibliotecas de terceiros

Baixadas automaticamente pelo CMake no primeiro build (não ficam no repositório):

| Biblioteca | Versão | Uso | Licença |
|---|---|---|---|
| [Dear ImGui](https://github.com/ocornut/imgui) | 1.91.5 | Toda a interface (janelas, botões, tabelas, gráficos desenhados) | MIT |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 | Ler e gravar os perfis e o histórico | MIT |

### APIs do Windows

| API | Uso |
|---|---|
| **Win32** | Janela sem moldura, barra de título própria, redimensionar, mensagens |
| **DirectX 11 / DXGI** | Desenho da janela principal e do overlay (um device compartilhado, um swap chain para cada) |
| **Raw Input** (`WM_INPUT`) | Leitura do movimento do mouse em contagens brutas, só com a janela em foco |
| **WIC** (Windows Imaging Component) | Decodificar os PNGs embutidos das armas |
| **DWM** | Cantos arredondados no Windows 11 |
| **Shell** (`SHGetKnownFolderPath`, `ShellExecute`) | Pasta `%APPDATA%` e botão "Abrir pasta" |
| **Common Dialogs** (`GetOpenFileName`/`GetSaveFileName`) | Importar e exportar perfis |
| **Per-monitor DPI** | Interface nítida em telas com escala 125%, 150%... |

---

## 2. Organização do diretório

```
SprayTrainer/
├── CMakeLists.txt          Script de build (bibliotecas, alvos, flags, testes)
├── CMakePresets.json       Preset "vs2022-x64" (Visual Studio 2022, x64, warnings como erro)
├── README.md               Este arquivo
├── .gitignore              Ignora build/, out/, .vs/
│
├── resources/              Tudo que vai embutido dentro do .exe
│   ├── app.rc              Script de recursos: ícone, PNGs das armas, versão 1.0.0
│   ├── resource.h          IDs numéricos dos recursos (usados no .rc e no C++)
│   ├── app.ico             Ícone do app (16 a 256 px)
│   └── weapons/            Silhuetas PNG com fundo transparente (13 armas)
│       ├── ak47.png  m4a4.png  m4a1s.png  famas.png  galilar.png
│       ├── mac10.png  mp9.png  mp5sd.png  mp7.png  p90.png
│       └── bizon.png  sg553.png  ump45.png
│
├── src/
│   ├── main.cpp            Ponto de entrada (wWinMain): inicia COM e o App
│   │
│   ├── core/               NÚCLEO — lógica pura, sem Windows nem interface (testável)
│   │   ├── weapon.*            As 13 armas: nome, cadência, pente e padrões de fábrica
│   │   ├── defaults.h          Valores de fábrica do treino (arma, sensibilidade, meta)
│   │   ├── training_session.*  Uma rajada: tempo de cada tiro, posição do mouse, erro e nota
│   │   ├── pattern_store.*     Padrões das armas em memória, protegidos por mutex
│   │   ├── pattern_recording.* Converte um desenho feito com o mouse em padrão de recuo
│   │   ├── history.*           Histórico de rajadas (últimas 300) e estatísticas
│   │   └── spsc_ring.h         Fila sem trava (lock-free) para os movimentos do mouse
│   │
│   ├── config/             DADOS EM DISCO — perfis, histórico e pasta de dados
│   │   ├── profile.*           Formato JSON do perfil e validação (o que aceita/recusa)
│   │   ├── profile_store.*     Salvar, carregar, renomear, duplicar, excluir, importar/exportar
│   │   ├── history_json.*      Formato JSON do histórico
│   │   └── paths_win.*         Onde ficam os dados (%APPDATA% ou modo portátil) — só Windows
│   │
│   ├── input/              ENTRADA
│   │   └── raw_input.*         Registro do Raw Input, leitura do WM_INPUT e relógio (QPC)
│   │
│   ├── ui/                 INTERFACE — janela principal
│   │   ├── app.*               Janela Win32, DirectX 11, loop principal, DPI e escala
│   │   ├── app_state.h         Estado único do app (compartilhado entre telas e overlay)
│   │   ├── main_window.*       Barra de título, barra lateral, aviso do topo, troca de páginas
│   │   ├── page_principal.cpp  Tela Principal (status, arma, área de treino, resultado)
│   │   ├── page_ajustes.cpp    Ajustes: Padrões de recuo, Treino, Overlay, Interface
│   │   ├── page_visual.cpp     Aparência do gráfico (guia, cores, grade, números)
│   │   ├── page_historico.cpp  Histórico: filtro, números, gráfico da nota, tabela, replay
│   │   ├── page_presets.cpp    Perfis: lista, salvar, carregar, importar/exportar
│   │   ├── pages.h             Declaração das páginas
│   │   ├── pattern_editor.*    Editor de padrões (tabela 31×X/Y, gráfico, gravar com o mouse)
│   │   ├── profiles.*          Ações de perfil e os diálogos (nome, excluir, sair sem salvar)
│   │   ├── spray_plot.*        Desenho do gráfico do spray (linha ideal × sua linha)
│   │   ├── widgets.*           Componentes: interruptor, botões, cartões, seletor de arma
│   │   ├── icons.*             Ícones vetoriais (casa, lixeira, olho, mouse...)
│   │   ├── theme.*             Cores, fontes e estilo do tema escuro
│   │   ├── textures.*          Carrega os PNGs embutidos para o DirectX (via WIC)
│   │   ├── swap_target.*       Swap chain + render target (um por janela)
│   │   └── file_dialog.*       Diálogos de abrir/salvar arquivo
│   │
│   └── overlay/            OVERLAY — janela pequena sempre no topo
│       ├── overlay.*           Janela, clique que atravessa, posição, opacidade, escala
│       └── overlay_ui.*        Conteúdo: modo compacto e expandido (Treino / Padrão)
│
└── tests/                  TESTES AUTOMÁTICOS (rodam em qualquer sistema)
    ├── core_tests.cpp      Núcleo: padrões, sessão, nota, fila, gravação de padrão
    ├── config_tests.cpp    Perfis, validação, nomes, armazenamento, histórico
    └── data/
        └── padrao.json     Perfil de referência: o padrão de fábrica tem que ser igual a ele
```

### Camadas

```
            ┌──────────────────────────────────────────────┐
            │  ui/  e  overlay/   (Dear ImGui + DirectX 11) │  ← só Windows
            └───────────────┬──────────────────────────────┘
                            │ usa
            ┌───────────────┴───────────┐   ┌──────────────┐
            │  config/  (perfis, JSON)  │   │  input/      │  ← Raw Input (Windows)
            └───────────────┬───────────┘   └──────┬───────┘
                            │ usa                  │ alimenta
            ┌───────────────┴──────────────────────┴───────┐
            │  core/   (armas, rajada, nota, histórico)     │  ← C++ puro, testado
            └──────────────────────────────────────────────┘
```

- **`core/`** e **`config/`** (exceto `paths_win`) não dependem de Windows nem da interface:
  por isso os testes rodam em qualquer sistema.
- **Fluxo do mouse:** `WM_INPUT` → `raw_input` → fila lock-free (`spsc_ring`) → a cada frame a
  `training_session` consome a fila com o instante de cada movimento e registra os tiros no tempo
  certo.
- **Estado:** a janela principal e o overlay rodam na mesma thread e leem o mesmo `AppState`,
  então o que muda num lado aparece no outro.

---

## 3. O que precisa para rodar e para compilar

### Para **rodar** o `SprayTrainer.exe`

| Item | Detalhe |
|---|---|
| Sistema | **Windows 10 ou Windows 11, 64 bits** (a escala por monitor funciona por completo a partir do Windows 10 versão 1703) |
| Placa de vídeo | Compatível com **DirectX 11** (qualquer uma dos últimos ~10 anos). Sem ela, o app tenta o renderizador por software do Windows (WARP). |
| Visual C++ Redistributable | **Microsoft Visual C++ Redistributable 2015–2022 (x64)**, se o exe foi compilado com o Visual Studio. Quase todo PC já tem; se aparecer erro de `VCRUNTIME140.dll` ou `MSVCP140.dll`, instale pelo site da Microsoft. |
| Fontes | Segoe UI (já vem no Windows) |

Não precisa instalar mais nada: as imagens, o ícone e as bibliotecas vão dentro do `.exe`.

### Para **compilar** no Windows 10/11

| Programa | Versão | Para quê | Onde baixar |
|---|---|---|---|
| **Visual Studio 2022** (Community é grátis) | 17.x | Compilador C++ (MSVC v143), Windows SDK e CMake | visualstudio.microsoft.com |
| ↳ carga de trabalho **"Desenvolvimento para desktop com C++"** | — | Instala o MSVC, o **Windows 10/11 SDK** e as **Ferramentas CMake do C++ para Windows** | marcar no Visual Studio Installer |
| **Git** | 2.x | O CMake usa o Git para baixar o Dear ImGui | git-scm.com |
| **Internet** | — | Só no **primeiro** build (download do ImGui e do nlohmann/json) | — |

O **CMake** (3.22 ou mais novo) já vem com o Visual Studio. Se preferir não instalar o Visual
Studio inteiro, as **Build Tools for Visual Studio 2022** (com a mesma carga de trabalho) + o
**CMake** avulso também servem.

> Alternativa: dá para compilar com **MinGW-w64** (GCC). É assim que o projeto é verificado fora
> do Windows; ver [5.4](#54-compilando-com-mingw-w64-alternativa).

---

## 4. Como compilar

### 4.1 Pelo terminal (recomendado)

Abra o **Developer PowerShell for VS 2022** (menu Iniciar), entre na pasta do projeto e rode:

```powershell
git clone https://github.com/Anderlinux/SprayTrainer
cd SprayTrainer

cmake --preset vs2022-x64          # 1. configura (baixa ImGui e nlohmann/json na 1ª vez)
cmake --build --preset release     # 2. compila em Release
ctest --preset release             # 3. roda os testes (opcional)
```

Resultado: **`build\vs2022-x64\Release\SprayTrainer.exe`**

Para uma versão de depuração: `cmake --build --preset debug` →
`build\vs2022-x64\Debug\SprayTrainer.exe`.

### 4.2 Pelo Visual Studio (sem terminal)

1. **Arquivo → Abrir → Pasta...** e escolha a pasta do projeto.
2. O Visual Studio lê o `CMakePresets.json` e configura sozinho (aguarde o "CMake generation
   finished" na janela Saída).
3. No topo, escolha a configuração **Release** e o alvo **SprayTrainer.exe**.
4. **Compilar → Compilar Tudo** (Ctrl+Shift+B) e depois **F5** para rodar.

### 4.3 Atualizar e recompilar

```powershell
git pull
cmake --build --preset release
```

Para começar do zero: apague a pasta `build\` e rode os três comandos de novo.

---

## 5. Como a compilação funciona

### 5.1 Etapas

1. **Configurar** (`cmake --preset vs2022-x64`)
   - Gera uma solução do Visual Studio 2022, arquitetura **x64**, em `build/vs2022-x64/`.
   - Baixa o **nlohmann/json 3.11.3** pelo pacote oficial da release, **conferindo o hash
     SHA-256** (se o arquivo for adulterado, o build para).
   - Baixa o **Dear ImGui 1.91.5** com `git clone` na tag `v1.91.5`.
   - Liga `ST_WARNINGS_AS_ERRORS` (qualquer warning vira erro).
2. **Compilar** (`cmake --build --preset release`): gera as bibliotecas e o executável na ordem
   abaixo.
3. **Recursos:** o `rc.exe` compila o `resources/app.rc`, embutindo ícone, PNGs e versão.
4. **Link:** junta tudo num único `SprayTrainer.exe` com as bibliotecas do Windows.

### 5.2 Alvos do CMake

| Alvo | Tipo | Conteúdo | Depende de |
|---|---|---|---|
| `st_core` | biblioteca estática | `src/core/*` | — |
| `st_config` | biblioteca estática | `src/config/*` (sem `paths_win`) | `st_core`, nlohmann/json |
| `imgui` | biblioteca estática | Dear ImGui + backends Win32 e DX11 | — |
| `SprayTrainer` | executável Windows (`WIN32`) | `src/main.cpp`, `ui/`, `overlay/`, `input/`, `paths_win`, `app.rc` | os três acima |
| `st_core_tests` | executável de teste | `tests/core_tests.cpp` | `st_core` |
| `st_config_tests` | executável de teste | `tests/config_tests.cpp` | `st_config` |

O `SprayTrainer` só é gerado no Windows (`if(WIN32)`); os testes, em qualquer sistema.

### 5.3 Flags e opções

| Item | Valor | Por quê |
|---|---|---|
| Padrão C++ | **C++20**, sem extensões do compilador | código portável |
| Avisos (MSVC) | `/W4 /permissive- /utf-8` (+ `/WX` no preset) | nível alto de avisos, C++ padrão estrito, fontes em UTF-8 |
| Avisos (GCC/Clang) | `-Wall -Wextra -Wpedantic -Wshadow -Wconversion` (+ `-Werror`) | idem |
| Código de terceiros | ImGui com avisos desligados e includes como `SYSTEM` | os avisos dele não contam para o "zero warnings" |
| Definições | `UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN` | APIs Unicode do Windows, sem macros `min/max` |
| Bibliotecas do Windows | `d3d11 d3dcompiler dxgi dwmapi windowscodecs ole32 shell32 comdlg32` | DirectX, DWM, WIC, COM, Shell, diálogos |

Opções que você pode mudar na configuração:

| Opção | Padrão | Efeito |
|---|---|---|
| `ST_BUILD_TESTS` | `ON` | Compila os testes |
| `ST_WARNINGS_AS_ERRORS` | `OFF` (o preset liga) | Trata avisos como erro |

Exemplo: `cmake --preset vs2022-x64 -DST_BUILD_TESTS=OFF`

### 5.4 Compilando com MinGW-w64 (alternativa)

O projeto também compila com **MinGW-w64** (GCC 13), no Windows (MSYS2) ou a partir do Linux
(cross-compile). Nesse caso o exe sai **estático** (`-static`) e não precisa do Visual C++
Redistributable.

```bash
# Linux (Ubuntu/Debian): sudo apt install g++-mingw-w64-x86-64-posix cmake git
cat > mingw.cmake <<'EOF'
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++-posix)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
EOF
cmake -S . -B build/mingw -DCMAKE_TOOLCHAIN_FILE=mingw.cmake -DCMAKE_BUILD_TYPE=Release -DST_WARNINGS_AS_ERRORS=ON
cmake --build build/mingw -j
# resultado: build/mingw/SprayTrainer.exe
```

---

## 6. Testes

```powershell
ctest --preset release            # Windows, depois do build
```

```bash
cmake -S . -B build && cmake --build build && ctest --test-dir build   # qualquer sistema
```

| Teste | Cobre |
|---|---|
| `core` | padrões de fábrica, sessão de treino (jogador perfeito × parado), régua da nota, fila lock-free, conversão desenho → padrão (suavização, limite de 10°) |
| `config` | perfil ida e volta, cada motivo de recusa, avisos, nomes proibidos no Windows, gravação atômica, histórico (limite de 300, entradas inválidas), **padrão de fábrica idêntico a `tests/data/padrao.json`** |

---

## 7. Onde o app guarda os dados

| Modo | Pasta |
|---|---|
| Normal | `%APPDATA%\SprayTrainer\` |
| Portátil | `data\` ao lado do `SprayTrainer.exe` (crie um arquivo vazio `portable.txt` ao lado do exe) |

| Arquivo | Conteúdo |
|---|---|
| `profiles\<nome>.json` | um perfil por arquivo (legível e editável) |
| `settings.json` | último perfil usado (carregado ao abrir) |
| `history.json` | histórico de rajadas |

Os valores de fábrica (13 armas, UMP-45, sensibilidade 1.35, meta 60) vêm do perfil
`tests/data/padrao.json`, copiado para `src/core/weapon.cpp` e `src/core/defaults.h`.

---

## 8. Problemas comuns

| Sintoma | Solução |
|---|---|
| `cmake` ou `cl` não é reconhecido | Use o **Developer PowerShell for VS 2022**, não o PowerShell comum. |
| `Could not find git` / erro ao baixar o ImGui | Instale o Git, confira a internet e rode o `cmake --preset` de novo. |
| `No CMAKE_CXX_COMPILER could be found` ou SDK ausente | No Visual Studio Installer, marque **"Desenvolvimento para desktop com C++"** (inclui o Windows SDK). |
| Build para num *warning* | É de propósito (`/WX`). Corrija o aviso ou configure com `-DST_WARNINGS_AS_ERRORS=OFF`. |
| Mudou de versão do Visual Studio / erro estranho de cache | Apague a pasta `build\` e configure de novo. |
| Ao abrir: falta `VCRUNTIME140.dll` | Instale o **Visual C++ Redistributable 2015–2022 (x64)**. |
| Ao abrir: "Não foi possível iniciar o DirectX 11" | Atualize o driver de vídeo. |
| Aviso de perfil corrompido | Corrija o `.json` (o aviso diz o campo) ou exclua-o em Presets; o arquivo não é alterado. |
