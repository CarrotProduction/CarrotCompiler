# Define test files
set(FUNCTIONAL_TESTS_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/case/functional")
set(HIDDEN_FUNCTIONAL_TESTS_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/case/hidden_functional")
set(PERFORMANCE_TESTS_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/case/performance")
set(FINAL_PERFORMANCE_TESTS_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/case/final_performance")

# set(BUILD_PERFORMANCE_TESTS true)
# set(BUILD_IR_TESTING true)

# Define test function
function(add_test_dir testdir)
  file(GLOB files "${testdir}/*.sy")

  foreach(file ${files})
    get_filename_component(testfile "${file}" NAME_WE)
    get_filename_component(testcate "${testdir}" NAME)
    set(testname "${testcate}_${testfile}")
    if(BUILD_IR_TESTING)
      add_test(NAME "${testname}_llir"
        COMMAND ${CMAKE_COMMAND}
        -D "COMPILER=${CMAKE_BINARY_DIR}/compiler"
        -D "RUNTIME=${CMAKE_BINARY_DIR}/runtime"
        -D "TEST_DIR=${testdir}"
        -D "TEST_NAME=${testfile}"
        -P ${CMAKE_SOURCE_DIR}/cmake/LLVMIRTest.cmake)
    endif(BUILD_IR_TESTING)
    add_test(NAME "${testname}_asm"
      COMMAND ${CMAKE_COMMAND}
      -D "COMPILER=${CMAKE_BINARY_DIR}/compiler"
      -D "RUNTIME=${CMAKE_BINARY_DIR}/runtime"
      -D "TEST_DIR=${testdir}"
      -D "TEST_NAME=${testfile}"
      -P ${CMAKE_SOURCE_DIR}/cmake/RISCVTest.cmake)
  endforeach()
endfunction()

# Functional tests
add_test_dir("${FUNCTIONAL_TESTS_DIR}")

# Hidden functional tests
add_test_dir("${HIDDEN_FUNCTIONAL_TESTS_DIR}")

if(BUILD_PERFORMANCE_TESTS)
  # Performance tests
  add_test_dir("${PERFORMANCE_TESTS_DIR}")

  # Final performance tests
  add_test_dir("${FINAL_PERFORMANCE_TESTS_DIR}")
endif(BUILD_PERFORMANCE_TESTS)