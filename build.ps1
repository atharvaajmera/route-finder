# Build script for Windows (PowerShell)

Write-Host "==================================" -ForegroundColor Cyan
Write-Host "Test Centre Allotter - Build Script" -ForegroundColor Cyan
Write-Host "==================================" -ForegroundColor Cyan
Write-Host ""

# Check if g++ is available
Write-Host "Checking for g++..." -ForegroundColor Yellow
$gppPath = Get-Command g++ -ErrorAction SilentlyContinue

if (-not $gppPath) {
    Write-Host "ERROR: g++ not found!" -ForegroundColor Red
    Write-Host "Please install MinGW or MSYS2" -ForegroundColor Red
    exit 1
}

Write-Host "Found g++ at: $($gppPath.Source)" -ForegroundColor Green
Write-Host ""

# Navigate to backend directory
Set-Location -Path "backend"

Write-Host "Compiling backend server..." -ForegroundColor Yellow
Write-Host "Command: g++ -std=c++17 main.cpp -o server.exe -I.. -lcurl -lws2_32" -ForegroundColor Gray
Write-Host ""

# Compile
$compileResult = & g++ -std=c++17 main.cpp -o server.exe -I.. -lcurl -lws2_32 2>&1

if ($LASTEXITCODE -ne 0) {
    Write-Host "COMPILATION FAILED!" -ForegroundColor Red
    Write-Host $compileResult -ForegroundColor Red
    Set-Location -Path ".."
    exit 1
}

Write-Host "Compilation successful!" -ForegroundColor Green
Write-Host ""

# Check if executable was created
if (Test-Path "server.exe") {
    Write-Host "✓ server.exe created successfully" -ForegroundColor Green
    $fileSize = (Get-Item "server.exe").Length / 1KB
    Write-Host "  Size: $([math]::Round($fileSize, 2)) KB" -ForegroundColor Gray
}
else {
    Write-Host "ERROR: server.exe not found!" -ForegroundColor Red
    Set-Location -Path ".."
    exit 1
}

Write-Host ""
Write-Host "==================================" -ForegroundColor Cyan
Write-Host "Build Complete!" -ForegroundColor Green
Write-Host "==================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "To run the server:" -ForegroundColor Yellow
Write-Host "  cd backend" -ForegroundColor White
Write-Host "  .\server.exe" -ForegroundColor White
Write-Host ""
Write-Host "Then open frontend\index.html in your browser" -ForegroundColor Yellow
Write-Host ""

Set-Location -Path ".."
