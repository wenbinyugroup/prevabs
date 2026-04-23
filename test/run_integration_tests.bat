@echo off
REM Test runner for PreVABS integration tests
REM
REM Usage:
REM   run_integration_tests.bat [filter]         Run tests (default)
REM   run_integration_tests.bat clean [filter]   Clean generated output files
REM
REM   filter  Directory prefix to match, e.g.:
REM             t1        matches t1_strip
REM             t2_z      matches only t2_z
REM           Omit to process all test directories.

setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "TEST_DIR=%SCRIPT_DIR%integration"
set "PREVABS=%SCRIPT_DIR%..\build_msvc\Release\prevabs.exe"

set "ACTION=run"
set "FILTER="

if /i "%~1"=="clean" (
    set "ACTION=clean"
    set "FILTER=%~2"
) else (
    set "ACTION=run"
    set "FILTER=%~1"
)

if "%ACTION%"=="run" (
    if not exist "%PREVABS%" (
        echo Error: prevabs.exe not found at %PREVABS%
        echo Please build the project first.
        exit /b 1
    )
    if "!FILTER!"=="" (
        echo Running all integration tests...
    ) else (
        echo Running integration tests matching "!FILTER!"...
    )
) else (
    if "!FILTER!"=="" (
        echo Cleaning all integration test outputs...
    ) else (
        echo Cleaning integration test outputs matching "!FILTER!"...
    )
)

set "PASS_COUNT=0"
set "FAIL_COUNT=0"
set "TIMEOUT_COUNT=0"

for /d %%D in ("%TEST_DIR%\t*") do (
    call :process_dir "%%D" "%%~nxD"
)

if "%ACTION%"=="run" (
    echo.
    echo Results: !PASS_COUNT! passed, !FAIL_COUNT! failed, !TIMEOUT_COUNT! timed out
)
exit /b 0


:process_dir
set "DIR_PATH=%~1"
set "DIR_NAME=%~2"
set "INDEX_FILE=!DIR_PATH!\INDEX.txt"

REM Apply filter: directory name must start with FILTER
if not "!FILTER!"=="" (
    echo !DIR_NAME! | findstr /b /i "!FILTER!" >nul 2>&1
    if errorlevel 1 exit /b 0
)

if not exist "!INDEX_FILE!" (
    echo [SKIP] !DIR_NAME!: no INDEX.txt
    exit /b 0
)

if "%ACTION%"=="run" (
    call :run_dir "!DIR_PATH!" "!DIR_NAME!"
) else (
    call :clean_dir "!DIR_PATH!" "!DIR_NAME!"
)
exit /b 0


:run_dir
set "DIR_PATH=%~1"
set "DIR_NAME=%~2"
set "INDEX_FILE=!DIR_PATH!\INDEX.txt"
set "IN_MAIN=0"
set "RAN_COUNT=0"

for /f "usebackq tokens=*" %%L in ("!INDEX_FILE!") do (
    set "LINE=%%L"
    if "!LINE!"=="main" (
        set "IN_MAIN=1"
    ) else if "!LINE!"=="support" (
        set "IN_MAIN=0"
    ) else if "!LINE!"=="" (
        set "IN_MAIN=0"
    ) else if "!IN_MAIN!"=="1" (
        set "PREFIX=!LINE:~0,2!"
        if "!PREFIX!"=="- " (
            set "XML_FILE=!LINE:~2!"
            set "BASE=!XML_FILE:.xml=!"
            set "XML_PATH=!DIR_PATH!\!XML_FILE!"
            if exist "!XML_PATH!" (
                echo Running !DIR_NAME!\!BASE!...
                set "LOG_PATH=!DIR_PATH!\!BASE!.log"
                powershell -NoProfile -ExecutionPolicy Bypass -File "!SCRIPT_DIR!run_with_timeout.ps1" -Exe "!PREVABS!" -InputXml "!XML_PATH!" -LogFile "!LOG_PATH!"
                set "RUN_EXIT=!ERRORLEVEL!"
                set "STATUS=FAIL"
                if "!RUN_EXIT!"=="0"   set "STATUS=PASS"
                if "!RUN_EXIT!"=="124" set "STATUS=TIMEOUT"
                if "!STATUS!"=="PASS" (
                    echo [PASS] !DIR_NAME!\!BASE!
                    set /a PASS_COUNT+=1
                ) else if "!STATUS!"=="TIMEOUT" (
                    echo [TIMEOUT] !DIR_NAME!\!BASE!
                    set /a TIMEOUT_COUNT+=1
                ) else (
                    echo [FAIL] !DIR_NAME!\!BASE!
                    set /a FAIL_COUNT+=1
                )
                set /a RAN_COUNT+=1
            ) else (
                echo [SKIP] !DIR_NAME!\!XML_FILE!: file not found
            )
        )
    )
)

if "!RAN_COUNT!"=="0" (
    echo [SKIP] !DIR_NAME!: no main entries in INDEX.txt
)
exit /b 0


:clean_dir
set "DIR_PATH=%~1"
set "DIR_NAME=%~2"
set "INDEX_FILE=!DIR_PATH!\INDEX.txt"

echo Cleaning !DIR_NAME!...

for /f "usebackq eol=| tokens=*" %%F in ('dir /b /a-d "!DIR_PATH!" 2^>nul') do (
    set "FNAME=%%F"
    set "SHOULD_DELETE=1"

    REM Always keep INDEX.txt
    if /i "!FNAME!"=="INDEX.txt" set "SHOULD_DELETE=0"

    REM Keep files listed under main or support in INDEX.txt
    if "!SHOULD_DELETE!"=="1" (
        findstr /i /c:"- !FNAME!" "!INDEX_FILE!" >nul 2>&1
        if not errorlevel 1 set "SHOULD_DELETE=0"
    )

    if "!SHOULD_DELETE!"=="1" (
        echo   Del: !FNAME!
        del "!DIR_PATH!\!FNAME!" >nul 2>&1
    )
)
exit /b 0
