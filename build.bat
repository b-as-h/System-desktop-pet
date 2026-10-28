@echo off
chcp 65001 >nul
setlocal

rem ============================================================
rem  One-click build script
rem  Uses Qt's bundled CMake / Ninja / MinGW, no system PATH needed
rem ============================================================

set "QT=C:\Qt\6.11.2\mingw_64"
set "MINGW=C:\Qt\Tools\mingw1310_64"
set "CMAKE=C:\Qt\Tools\CMake_64\bin\cmake.exe"
set "NINJA=C:\Qt\Tools\Ninja\ninja.exe"

if not exist "%QT%"    ( echo [ERROR] Qt not found: %QT% & pause & exit /b 1 )
if not exist "%MINGW%" ( echo [ERROR] MinGW not found: %MINGW% & pause & exit /b 1 )
if not exist "%CMAKE%" ( echo [ERROR] CMake not found: %CMAKE% & pause & exit /b 1 )

set "PATH=%MINGW%\bin;%QT%\bin;%PATH%"

echo [1/2] Configuring...
"%CMAKE%" -S "%~dp0." -B "%~dp0build" -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_PREFIX_PATH="%QT%" ^
  -DCMAKE_CXX_COMPILER="%MINGW%\bin\g++.exe" ^
  -DCMAKE_MAKE_PROGRAM="%NINJA%"
if errorlevel 1 ( echo [ERROR] Configure failed & pause & exit /b 1 )

echo.
echo [2/2] Building...
"%CMAKE%" --build "%~dp0build"
if errorlevel 1 ( echo [ERROR] Build failed & pause & exit /b 1 )

echo.
echo ============================================================
echo  Build OK: %~dp0build\bin\SystemInfoPet.exe
echo  Double-click run.bat to start
echo ============================================================
pause
