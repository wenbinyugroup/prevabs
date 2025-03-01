@echo off

set EXE_PATH=../build_msvc/Release/prevabs.exe
set PARAMS=-h -vabs -e

rem Define a list variable to hold all the test file names
set TEST_FILES=^
    t1_strip/strip.xml ^
    t1_strip/strip_two_lines.xml ^
    t1_strip/strip_two_segs.xml ^
    t2_z/z.xml ^
    t3_v/v.xml ^
    t3_v/v_offset_out.xml ^
    t4_I/i_2c.xml ^
    t4_I/i_web.xml ^
    t5_T/t.xml

rem Loop through each file in the list and run the command
for %%f in (%TEST_FILES%) do (
    "%EXE_PATH%" -i %%f %PARAMS%
)
