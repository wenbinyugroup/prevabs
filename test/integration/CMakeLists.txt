cmake_minimum_required(VERSION 3.14)
project(prevabs_integration_tests NONE)

enable_testing()

# Path to prevabs must be supplied by the caller (-DPREVABS=<path>).
set(PREVABS "" CACHE FILEPATH "Path to prevabs executable")
if(NOT EXISTS "${PREVABS}")
    message(FATAL_ERROR
        "prevabs executable not found. Supply its path:\n"
        "  cmake -S . -B build -DPREVABS=../../build_msvc/Release/prevabs.exe")
endif()

# Parse one INDEX.txt and register a CTest entry for every file under 'main'.
function(register_tests dir_name)
    set(test_dir   "${CMAKE_SOURCE_DIR}/${dir_name}")
    set(index_file "${test_dir}/INDEX.txt")
    if(NOT EXISTS "${index_file}")
        return()
    endif()

    file(READ "${index_file}" content)
    string(REPLACE "\r\n" "\n" content "${content}")
    string(REPLACE "\r"   "\n" content "${content}")
    string(REPLACE "\n"   ";"  lines   "${content}")

    set(in_main FALSE)
    foreach(line IN LISTS lines)
        string(STRIP "${line}" line)
        if(line STREQUAL "main")
            set(in_main TRUE)
        elseif(line STREQUAL "support" OR line STREQUAL "")
            set(in_main FALSE)
        elseif(in_main AND line MATCHES "^- ")
            string(SUBSTRING "${line}" 2 -1 xml_file)
            get_filename_component(base "${xml_file}" NAME_WE)
            add_test(
                NAME             "${dir_name}/${base}"
                COMMAND          "${PREVABS}" -i "${xml_file}" --hm
                WORKING_DIRECTORY "${test_dir}"
            )
            set_tests_properties("${dir_name}/${base}" PROPERTIES
                TIMEOUT 15
                LABELS  "${dir_name}"
            )
            message(STATUS "  registered: ${dir_name}/${base}")
        endif()
    endforeach()
endfunction()

# Discover every t* subdirectory and register its tests.
file(GLOB _dirs
    LIST_DIRECTORIES TRUE
    RELATIVE "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/t*"
)
foreach(_dir IN LISTS _dirs)
    if(IS_DIRECTORY "${CMAKE_SOURCE_DIR}/${_dir}")
        register_tests("${_dir}")
    endif()
endforeach()
