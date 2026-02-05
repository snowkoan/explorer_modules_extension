@echo off
setlocal

set "CONFIG=Debug"
if /I "%~1"=="release" set "CONFIG=Release"

cmake -S . -B build
if errorlevel 1 exit /b %errorlevel%

cmake --build build --config %CONFIG%
exit /b %errorlevel%
