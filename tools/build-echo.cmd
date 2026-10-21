@echo off
setlocal
set "VSLANG=1033"
cd /d "%~dp0.."
if not defined ECHO_QT (
  echo Set ECHO_QT to an installed Qt 6 MSVC x64 prefix.
  exit /b 1
)
if not defined VSCMD_VER (
  if not defined ECHO_VCVARS (
    for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "ECHO_VCVARS=%%I\VC\Auxiliary\Build\vcvars64.bat"
  )
  if not defined ECHO_VCVARS (
    echo Visual Studio C++ Build Tools were not found. Use an x64 developer prompt.
    exit /b 1
  )
)
if not defined VSCMD_VER call "%ECHO_VCVARS%"
if errorlevel 1 exit /b %errorlevel%
set "PATH=%ECHO_QT%\bin;%PATH%"
if not defined ECHO_CMAKE set "ECHO_CMAKE=cmake"
if defined ECHO_NINJA (
  "%ECHO_CMAKE%" -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_PREFIX_PATH="%ECHO_QT%" -DCMAKE_MAKE_PROGRAM="%ECHO_NINJA%"
) else (
  "%ECHO_CMAKE%" -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_PREFIX_PATH="%ECHO_QT%"
)
if errorlevel 1 exit /b %errorlevel%
"%ECHO_CMAKE%" --build build --parallel 4
exit /b %errorlevel%
