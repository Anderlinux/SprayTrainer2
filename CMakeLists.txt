cmake_minimum_required(VERSION 3.22)
project(SprayTrainer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ST_BUILD_TESTS "Compila os testes do núcleo" ON)
option(ST_WARNINGS_AS_ERRORS "Trata warnings como erro" OFF)

function(st_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        if(ST_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
        if(ST_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

include(FetchContent)

# nlohmann/json: pacote oficial da release (pequeno), com hash verificado.
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

# ------------------------------------------------------------------ núcleo (portável)
add_library(st_core STATIC
    src/core/history.cpp
    src/core/pattern_recording.cpp
    src/core/repetition.cpp
    src/core/weapon.cpp
    src/core/pattern_store.cpp
    src/core/training_session.cpp
    src/core/vision/auto_picker.cpp
    src/core/vision/image.cpp
    src/core/vision/matcher.cpp
    src/core/vision/png.cpp
    src/core/vision/weapon_images.cpp
    # Orquestração do modo automático: pura (captura injetada por função),
    # testável em Linux; no Windows o App injeta a captura real via BitBlt.
    src/ui/auto_vision.cpp
)
target_include_directories(st_core PUBLIC src)
# weapon_images usa nlohmann/json para a seção "weaponImages" do perfil.
# A biblioteca continua portável (sem Win32) — só depende do header-only json.
target_link_libraries(st_core PUBLIC nlohmann_json::nlohmann_json)
st_warnings(st_core)

# ------------------------------------------------------------------ perfis (portável)
add_library(st_config STATIC
    src/config/history_json.cpp
    src/config/profile.cpp
    src/config/profile_store.cpp
)
target_link_libraries(st_config PUBLIC st_core nlohmann_json::nlohmann_json)
st_warnings(st_config)

if(ST_BUILD_TESTS)
    enable_testing()
    add_executable(st_core_tests tests/core_tests.cpp)
    target_link_libraries(st_core_tests PRIVATE st_core)
    st_warnings(st_core_tests)
    add_test(NAME core COMMAND st_core_tests)

    # Testes do reconhecimento visual automático (downscale, template matching,
    # máquina de estados anti-repetição e decoder PNG). Núcleo puro, roda em qualquer plataforma.
    add_executable(st_vision_tests tests/vision_tests.cpp)
    target_link_libraries(st_vision_tests PRIVATE st_core nlohmann_json::nlohmann_json)
    st_warnings(st_vision_tests)
    add_test(NAME vision COMMAND st_vision_tests)

    add_executable(st_config_tests tests/config_tests.cpp)
    target_link_libraries(st_config_tests PRIVATE st_config)
    target_compile_definitions(st_config_tests PRIVATE ST_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    st_warnings(st_config_tests)
    add_test(NAME config COMMAND st_config_tests)

    # Testes da máquina de estados do replay (teclas de disparo/ativação/desativação).
    # Cobrem o comportamento que mantém as teclas configuráveis livres para o jogo:
    # quando o replay está desativado, a tecla de desativação não é consumida.
    add_executable(st_state_machine_tests tests/state_machine_tests.cpp)
    target_link_libraries(st_state_machine_tests PRIVATE st_core)
    st_warnings(st_state_machine_tests)
    add_test(NAME state_machine COMMAND st_state_machine_tests)
endif()

# ------------------------------------------------------------------ aplicativo (Windows)
if(WIN32)
    enable_language(RC)
    FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.91.5
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(imgui)

    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
    )
    # SYSTEM: warnings de código de terceiros não contam para o "zero warnings".
    target_include_directories(imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
    target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
    if(MSVC)
        target_compile_options(imgui PRIVATE /W0 /utf-8)
    endif()

    add_executable(SprayTrainer WIN32
        src/main.cpp
        src/config/paths_win.cpp
        src/input/raw_input.cpp
        src/overlay/overlay.cpp
        src/overlay/overlay_ui.cpp
        src/ui/app.cpp
        src/ui/icons.cpp
        src/ui/main_window.cpp
        src/ui/page_ajustes.cpp
        src/ui/page_historico.cpp
        src/ui/page_presets.cpp
        src/ui/page_principal.cpp
        src/ui/page_visual.cpp
        src/ui/pattern_editor.cpp
        src/ui/profiles.cpp
        src/ui/auto_vision.cpp
        src/platform/win32/capture.cpp
        src/ui/file_dialog.cpp
        src/ui/spray_plot.cpp
        src/ui/swap_target.cpp
        src/ui/textures.cpp
        src/ui/theme.cpp
        src/ui/widgets.cpp
        resources/app.rc
    )
    target_include_directories(SprayTrainer PRIVATE resources)
    target_compile_definitions(SprayTrainer PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(SprayTrainer PRIVATE st_core st_config imgui d3d11 d3dcompiler dxgi dwmapi windowscodecs ole32 shell32 comdlg32)
    st_warnings(SprayTrainer)
    if(MINGW)
        target_link_options(SprayTrainer PRIVATE -municode -static)
    endif()
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT SprayTrainer)
endif()
