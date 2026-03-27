# MCDevConsole Release Build Script (PowerShell)
# Usage: .\scripts\build-release.ps1

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "MCDevConsole Release Build Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 检查 Node.js
Write-Host "[Check] Node.js..." -ForegroundColor Yellow
$nodeCheck = Get-Command node -ErrorAction SilentlyContinue
if (-not $nodeCheck) {
    Write-Host "[ERROR] Node.js not found. Please install Node.js first." -ForegroundColor Red
    exit 1
}
Write-Host "[OK] Node.js found: $(node --version)" -ForegroundColor Green

# 检查 CMake
Write-Host "[Check] CMake..." -ForegroundColor Yellow
$cmakeCheck = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmakeCheck) {
    Write-Host "[ERROR] CMake not found. Please install CMake first." -ForegroundColor Red
    exit 1
}
Write-Host "[OK] CMake found: $(cmake --version | Select-Object -First 1)" -ForegroundColor Green
Write-Host ""

# 1. 构建前端
Write-Host "[1/3] Building frontend..." -ForegroundColor Cyan
$webDir = Join-Path $PSScriptRoot "..\src\client\web"
Push-Location $webDir

if (-not (Test-Path "node_modules")) {
    Write-Host "Installing npm dependencies..." -ForegroundColor Yellow
    npm install
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] npm install failed" -ForegroundColor Red
        exit 1
    }
}

npm run build
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Frontend build failed" -ForegroundColor Red
    exit 1
}
Write-Host "[OK] Frontend build completed" -ForegroundColor Green
Pop-Location
Write-Host ""

# 2. 配置 CMake
Write-Host "[2/3] Configuring CMake..." -ForegroundColor Cyan
$rootDir = Join-Path $PSScriptRoot ".."
Push-Location $rootDir

cmake --preset default
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] CMake configuration failed" -ForegroundColor Red
    exit 1
}
Write-Host "[OK] CMake configuration completed" -ForegroundColor Green
Write-Host ""

# 3. 编译 Release
Write-Host "[3/3] Building Release..." -ForegroundColor Cyan
cmake --build build --config Release
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Build failed" -ForegroundColor Red
    exit 1
}
Write-Host "[OK] Build completed" -ForegroundColor Green
Pop-Location
Write-Host ""

Write-Host "========================================" -ForegroundColor Green
Write-Host "Build completed successfully!" -ForegroundColor Green
Write-Host "Output: build\Release\MCDevConsole.exe" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
