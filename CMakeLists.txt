cmake_minimum_required(VERSION 3.20)
project(webview_godot VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Paths — edit these if your folders differ ──────────────────────────────────
# godot-cpp: clone from https://github.com/godotengine/godot-cpp (4.x branch)
set(GODOT_CPP_PATH  "${CMAKE_SOURCE_DIR}/../godot-cpp"  CACHE PATH "Path to godot-cpp")

# WebView2 NuGet package — after running install_deps.bat this will be auto-set
set(WEBVIEW2_PATH   "${CMAKE_SOURCE_DIR}/packages/Microsoft.Web.WebView2"  CACHE PATH "Path to WebView2 SDK")

# WIL (Windows Implementation Library) — also from NuGet
set(WIL_PATH        "${CMAKE_SOURCE_DIR}/packages/Microsoft.Windows.ImplementationLibrary"  CACHE PATH "Path to WIL")
# ───────────────────────────────────────────────────────────────────────────────

# godot-cpp
add_subdirectory(${GODOT_CPP_PATH} godot-cpp)

# WebView2 include/lib dirs (NuGet layout)
set(WEBVIEW2_INCLUDE "${WEBVIEW2_PATH}/build/native/include")
set(WEBVIEW2_LIB     "${WEBVIEW2_PATH}/build/native/x64/WebView2LoaderStatic.lib")
set(WIL_INCLUDE      "${WIL_PATH}/include")

# ── Plugin target ──────────────────────────────────────────────────────────────
add_library(webview_godot SHARED
    src/register_types.cpp
    src/webview_window.cpp
)

target_include_directories(webview_godot PRIVATE
    src
    ${WEBVIEW2_INCLUDE}
    ${WIL_INCLUDE}
)

target_link_libraries(webview_godot PRIVATE
    godot-cpp
    ${WEBVIEW2_LIB}
)

# Windows-specific
target_compile_definitions(webview_godot PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
)

# Output directly into the addon bin folder
set_target_properties(webview_godot PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/godot_addon/addons/livekit_webview/bin"
    PREFIX ""
    SUFFIX ".dll"
)
