@echo off
rem P10 GameplayLint - run the engine's stock Data Validation commandlet on the project (configuration B0 in 01).
rem Usage: validate.bat [extra args...]   e.g. validate.bat -AssetType=DataTable
rem Stock arguments (UE 5.8.4, DataValidationCommandlet.cpp): -AssetType=<class>, -IncludeOnlyOnDiskAssets, -includeengine.
setlocal
set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "PROJECT=%~dp0..\GameplayLint\GameplayLint.uproject"
set "LOG=%~dp0..\GameplayLint\Saved\Logs\DataValidation.log"

"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJECT%" -run=DataValidation -unattended -nullrhi -nosplash -traceautostart=0 -stdout -FullStdOutLogOutput -abslog="%LOG%" %*
exit /b %ERRORLEVEL%
