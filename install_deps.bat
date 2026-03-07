@echo off
echo ═══════════════════════════════════════════════════
echo  WebView Godot Plugin — Dependency Installer
echo ═══════════════════════════════════════════════════

REM Check for NuGet
where nuget >nul 2>&1
if %errorlevel% neq 0 (
    echo [INFO] NuGet not found. Downloading nuget.exe...
    powershell -Command "Invoke-WebRequest -Uri https://dist.nuget.org/win-x86-commandline/latest/nuget.exe -OutFile nuget.exe"
    set NUGET=nuget.exe
) else (
    set NUGET=nuget
)

echo [INFO] Installing WebView2 SDK...
%NUGET% install Microsoft.Web.WebView2 -OutputDirectory packages -NonInteractive

echo [INFO] Installing WIL (Windows Implementation Library)...
%NUGET% install Microsoft.Windows.ImplementationLibrary -OutputDirectory packages -NonInteractive

echo.
echo [DONE] Dependencies installed into: packages\
echo        Now run build.bat to compile the plugin.
pause
