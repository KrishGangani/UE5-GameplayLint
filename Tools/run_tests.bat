@echo off
rem P10 GameplayLint - run the automation tests headless in the editor and exit with their result.
rem Usage: run_tests.bat [Filter]   (default filter: GameplayLint, i.e. every test of this project)
rem Exit code 0 = all passed; non-zero = a failure. Adapted from UE5-UpdateBudget's run_tests.bat.
rem Needs the Development editor target: build.bat GameplayLintEditor Development. Run it through the UE job queue.
setlocal
set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "PROJECT=%~dp0..\GameplayLint\GameplayLint.uproject"
set "FILTER=%~1"
if "%FILTER%"=="" set "FILTER=GameplayLint"
set "REPORT=%~dp0..\GameplayLint\Saved\Automation\Reports"
set "LOG=%~dp0..\GameplayLint\Saved\Logs\Tests.log"

"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJECT%" -ExecCmds="Automation RunTests %FILTER%;Quit" -unattended -nullrhi -nosound -nosplash -nopause -traceautostart=0 -stdout -FullStdOutLogOutput -log -abslog="%LOG%" -ReportExportPath="%REPORT%"
exit /b %ERRORLEVEL%
