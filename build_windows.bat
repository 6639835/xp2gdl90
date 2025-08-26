@echo off
REM OpenAL Shared Example Plugin Build Script (Windows)
REM Usage: build_windows.bat [clean]

setlocal enabledelayedexpansion

set PROJECT_DIR=%~dp0
set BUILD_DIR=%PROJECT_DIR%build

echo === OpenAL Shared Example Plugin Build Script (Windows) ===
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
cmake -DCMAKE_BUILD_TYPE=Release "%PROJECT_DIR:~0,-1%"
if errorlevel 1 (
    echo CMake configuration failed.
    exit /b 1
)

echo Starting compilation...
cmake --build . --config Release
if errorlevel 1 (
    echo Compilation failed.
    exit /b 1
)

echo.
echo === Build Complete ===

REM Check output file (Visual Studio puts it in Release subdirectory)
set XPL_FILE_RELEASE=%BUILD_DIR%\Release\win.xpl
set XPL_FILE_ROOT=%BUILD_DIR%\win.xpl

if exist "%XPL_FILE_RELEASE%" (
    echo [SUCCESS] Generated: %XPL_FILE_RELEASE%
    
    REM Copy to root build directory for consistency
    copy "%XPL_FILE_RELEASE%" "%XPL_FILE_ROOT%" >nul
    echo [INFO] Copied to: %XPL_FILE_ROOT%
    
    echo.
    echo File information:
    dir "%XPL_FILE_ROOT%"
    
    REM Get file size and check if it's reasonable
    for %%A in ("%XPL_FILE_ROOT%") do (
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
    echo   X-Plane\Resources\plugins\OpenALSharedExample\win.xpl
    echo   Also copy sound.wav file to the same directory
) else (
    if exist "%XPL_FILE_ROOT%" (
        echo [SUCCESS] Generated: %XPL_FILE_ROOT%
        
        echo.
        echo File information:
        dir "%XPL_FILE_ROOT%"
        
        REM Get file size and check if it's reasonable
        for %%A in ("%XPL_FILE_ROOT%") do (
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
        echo   X-Plane\Resources\plugins\OpenALSharedExample\win.xpl
        echo   Also copy sound.wav file to the same directory
    ) else (
        echo [ERROR] Output file not found in either location:
        echo   - %XPL_FILE_RELEASE%
        echo   - %XPL_FILE_ROOT%
        exit /b 1
    )
)

endlocal
