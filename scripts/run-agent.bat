@rem ========================================================
@rem        ____
@rem       ╱   ╱╲    Plywood C++ Runtime Library
@rem      ╱___╱╭╮╲   https://plywood.dev/
@rem       └──┴┴┴┘
@rem ========================================================

@echo off
setlocal

rem Set PLYWOOD_ROOT to the normalized path to the parent directory.
for %%I in ("%~dp0..") do set "PLYWOOD_ROOT=%%~fI"

rem Print usage information when no command-line options are provided.
if not "%~1"=="" goto haveArgs
>&2 echo Convenience script to build and run an agent in one command.
>&2 echo.
>&2 echo Usage: %~nx0 [-b] ^<agent options^>
>&2 echo   -b: Build the agent (in Debug) before running
>&2 echo.
>&2 echo Options accepted by the agent:
"%PLYWOOD_ROOT%\bin\agent.exe" -usage
exit /b 1
:haveArgs

rem Optionally build the agent app before running it.
set "agentArgs=%*"
if not "%~1"=="-b" goto runAgent

call "%PLYWOOD_ROOT%\scripts\build-agent.bat"
if errorlevel 1 exit /b %errorlevel%

rem Remove -b while preserving all remaining arguments.
if "%~2"=="" goto noAgentArgs
set "agentArgs=%agentArgs:* =%"
goto runAgent
:noAgentArgs
set "agentArgs="

:runAgent
rem Run the agent app with all remaining arguments.
"%PLYWOOD_ROOT%\bin\agent.exe" %agentArgs%
exit /b %errorlevel%
