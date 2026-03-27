@echo off
setlocal

echo ========================================
echo MCDevConsole Release Build Script
echo ========================================
echo.

:: 检查 Node.js
where node >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Node.js not found. Please install Node.js first.
    exit /b 1
)

:: 检查 CMake
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] CMake not found. Please install CMake first.
    exit /b 1
)

:: 1. 构建前端
echo [1/3] Building frontend...
cd /d "%~dp0..\src\client\web"
if not exist "node_modules" (
    echo Installing dependencies...
    call npm install
    if %errorlevel% neq 0 (
        echo [ERROR] npm install failed
        exit /b 1
    )
)

call npm run build
if %errorlevel% neq 0 (
    echo [ERROR] Frontend build failed
    exit /b 1
)
echo Frontend build completed.
echo.

:: 2. 配置 CMake
echo [2/3] Configuring CMake...
cd /d "%~dp0.."
cmake --preset default
if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed
    exit /b 1
)
echo.

:: 3. 编译 Release
echo [3/3] Building Release...
cmake --build build --config Release
if %errorlevel% neq 0 (
    echo [ERROR] Build failed
    exit /b 1
)
echo.

echo ========================================
echo Build completed successfully!
echo Output: build\Release\MCDevConsole.exe
echo ========================================

endlocal
