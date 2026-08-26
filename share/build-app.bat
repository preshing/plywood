@rem ──────────────────────────────────────────────────────┐
@rem      ____                                             │
@rem     ╱   ╱╲     Plywood C++ Runtime Library            │
@rem    ╱___╱╭╮╲    https://plywood.dev/                   │
@rem     └──┴┴┴┘                                           │
@rem ──────────────────────────────────────────────────────┘

@echo off
setlocal

rem Set PLYWOOD_ROOT to the normalized path to the script's parent directory.
for %%I in ("%~dp0..") do set "PLYWOOD_ROOT=%%~fI"

rem Get the app name.
if "%~1"=="" goto usage
set "APP_NAME=%~1"

rem Reject names that don't identify an app project.
if not exist "%PLYWOOD_ROOT%\apps\%APP_NAME%\CMakeLists.txt" (
    >&2 echo Error: '%APP_NAME%' is not an app in %PLYWOOD_ROOT%\apps.
    exit /b 1
)

rem Require -r or --run before passing arguments to the app.
set "RUN_APP=0"
if "%~2"=="-r" set "RUN_APP=1"
if "%~2"=="--run" set "RUN_APP=1"
if not "%~2"=="" if "%RUN_APP%"=="0" (
    >&2 echo Error: Expected -r or --run after the app name, but got '%~2'.
    exit /b 1
)

rem Generate the build system for the app in Debug mode.
rem Skip this step if the app will run and CMakeCache.txt already exists.
set "BUILD_DIR=%PLYWOOD_ROOT%\apps\%APP_NAME%\build-win"
if "%RUN_APP%"=="0" goto generateBuildSystem
if exist "%BUILD_DIR%\CMakeCache.txt" goto buildApp
:generateBuildSystem
cmake -S "%PLYWOOD_ROOT%\apps\%APP_NAME%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Debug
if errorlevel 1 exit /b %errorlevel%

rem Build the app in Debug mode.
rem This also regenerates the build system if out of date.
:buildApp
cmake --build "%BUILD_DIR%" --config Debug
if errorlevel 1 exit /b %errorlevel%

rem Optionally run the app with all arguments that follow -r or --run.
if "%RUN_APP%"=="0" exit /b 0
set "APP_ARGS=%* "
set "APP_ARGS=%APP_ARGS:* =%"
set "APP_ARGS=%APP_ARGS:* =%"
"%PLYWOOD_ROOT%\bin\%APP_NAME%.exe" %APP_ARGS% & call :ctrlCHandler
:ctrlCHandler
exit /b %errorlevel%

rem Print usage.
:usage
>&2 echo Usage: %~nx0 ^<app^> [-r^|--run [app arguments...]]
exit /b 1
