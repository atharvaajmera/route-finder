@echo off
echo ========================================
echo Building Test Centre Allotter with libcurl
echo ========================================
echo.

REM Set vcpkg path (adjust if you installed vcpkg elsewhere)
set VCPKG_ROOT=C:\vcpkg

REM Check if vcpkg is installed
if not exist "%VCPKG_ROOT%\installed\x64-windows\lib\libcurl.lib" (
    echo ERROR: libcurl not found!
    echo.
    echo Please install vcpkg and libcurl first:
    echo   1. cd C:\
    echo   2. git clone https://github.com/Microsoft/vcpkg.git
    echo   3. cd vcpkg
    echo   4. bootstrap-vcpkg.bat
    echo   5. vcpkg install curl:x64-windows
    echo.
    echo See INSTALL_LIBCURL.md for detailed instructions.
    echo.
    pause
    exit /b 1
)

echo Found libcurl at: %VCPKG_ROOT%\installed\x64-windows\lib\libcurl.lib
echo.
echo Compiling...

cl.exe /EHsc /std:c++17 /I.. /I%VCPKG_ROOT%\installed\x64-windows\include main.cpp /Fe:server.exe /link /LIBPATH:%VCPKG_ROOT%\installed\x64-windows\lib libcurl.lib ws2_32.lib wldap32.lib advapi32.lib crypt32.lib normaliz.lib

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Compilation FAILED!
    pause
    exit /b 1
)

echo.
echo ========================================
echo Compilation SUCCESS!
echo ========================================
echo.
echo Copying required DLLs...

REM Copy libcurl.dll to backend directory for easy execution
if exist "%VCPKG_ROOT%\installed\x64-windows\bin\libcurl.dll" (
    copy "%VCPKG_ROOT%\installed\x64-windows\bin\libcurl.dll" . >nul
    echo Copied libcurl.dll
)

echo.
echo Ready to run! Execute: server.exe
echo.
pause
