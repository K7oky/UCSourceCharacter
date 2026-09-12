@echo off
rem ==============================================================================================
rem  Builds the UCSourceCharacter editor target, including the SourceMovement module.
rem
rem  Usage:   Build_Editor.bat            -- incremental build
rem           Build_Editor.bat rebuild    -- full rebuild of the project modules
rem
rem  Override the engine location if it is not the default:
rem           set UE_ROOT=D:\UE_5.6
rem           Build_Editor.bat
rem ==============================================================================================

setlocal EnableDelayedExpansion

set "PROJECT_DIR=%~dp0"
if "%PROJECT_DIR:~-1%"=="\" set "PROJECT_DIR=%PROJECT_DIR:~0,-1%"
set "PROJECT_FILE=%PROJECT_DIR%\UCSourceCharacter.uproject"

rem ---- locate the engine -----------------------------------------------------------------------

if not "%UE_ROOT%"=="" goto :have_root

set "UE_ROOT=C:\Program Files\Epic Games\UE_5.6"
if exist "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" goto :have_root

rem Fall back to whatever the launcher registered.
for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine" /v INSTALLDIR 2^>nul ^| find "INSTALLDIR"') do set "EPIC_DIR=%%B"
if not "%EPIC_DIR%"=="" (
    if exist "%EPIC_DIR%UE_5.6\Engine\Build\BatchFiles\Build.bat" set "UE_ROOT=%EPIC_DIR%UE_5.6"
)

:have_root

if not exist "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" (
    echo.
    echo [ERROR] Unreal Engine 5.6 not found.
    echo         Looked for: "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat"
    echo.
    echo         Set UE_ROOT to your engine folder and run again, e.g.
    echo             set UE_ROOT=D:\Epic\UE_5.6
    echo             Build_Editor.bat
    echo.
    pause
    exit /b 1
)

if not exist "%PROJECT_FILE%" (
    echo [ERROR] Project file not found: "%PROJECT_FILE%"
    pause
    exit /b 1
)

rem ---- build -----------------------------------------------------------------------------------

set "EXTRA_ARGS="
if /i "%~1"=="rebuild" set "EXTRA_ARGS=-Rebuild"

echo ==============================================================================
echo  Engine : %UE_ROOT%
echo  Project: %PROJECT_FILE%
echo  Target : UCSourceCharacterEditor Win64 Development %EXTRA_ARGS%
echo ==============================================================================
echo.

call "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" UCSourceCharacterEditor Win64 Development -Project="%PROJECT_FILE%" -WaitMutex %EXTRA_ARGS%

set "BUILD_RESULT=%ERRORLEVEL%"

echo.
if "%BUILD_RESULT%"=="0" (
    echo ==============================================================================
    echo  BUILD SUCCEEDED
    echo.
    echo  Next: run the movement tests with   Run_SourceMovement_Tests.bat
    echo ==============================================================================
) else (
    echo ==============================================================================
    echo  BUILD FAILED  ^(exit code %BUILD_RESULT%^)
    echo.
    echo  Scroll up for the first "error:" line -- that is the real cause.
    echo ==============================================================================
)

echo.
pause
exit /b %BUILD_RESULT%
