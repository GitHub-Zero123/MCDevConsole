@echo off
echo Testing CMake build flow...
echo.

cd /d "%~dp0.."

echo [1] Cleaning old build...
if exist "build" rmdir /s /q "build"
if exist "src\client\cpp\include\MCDevConsole\WebResource.hpp" del "src\client\cpp\include\MCDevConsole\WebResource.hpp"

echo [2] Configuring CMake...
cmake --preset default
if %errorlevel% neq 0 (
    echo [ERROR] CMake configure failed
    exit /b 1
)

echo [3] Building Release...
cmake --build build --config Release
if %errorlevel% neq 0 (
    echo [ERROR] Build failed
    exit /b 1
)

echo.
echo [SUCCESS] Build completed!
echo Check: build\Release\MCDevConsole.exe
