@rem ──────────────────────────────────────────────────────┐
@rem      ____                                             │
@rem     ╱   ╱╲     Plywood C++ Runtime Library            │
@rem    ╱___╱╭╮╲    https://plywood.dev/                   │
@rem     └──┴┴┴┘                                           │
@rem ──────────────────────────────────────────────────────┘

@echo off
setlocal

rem Set VCPKG_ROOT to the vcpkg directory beside the Plywood repository.
for %%I in ("%~dp0..\..\vcpkg") do set "VCPKG_ROOT=%%~fI"

rem Clone vcpkg unless an existing checkout is already available.
if exist "%VCPKG_ROOT%\.git" goto bootstrapVcpkg
git clone https://github.com/microsoft/vcpkg.git "%VCPKG_ROOT%"
if errorlevel 1 exit /b %errorlevel%

rem Bootstrap vcpkg.
:bootstrapVcpkg
call "%VCPKG_ROOT%\bootstrap-vcpkg.bat"
if errorlevel 1 exit /b %errorlevel%

rem Download and build libcurl with OpenSSL for 64-bit Windows.
pushd "%VCPKG_ROOT%"
if errorlevel 1 exit /b %errorlevel%
vcpkg install curl[openssl]:x64-windows
set "RESULT=%errorlevel%"
popd
exit /b %RESULT%
