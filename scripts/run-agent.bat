@rem ========================================================
@rem        ____
@rem       ╱   ╱╲    Plywood C++ Runtime Library
@rem      ╱___╱╭╮╲   https://plywood.dev/
@rem       └──┴┴┴┘
@rem ========================================================

@echo off
setlocal

rem Print usage information when no command-line arguments are provided.
if not "%~1"=="" goto haveArgs
>&2 echo Usage: %~nx0 [-b] ^<agent arguments^>
>&2 echo -b : Build the agent app in Debug before running.
"%~dp0..\apps\agent\build-win\Debug\agent.exe" %agentArgs%
exit /b 1
:haveArgs

rem Optionally build the agent app before running it.
set "agentArgs=%*"
if not "%~1"=="-b" goto runAgent

rem %~dp0 expands to the drive and directory containing the script.
call "%~dp0build-agent.bat"
if errorlevel 1 exit /b %errorlevel%

rem Remove -b while preserving all remaining arguments.
if "%~2"=="" goto noAgentArgs
set "agentArgs=%agentArgs:* =%"
goto runAgent
:noAgentArgs
set "agentArgs="

:runAgent
rem Run the agent app with all remaining arguments.
"%~dp0..\apps\agent\build-win\Debug\agent.exe" %agentArgs%
exit /b %errorlevel%
