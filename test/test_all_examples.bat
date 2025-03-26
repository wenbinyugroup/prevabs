@echo off

rem Define a list variable to hold all the test file names
set TEST_FILES=^
    t1_strip\strip ^
    t1_strip\strip_two_lines ^
    t1_strip\strip_two_segs ^
    t2_z\z ^
    t3_v\v ^
    t3_v\v_offset_out ^
    t4_I\i_2c ^
    t4_I\i_web ^
    t5_T\t ^
    t6_circle\circle ^
    t7_box\box

set EXE_PATH=..\build_msvc\Release\prevabs.exe
set PARAMS=-h -vabs -v

cd test

if "%1" == "run" (

    rem Loop through each file in the list and run the command
    for %%f in (%TEST_FILES%) do (
        "%EXE_PATH%" -i %%f.xml %PARAMS%
        vabs %%f.sg
    )
) else if "%1" == "show" (
    rem Loop through each file in the list and show the cs in gmsh
    for %%f in (%TEST_FILES%) do (
        start "" gmsh %%f.geo_unrolled %%f.msh %%f.opt
    )
) else if "%1" == "clean" (
    rem Loop through each file in the list and delete the output files
    for %%f in (%TEST_FILES%) do (
        del %%f.sg %%f.sg.K %%f.geo_unrolled %%f.msh
    )
) else (
    echo "Usage: test_all_examples.bat [run|show|clean]"
)

cd ..
