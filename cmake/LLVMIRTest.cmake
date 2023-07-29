# arguments checking
if(NOT COMPILER)
  message(FATAL_ERROR "Require COMPILER to be defined")
endif(NOT COMPILER)

if(NOT RUNTIME)
  message(FATAL_ERROR "Require RUNTIME to be defined")
endif(NOT RUNTIME)

if(NOT TEST_DIR)
  message(FATAL_ERROR "Require TEST_DIR to be defined")
endif(NOT TEST_DIR)

if(NOT TEST_NAME)
  message(FATAL_ERROR "Require TEST_NAME to be defined")
endif(NOT TEST_NAME)

# Sources
set(TEST_SRC "${TEST_DIR}/${TEST_NAME}.sy")

if(EXISTS "${TEST_DIR}/${TEST_NAME}.in")
  set(TEST_INS "${TEST_DIR}/${TEST_NAME}.in")
else(EXISTS "${TEST_DIR}/${TEST_NAME}.in")
  set(TEST_INS "/dev/null")
endif(EXISTS "${TEST_DIR}/${TEST_NAME}.in")

set(RUNTIME_ASM "${RUNTIME}/sysy.ll")

# Generated
set(TEST_ASM "${TEST_NAME}.ll")
set(TEST_BTC "${TEST_NAME}.bc")
set(TEST_OUT "${TEST_NAME}_ir.out")
configure_file("${TEST_DIR}/${TEST_NAME}.out" "${TEST_NAME}.ref" NEWLINE_STYLE LF)
set(TEST_REF "${TEST_NAME}.ref")

# SysY to LLVM IR
execute_process(
  COMMAND
  ${COMPILER} -c ${TEST_SRC}
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  OUTPUT_FILE ${TEST_ASM}
  ERROR_VARIABLE TEST_ERR
  RESULT_VARIABLE TEST_RET
)

if(TEST_RET)
  message(SEND_ERROR "Failed: SysY Compiler Error in ${TEST_SRC}: ${TEST_ERR}")
  file(READ "${TEST_SRC}" TEST_SRC_CONTENT)
  message(NOTICE "Source File:\n ${TEST_SRC_CONTENT}")
  return()
endif(TEST_RET)

# LLVM IR & Runtime link to BitCode
execute_process(
  COMMAND
  llvm-link "${RUNTIME_ASM}" "${CMAKE_CURRENT_BINARY_DIR}/${TEST_ASM}"
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  OUTPUT_FILE ${TEST_BTC}
  ERROR_VARIABLE TEST_ERR
  RESULT_VARIABLE TEST_RET
)

if(TEST_RET)
  message(SEND_ERROR "Failed: LLVM Link Error in ${TEST_SRC}: ${TEST_ERR}")
  file(READ "${TEST_ERR}" TEST_ERR_CONTENT)
  message(NOTICE "Generated Assmebly:\n ${TEST_ERR_CONTENT}")
  return()
endif(TEST_RET)

# Run BitCode with lli
execute_process(
  COMMAND
  lli ${TEST_BTC}
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  INPUT_FILE ${TEST_INS}
  OUTPUT_VARIABLE TEST_OUT_CONTENT
  ERROR_VARIABLE TEST_ERR
  RESULT_VARIABLE TEST_RET
  TIMEOUT 10
)

# Append EOF LR, if not presented
if(TEST_OUT_CONTENT)
  string(LENGTH "${TEST_OUT_CONTENT}" TEST_OUT_LEN)
  math(EXPR TEST_OUT_LAST "${TEST_OUT_LEN} - 1")
  string(SUBSTRING "${TEST_OUT_CONTENT}" ${TEST_OUT_LAST} 1 LAST_CHAR)

  if(NOT "${LAST_CHAR}" STREQUAL "\n")
    # If it's not, append a newline to var
    set(TEST_OUT_CONTENT "${TEST_OUT_CONTENT}\n")
  endif()
endif(TEST_OUT_CONTENT)

set(TEST_OUT_CONTENT "${TEST_OUT_CONTENT}${TEST_RET}\n")
file(WRITE "${TEST_OUT}" "${TEST_OUT_CONTENT}")

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E compare_files "${TEST_OUT}" "${TEST_REF}"
  RESULT_VARIABLE TEST_RESULT
)

if(TEST_RESULT)
  # get_filename_component(TESTNAME "${TEST_ARGS}" NAME)
  # file(RENAME "${TEST_OUTPUT}" "${CMAKE_BINARY_DIR}/${TESTNAME}.out")
  # file(WRITE  "${CMAKE_BINARY_DIR}/${TESTNAME}.err" ${TEST_ERROR})
  message(SEND_ERROR "Failed: The output of ${TEST_NAME} did not match ${TEST_REF}")
  file(READ ${TEST_REF} TEST_REF_CONTENT)
  message(NOTICE "Expected Output: ${TEST_REF_CONTENT}")
  message(NOTICE "Auctual Output: ${TEST_OUT_CONTENT}")
  return()
endif(TEST_RESULT)