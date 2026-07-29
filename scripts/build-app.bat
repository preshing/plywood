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

rem Get the app name.
if "%~1"=="" goto usage
set "APP_NAME=%~1"

rem Accept optional -run argument after the app name.
set "RUN_APP=0"
if "%~2"=="" goto validateApp
if not "%~2"=="-run" goto usage
set "RUN_APP=1"
set "APP_ARGS=%*"
set "APP_ARGS=%APP_ARGS:* =%"
set "APP_ARGS=%APP_ARGS:* =%"

:validateApp
rem Reject names that don't identify an app project.
if not exist "%PLYWOOD_ROOT%\apps\%APP_NAME%\CMakeLists.txt" (
    >&2 echo Error: '%APP_NAME%' is not an app in %PLYWOOD_ROOT%\apps.
    exit /b 1
)

rem Generate the build system for the app (in Debug).
cmake -S "%PLYWOOD_ROOT%\apps\%APP_NAME%" -B "%PLYWOOD_ROOT%\apps\%APP_NAME%\build-win" -DCMAKE_BUILD_TYPE=Debug
if errorlevel 1 exit /b %errorlevel%

rem Build the app (in Debug).
cmake --build "%PLYWOOD_ROOT%\apps\%APP_NAME%\build-win" --config Debug
if errorlevel 1 exit /b %errorlevel%

rem Optionally run the app with all arguments following -run.
if not "%RUN_APP%"=="1" exit /b 0
if "%~3"=="" goto runAppWithoutArgs
"%PLYWOOD_ROOT%\bin\%APP_NAME%.exe" %APP_ARGS%
exit /b %errorlevel%

:runAppWithoutArgs
"%PLYWOOD_ROOT%\bin\%APP_NAME%.exe"
exit /b %errorlevel%

:usage
>&2 echo Usage: %~nx0 ^<app^> [-run [app arguments...]]
exit /b 1
