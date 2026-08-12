if(NOT DEFINED V2_EXECUTABLE OR NOT DEFINED V3_EXECUTABLE)
  message(FATAL_ERROR "V2_EXECUTABLE and V3_EXECUTABLE are required")
endif()

execute_process(COMMAND "${V2_EXECUTABLE}" OUTPUT_VARIABLE v2 RESULT_VARIABLE v2_result)
execute_process(COMMAND "${V3_EXECUTABLE}" OUTPUT_VARIABLE v3 RESULT_VARIABLE v3_result)
if(NOT v2_result EQUAL 0 OR NOT v3_result EQUAL 0)
  message(FATAL_ERROR "A corpus executable failed")
endif()

string(REPLACE "\n" ";" v2_lines "${v2}")
set(accepted 0)
foreach(line IN LISTS v2_lines)
  if(line MATCHES "^case\\|[^|]+\\|1\\|")
    math(EXPR accepted "${accepted} + 1")
    string(FIND "${v3}" "${line}\n" position)
    if(position EQUAL -1)
      message(FATAL_ERROR "V3 compatibility mismatch for V2 snapshot:\n${line}")
    endif()
  endif()
endforeach()

if(accepted EQUAL 0)
  message(FATAL_ERROR "V2 accepted none of the p1_generator fixtures")
endif()

message(STATUS "V3 matched ${accepted} telegram snapshots accepted by V2")

