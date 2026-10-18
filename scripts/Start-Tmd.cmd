@echo off
setlocal
if not exist "%~dp0tree_md.exe" (
    echo Tmd is missing. Extract the complete Portable ZIP before starting.
    pause
    exit /b 1
)
start "Tmd" /D "%~dp0" "%~dp0tree_md.exe" %*
exit /b %errorlevel%
