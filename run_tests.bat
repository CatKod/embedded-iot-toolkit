@echo off
REM ============================================================
REM  embedded-iot-toolkit - build and run the host unit tests
REM  Requires: any C99 compiler (gcc / clang / cl). Uses gcc by
REM  default; override with:  set CC=clang  &&  run_tests.bat
REM ============================================================
setlocal

set CC=gcc
if not "%CC%"=="" set CC=%CC%

if not exist build mkdir build

echo Building test_core...
%CC% -std=c99 -Wall -Wextra -Wpedantic -Werror ^
  -Iinclude -Itest ^
  src\ekt_soft_timer.c ^
  src\ekt_frame.c ^
  src\ekt_can_codec.c ^
  src\ekt_sensor.c ^
  port\posix\ekt_port_posix.c ^
  test\test_core.c ^
  -o build\test_core.exe
if errorlevel 1 (
  echo.
  echo BUILD FAILED
  exit /b 1
)

echo Running...
echo.
build\test_core.exe
set RESULT=%errorlevel%

echo.
if "%RESULT%"=="0" (
  echo ALL TESTS PASSED
) else (
  echo TESTS FAILED - exit code %RESULT%
)
exit /b %RESULT%
