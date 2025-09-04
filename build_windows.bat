@echo off
REM XP2GDL90 Plugin Build Script (Windows)
REM Usage: build_windows.bat [clean]

setlocal enabledelayedexpansion

set PROJECT_DIR=%~dp0
set BUILD_DIR=%PROJECT_DIR%build

echo === XP2GDL90 Plugin Build Script (Windows) ===
echo Project Directory: %PROJECT_DIR%
echo Build Directory: %BUILD_DIR%

REM Check if cleanup is needed
if "%1"=="clean" (
    echo Cleaning build directory...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
)

REM Create build directory
if not exist "%BUILD_DIR%" (
    echo Creating build directory...
    mkdir "%BUILD_DIR%"
)

cd /d "%BUILD_DIR%"

REM Check if CMake is installed
cmake --version >nul 2>&1
if errorlevel 1 (
    echo Error: CMake not found. Please install CMake first.
    echo Download from: https://cmake.org/download/
    exit /b 1
)

REM Check if X-Plane SDK exists
if not exist "%PROJECT_DIR%SDK" (
    echo Error: X-Plane SDK not found. Please ensure SDK directory exists.
    echo The SDK should be located at: %PROJECT_DIR%SDK
    exit /b 1
)

echo Running CMake configuration...

REM Try Visual Studio generator first, fall back to NMake if not available
cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release "%PROJECT_DIR:~0,-1%"
if errorlevel 1 (
    echo Visual Studio generator failed, trying Visual Studio 16...
    cmake -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=Release "%PROJECT_DIR:~0,-1%"
    if errorlevel 1 (
        echo Visual Studio generators failed, trying NMake...
        cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release "%PROJECT_DIR:~0,-1%"
        if errorlevel 1 (
            echo CMake configuration failed with all generators.
            exit /b 1
        )
    )
)

echo Starting compilation...
cmake --build . --config Release
if errorlevel 1 (
    echo Compilation failed.
    exit /b 1
)

echo.
echo === Build Complete ===

REM Check output file
set XPL_FILE=%BUILD_DIR%\win.xpl

if exist "%XPL_FILE%" (
    echo [SUCCESS] Generated: %XPL_FILE%
    
    echo.
    echo File information:
    dir "%XPL_FILE%"
    
    REM Get file size and check if it's reasonable
    for %%A in ("%XPL_FILE%") do (
        set FILE_SIZE=%%~zA
        set /a FILE_SIZE_KB=!FILE_SIZE!/1024
        echo File size: !FILE_SIZE_KB! KB
        
        if !FILE_SIZE! LSS 10240 (
            echo.
            echo [WARNING] Plugin file is unusually small ^(!FILE_SIZE! bytes^)
            echo This might indicate a build issue.
        )
    )
    
    echo.
    echo [INSTALLATION] Copy win.xpl to:
    echo   X-Plane\Resources\plugins\xp2gdl90\win.xpl
    echo   No additional files needed - plugin is self-contained
) else (
    echo [ERROR] Output file not found: %XPL_FILE%
    exit /b 1
)

endlocal
