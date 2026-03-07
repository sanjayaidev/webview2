@echo off
echo ═══════════════════════════════════════════════════
echo  WebView Godot Plugin — Build
echo ═══════════════════════════════════════════════════

REM Auto-detect WebView2 and WIL package paths (take first match)
for /d %%D in (packages\Microsoft.Web.WebView2.*) do set WEBVIEW2_PATH=%%~fD
for /d %%D in (packages\Microsoft.Windows.ImplementationLibrary.*) do set WIL_PATH=%%~fD

if not defined WEBVIEW2_PATH (
    echo [ERROR] WebView2 package not found. Run install_deps.bat first.
    pause & exit /b 1
)
if not defined WIL_PATH (
    echo [ERROR] WIL package not found. Run install_deps.bat first.
    pause & exit /b 1
)

echo [INFO] WebView2: %WEBVIEW2_PATH%
echo [INFO] WIL:      %WIL_PATH%

REM Create build dir
if not exist build mkdir build
cd build

REM Configure
cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DWEBVIEW2_PATH="%WEBVIEW2_PATH%" ^
    -DWIL_PATH="%WIL_PATH%"

if %errorlevel% neq 0 (
    echo [ERROR] CMake configure failed.
    cd ..
    pause & exit /b 1
)

REM Build Release
cmake --build . --config Release

if %errorlevel% neq 0 (
    echo [ERROR] Build failed.
    cd ..
    pause & exit /b 1
)

cd ..
echo.
echo [DONE] DLL output: godot_addon\addons\livekit_webview\bin\webview_godot.dll
pause
