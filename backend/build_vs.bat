@echo off
echo ========================================
echo Test Centre Allotter - MSVC Build
echo ========================================
echo.

REM Check if we're in a Visual Studio command prompt
where cl >nul 2>nul
if %errorlevel% neq 0 (
    echo ERROR: Visual Studio C++ compiler not found!
    echo.
    echo Please run this from:
    echo   "x64 Native Tools Command Prompt for VS 2022"
    echo.
    echo Or open Visual Studio Developer Command Prompt
    echo.
    pause
    exit /b 1
)

echo Compiler found! Building...
echo.

REM Compile the server
cl /EHsc /std:c++17 /Fe:server.exe main.cpp /I.. ws2_32.lib

if %errorlevel% equ 0 (
    echo.
    echo ========================================
    echo BUILD SUCCESSFUL!
    echo ========================================
    echo.
    echo Server executable: server.exe
    echo.
    echo To run the server:
    echo   server.exe
    echo.
    echo Then open frontend\index.html in your browser
    echo.
) else (
    echo.
    echo ========================================
    echo BUILD FAILED!
    echo ========================================
    echo.
    echo Check the errors above
    echo.
)

pause
