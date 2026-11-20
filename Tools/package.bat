@echo off
rem P10 GameplayLint - package a standalone Win64 build (BuildCookRun).
rem Usage: package.bat [Configuration]   Configuration: Development (default) | Shipping
rem The lint tooling is editor-only; the packaged game exists to prove that none of it leaks into a runtime build.
rem Note: the launcher (installed) engine cannot build the Test configuration (finding from UE5-UpdateBudget, Phase 4).
rem Output: ..\Build\Windows\GameplayLint.exe   (Build\ is git-ignored)
setlocal
set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "PROJECT=%~dp0..\GameplayLint\GameplayLint.uproject"
set "ARCHIVE=%~dp0..\Build"
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Development"

call "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%PROJECT%" -noP4 -utf8output -unattended -platform=Win64 -clientconfig=%CONFIG% -build -cook -stage -pak -archive -archivedirectory="%ARCHIVE%"
exit /b %ERRORLEVEL%
