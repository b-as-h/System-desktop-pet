@echo off
chcp 65001 >nul
setlocal

rem Qt's bin must be on PATH, otherwise Qt6Widgets.dll is not found
set "PATH=C:\Qt\6.11.2\mingw_64\bin;%PATH%"

if not exist "%~dp0build\bin\SystemInfoPet.exe" (
    echo [ERROR] Not built yet. Run build.bat first.
    pause
    exit /b 1
)

start "" "%~dp0build\bin\SystemInfoPet.exe"
