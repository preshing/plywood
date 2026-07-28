@rem ========================================================
@rem        ____
@rem       ╱   ╱╲    Plywood C++ Runtime Library
@rem      ╱___╱╭╮╲   https://plywood.dev/
@rem       └──┴┴┴┘
@rem ========================================================

@echo off
setlocal

rem Set PLYWOOD_ROOT to the normalized path to the script's parent directory.
for %%I in ("%~dp0..") do set "PLYWOOD_ROOT=%%~fI"

rem Generate the build system for the make-banner-comment app (in Debug).
cmake -S "%PLYWOOD_ROOT%\apps\make-banner-comment" -B "%PLYWOOD_ROOT%\apps\make-banner-comment\build-win" -DCMAKE_BUILD_TYPE=Debug
if errorlevel 1 exit /b %errorlevel%

rem Build the make-banner-comment app (in Debug).
cmake --build "%PLYWOOD_ROOT%\apps\make-banner-comment\build-win" --config Debug
if errorlevel 1 exit /b %errorlevel%

rem Run the make-banner-comment app.
"%PLYWOOD_ROOT%\bin\make-banner-comment.exe" %*
exit /b %errorlevel%
