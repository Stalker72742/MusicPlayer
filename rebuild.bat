@echo off
echo ========================================
echo MusicPlayer - Clean Build
echo ========================================

REM Remove build directory
if exist "build" (
    echo Removing old build directory...
    rmdir /s /q build
)

REM Run build script
echo.
call build.bat
