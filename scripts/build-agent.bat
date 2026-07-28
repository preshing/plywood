@rem ========================================================
@rem        ____
@rem       ╱   ╱╲    Plywood C++ Runtime Library
@rem      ╱___╱╭╮╲   https://plywood.dev/
@rem       └──┴┴┴┘
@rem ========================================================

@echo off
setlocal

rem Generate the build system for the agent app.
rem %~dp0 expands to the drive and directory containing the script.
cmake -S "%~dp0..\apps\agent" -B "%~dp0..\apps\agent\build-win" -DCMAKE_BUILD_TYPE=Debug
if errorlevel 1 exit /b %errorlevel%

rem Perform a single Debug build of the agent app.
cmake --build "%~dp0..\apps\agent\build-win" --config Debug
exit /b %errorlevel%
