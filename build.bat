@echo off
echo ========================================
echo MusicPlayer - Quick Build Script
echo ========================================
echo.

REM Check if build directory exists
if not exist "build" (
    echo Creating build directory...
    mkdir build
)

cd build

REM Configure with CMake
echo.
echo Configuring CMake (NEW UI)...
cmake .. -G "MinGW Makefiles" -DUSE_NEW_UI=ON -DCMAKE_BUILD_TYPE=Release

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: CMake configuration failed!
    pause
    exit /b 1
)

REM Build
echo.
echo Building project...
cmake --build . --config Release -j8

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Build failed!
    pause
    exit /b 1
)

REM Run
echo.
echo ========================================
echo Build successful! Starting application...
echo ========================================
echo.

if exist "Release\SoundLink.exe" (
    start Release\SoundLink.exe
) else if exist "SoundLink.exe" (
    start SoundLink.exe
) else (
    echo ERROR: Executable not found!
    pause
    exit /b 1
)

cd ..
