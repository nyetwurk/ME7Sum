@echo off
setlocal EnableExtensions

rem Expand before parenthesized blocks - (x86) breaks batch parsing inside ( )
set "PF86=%ProgramFiles(x86)%"
set "PF=%ProgramFiles%"

if defined VSINSTALL goto setup_env

if exist "%PF86%\Microsoft Visual Studio\Installer\vswhere.exe" (
  set "VSWHERE=%PF86%\Microsoft Visual Studio\Installer\vswhere.exe"
) else if exist "%PF%\Microsoft Visual Studio\Installer\vswhere.exe" (
  set "VSWHERE=%PF%\Microsoft Visual Studio\Installer\vswhere.exe"
) else (
  echo ERROR: vswhere not found. Install Visual Studio with the C++ workload.
  exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"

if not defined VSINSTALL (
  echo ERROR: No MSVC installation found. Install the "Desktop development with C++" workload.
  exit /b 1
)

:setup_env
echo Using Visual Studio at "%VSINSTALL%"
call "%VSINSTALL%\VC\Auxiliary\Build\vcvarsall.bat" x86
if errorlevel 1 exit /b 1

for /f "usebackq tokens=*" %%i in (`git describe --tags "--abbrev=4" --dirty --always`) do set "GIT_VERSION=%%i"

nmake %*
