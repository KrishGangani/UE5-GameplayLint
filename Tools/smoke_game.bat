@echo off
rem P10 GameplayLint - smoke test: start the game, load the default map, quit.
rem Usage: smoke_game.bat [packaged|uncooked] [extra args...]
rem   packaged (default): the packaged Development build from package.bat, no GPU, no sound.
rem   uncooked:           the editor binary in -game mode on the uncooked project.
setlocal
set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "PROJECT=%~dp0..\GameplayLint\GameplayLint.uproject"
set "MODE=%~1"
if "%MODE%"=="" set "MODE=packaged"
set "COMMON=-nullrhi -nosound -unattended -nosplash -traceautostart=0 -ExecCmds=quit -log"

if /i "%MODE%"=="uncooked" (
	"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJECT%" -game %COMMON% -abslog="%~dp0..\GameplayLint\Saved\Logs\SmokeUncooked.log" %2 %3 %4 %5 %6
) else (
	"%~dp0..\Build\Windows\GameplayLint.exe" %COMMON% -abslog="%~dp0..\Build\SmokePackaged.log" %2 %3 %4 %5 %6
)
exit /b %ERRORLEVEL%
