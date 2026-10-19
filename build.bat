@echo off
setlocal

set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    echo vcvars64.bat not found at "%VCVARS%"
    exit /b 1
)

call "%VCVARS%" >nul 2>&1
pushd "%~dp0"
if not exist dist mkdir dist
cl /nologo /EHsc /W4 /O2 /DUNICODE /D_UNICODE main.cpp /Fo:dist\main.obj /Fe:dist\wt-tmux-tray.exe /link /SUBSYSTEM:WINDOWS user32.lib shell32.lib
set "RC=%ERRORLEVEL%"
del /q dist\main.obj 2>nul
popd

if %RC% neq 0 (
    echo Build failed.
    exit /b %RC%
)
echo Build succeeded: dist\wt-tmux-tray.exe
endlocal
