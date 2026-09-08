cmake_minimum_required(VERSION 3.24)

project(KeyboardManager VERSION 1.0.1 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "KeyboardManager only supports Windows.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "KeyboardManager requires a 64-bit build.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9b
    GIT_SHALLOW TRUE
)
FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG 0.11.23
    GIT_SHALLOW TRUE
    # Only the single header is consumed; skip miniaudio's unrelated examples and helper libraries.
    SOURCE_SUBDIR cmake/KeyboardManager-no-build
)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(imgui miniaudio nlohmann_json)

add_library(imgui_backend STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx9.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
)
target_include_directories(imgui_backend PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_compile_definitions(imgui_backend PRIVATE
    IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
    UNICODE
    _UNICODE
)

add_executable(KeyboardManager WIN32
    src/App.cpp
    src/AudioEngine.cpp
    src/BindEngine.cpp
    src/ConfigStore.cpp
    src/KeyboardLayouts.cpp
    src/Paths.cpp
    src/RawInputWindow.cpp
    src/main.cpp
)

target_include_directories(KeyboardManager PRIVATE
    src
    ${miniaudio_SOURCE_DIR}
)
target_compile_definitions(KeyboardManager PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
    IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
)
target_compile_options(KeyboardManager PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive- /utf-8 /EHsc>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
)
target_link_libraries(KeyboardManager PRIVATE
    imgui_backend
    nlohmann_json::nlohmann_json
    d3d9
    dwmapi
    shell32
    ole32
    comdlg32
)

if(MSVC)
    target_link_options(KeyboardManager PRIVATE $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>)
    target_compile_options(KeyboardManager PRIVATE $<$<CONFIG:Release>:/O2 /GL>)
endif()

if(MINGW)
    target_link_options(KeyboardManager PRIVATE -municode -static -static-libgcc -static-libstdc++)
endif()

install(TARGETS KeyboardManager RUNTIME DESTINATION .)
install(DIRECTORY Sounds DESTINATION .)
install(FILES README.md DESTINATION .)

include(CTest)
if(BUILD_TESTING)
    add_executable(KeyboardManagerTests
        tests/ConfigStoreTests.cpp
        src/ConfigStore.cpp
        src/Paths.cpp
    )
    target_include_directories(KeyboardManagerTests PRIVATE src)
    target_compile_definitions(KeyboardManagerTests PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
    )
    target_compile_options(KeyboardManagerTests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive- /utf-8 /EHsc>
        $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    )
    target_link_libraries(KeyboardManagerTests PRIVATE
        nlohmann_json::nlohmann_json
        shell32
        ole32
    )
    add_test(NAME config_store COMMAND KeyboardManagerTests)
endif()
