@echo off
REM Test runner for PreVABS - runs all integration tests
REM Usage: run_tests.bat [test_name]
REM   Without arguments: runs all tests
REM   With test_name: runs only that test (e.g., box, t1_strip)

setlocal enabledelayedexpansion

set "TEST_DIR=%~dp0integration"
set "BUILDDIR=%TEST_DIR%\build"
set "PREVABS=..\build_msvc\Release\prevabs.exe"

if not exist "%PREVABS%" (
    echo Error: prevabs.exe not found at %PREVABS%
    echo Please build the project first.
    exit /b 1
)

if "%~1"=="" (
    echo Running all integration tests...
    for /d %%D in ("%TEST_DIR%\t"*) do (
        call :run_test "%%~nxD"
    )
    for /d %%D in ("%TEST_DIR%\box" "%TEST_DIR%\backlog\*") do (
        if exist "%%D\*.xml" (
            call :run_test "%%~nxD"
        )
    )
    echo.
    echo All tests complete.
) else (
    call :run_test "%~1"
)

exit /b 0

:run_test
set "TEST_NAME=%~1"
set "TEST_PATH=%TEST_DIR%\%TEST_NAME%"

if not exist "%TEST_PATH%" (
    echo [SKIP] %TEST_NAME%: directory not found
    exit /b 0
)

for /f "delims=" %%F in ('dir /b "%TEST_PATH%\*.xml" 2^>nul') do (
    echo Running %TEST_NAME%\%%~nF...
    "%PREVABS%" -i "%TEST_PATH%\%%~nF.xml" --hm > "%TEST_PATH%\%%~nF.log" 2>&1
    if errorlevel 1 (
        echo [FAIL] %TEST_NAME%\%%~nF
    ) else (
        echo [PASS] %TEST_NAME%\%%~nF
    )
)
exit /b 0
