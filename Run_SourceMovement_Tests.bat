@echo off
rem ==============================================================================================
rem  Runs the SourceMovement automation tests headless (no editor window, no GPU needed).
rem
rem  Usage:   Run_SourceMovement_Tests.bat                  -- all SourceMovement tests
rem           Run_SourceMovement_Tests.bat SourceMovement.Jump      -- one group
rem           Run_SourceMovement_Tests.bat SourceMovement.AirAccel  -- one group
rem
rem  Build first with Build_Editor.bat.
rem
rem  Results:
rem    a pass/fail summary at the end, plus a JSON/HTML report in  Saved\Automation\Reports\
rem    The full engine log is printed too; the summary is read back from the JSON report because
rem    the per-test lines scroll out of the console buffer on a full run.
rem ==============================================================================================

setlocal EnableDelayedExpansion

set "PROJECT_DIR=%~dp0"
if "%PROJECT_DIR:~-1%"=="\" set "PROJECT_DIR=%PROJECT_DIR:~0,-1%"
set "PROJECT_FILE=%PROJECT_DIR%\UCSourceCharacter.uproject"

set "TEST_FILTER=%~1"
if "%TEST_FILTER%"=="" set "TEST_FILTER=SourceMovement"

rem ---- locate the engine -----------------------------------------------------------------------

if not "%UE_ROOT%"=="" goto :have_root

set "UE_ROOT=C:\Program Files\Epic Games\UE_5.6"
if exist "%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" goto :have_root

for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine" /v INSTALLDIR 2^>nul ^| find "INSTALLDIR"') do set "EPIC_DIR=%%B"
if not "%EPIC_DIR%"=="" (
    if exist "%EPIC_DIR%UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" set "UE_ROOT=%EPIC_DIR%UE_5.6"
)

:have_root

set "EDITOR_CMD=%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

if not exist "%EDITOR_CMD%" (
    echo.
    echo [ERROR] UnrealEditor-Cmd.exe not found.
    echo         Looked for: "%EDITOR_CMD%"
    echo         Set UE_ROOT to your engine folder and run again.
    echo.
    pause
    exit /b 1
)

set "REPORT_DIR=%PROJECT_DIR%\Saved\Automation\Reports"
if not exist "%REPORT_DIR%" mkdir "%REPORT_DIR%" >nul 2>&1

echo ==============================================================================
echo  Engine : %UE_ROOT%
echo  Project: %PROJECT_FILE%
echo  Filter : %TEST_FILTER%
echo  Report : %REPORT_DIR%
echo ==============================================================================
echo.

rem -NullRHI       : no graphics device required
rem -unattended    : never prompt
rem -nopause       : do not hold the window open on exit
rem -stdout -FullStdOutLogOutput : print the test log to this console
"%EDITOR_CMD%" "%PROJECT_FILE%" ^
    -ExecCmds="Automation RunTests %TEST_FILTER%; Quit" ^
    -TestExit="Automation Test Queue Empty" ^
    -ReportExportPath="%REPORT_DIR%" ^
    -unattended -nopause -nosplash -NullRHI ^
    -stdout -FullStdOutLogOutput -NoLogTimes

set "TEST_RESULT=%ERRORLEVEL%"

echo.

rem ---- summary ---------------------------------------------------------------------------------
rem The engine prints one "Test Completed. Result={...}" line per test, but the full log is
rem thousands of lines and those scroll out of the console buffer long before the run ends. Read the
rem counts back out of the JSON report instead, and print the error text of anything that failed.
powershell -NoProfile -ExecutionPolicy Bypass -File "%PROJECT_DIR%\Tools\SummarizeTestReport.ps1" -ReportPath "%REPORT_DIR%\index.json"

echo  Full report: %REPORT_DIR%\index.json
if not "%TEST_RESULT%"=="0" echo  Editor exit code: %TEST_RESULT%

echo.
pause
exit /b %TEST_RESULT%
