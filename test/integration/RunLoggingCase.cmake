if(NOT DEFINED PREVABS OR NOT EXISTS "${PREVABS}")
  message(FATAL_ERROR "PREVABS executable not found: ${PREVABS}")
endif()

if(NOT DEFINED TEST_DIR OR NOT IS_DIRECTORY "${TEST_DIR}")
  message(FATAL_ERROR "TEST_DIR is missing or invalid: ${TEST_DIR}")
endif()

if(NOT DEFINED CASE_NAME OR CASE_NAME STREQUAL "")
  message(FATAL_ERROR "CASE_NAME is required")
endif()

set(input_xml "${TEST_DIR}/${CASE_NAME}.xml")
set(check_file "${TEST_DIR}/${CASE_NAME}.check.txt")
set(log_file "${TEST_DIR}/${CASE_NAME}.log")
set(dump_file "${TEST_DIR}/${CASE_NAME}.dcel_dump.txt")

if(NOT EXISTS "${input_xml}")
  message(FATAL_ERROR "Integration input not found: ${input_xml}")
endif()
if(NOT EXISTS "${check_file}")
  message(FATAL_ERROR "Logging check file not found: ${check_file}")
endif()

file(STRINGS "${check_file}" check_lines)

set(expected_exit 0)
set(extra_args "")
set(required_substrings "")
set(forbidden_substrings "")

foreach(line IN LISTS check_lines)
  string(STRIP "${line}" line)
  if(line STREQUAL "" OR line MATCHES "^#")
    continue()
  elseif(line MATCHES "^exit=([0-9]+)$")
    set(expected_exit "${CMAKE_MATCH_1}")
  elseif(line MATCHES "^args=(.*)$")
    set(arg_string "${CMAKE_MATCH_1}")
    separate_arguments(parsed_args WINDOWS_COMMAND "${arg_string}")
    list(APPEND extra_args ${parsed_args})
  elseif(line MATCHES "^contains=(.*)$")
    list(APPEND required_substrings "${CMAKE_MATCH_1}")
  elseif(line MATCHES "^absent=(.*)$")
    list(APPEND forbidden_substrings "${CMAKE_MATCH_1}")
  else()
    message(FATAL_ERROR
      "Unsupported directive in ${check_file}: ${line}")
  endif()
endforeach()

file(REMOVE "${log_file}" "${dump_file}")

execute_process(
  COMMAND "${PREVABS}" -i "${input_xml}" --hm ${extra_args}
  WORKING_DIRECTORY "${TEST_DIR}"
  RESULT_VARIABLE run_exit
  OUTPUT_VARIABLE run_stdout
  ERROR_VARIABLE run_stderr
)

if(NOT run_exit STREQUAL expected_exit)
  message(FATAL_ERROR
    "Unexpected exit code for ${CASE_NAME}: expected ${expected_exit}, "
    "got ${run_exit}\nstdout:\n${run_stdout}\nstderr:\n${run_stderr}")
endif()

if(NOT EXISTS "${log_file}")
  message(FATAL_ERROR "Expected log file was not created: ${log_file}")
endif()

file(READ "${log_file}" log_text)

foreach(needle IN LISTS required_substrings)
  string(FIND "${log_text}" "${needle}" match_pos)
  if(match_pos EQUAL -1)
    message(FATAL_ERROR
      "Expected log substring not found for ${CASE_NAME}: ${needle}")
  endif()
endforeach()

foreach(needle IN LISTS forbidden_substrings)
  string(FIND "${log_text}" "${needle}" match_pos)
  if(NOT match_pos EQUAL -1)
    message(FATAL_ERROR
      "Forbidden log substring was found for ${CASE_NAME}: ${needle}")
  endif()
endforeach()
