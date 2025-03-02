@echo off

set EXE_PATH=../build_msvc/Release/prevabs.exe
set PARAMS=-h -vabs -v

rem Define a list variable to hold all the test file names
set TEST_FILES=^
    t1_strip/strip ^
    t1_strip/strip_two_lines ^
    t1_strip/strip_two_segs ^
    t2_z/z ^
    t3_v/v ^
    t3_v/v_offset_out ^
    t4_I/i_2c ^
    t4_I/i_web ^
    t5_T/t

rem Loop through each file in the list and run the command
for %%f in (%TEST_FILES%) do (
    "%EXE_PATH%" -i %%f.xml %PARAMS%
    vabs %%f.sg
)
