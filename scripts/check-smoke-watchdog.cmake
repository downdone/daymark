# An isolated late-stage stall must trip the application's own 45-second stall guard.
# The guard bounds the time without progress, so reaching phase 436 legitimately takes
# as long as the whole run; the outer TIMEOUT must cover the full chain plus the stall.
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env QT_QPA_PLATFORM=offscreen DAYMARK_SMOKE_STALL_PHASE=436 "${DAYMARK_BINARY}" --smoke-test
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 150)
if(NOT result STREQUAL "3" OR NOT "${output}${errors}" MATCHES "UI smoke timeout at phase 436")
    message(FATAL_ERROR "Late-stage watchdog did not reject the incomplete run: ${result}\n${output}\n${errors}")
endif()
message(STATUS "Late-stage stall rejected with exit 3 at phase 436")
