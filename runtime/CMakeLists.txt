cmake_minimum_required(VERSION 3.21)

project(SysYRuntime VERSION 1.0
  DESCRIPTION "SysY Language official runtime environment"
  LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_COMPILER "riscv64-linux-gnu-gcc")

set(INCLUDE_DIRECTORY "${PROJECT_SOURCE_DIR}/include")
include_directories(${INCLUDE_DIRECTORY})

set(SOURCE_FILES "${PROJECT_SOURCE_DIR}/src/sylib.c")

add_custom_target(sysy-ir ALL
  COMMAND clang ${CMAKE_C_FLAGS}
  -I ${INCLUDE_DIRECTORY}
  -S -emit-llvm
  ${SOURCE_FILES}
  -o ${CMAKE_CURRENT_BINARY_DIR}/sysy.ll
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  COMMENT "Compiling SysY Runtime Library to LLVM-IR"
  SOURCES ${SOURCE_FILES})

add_library(sysy STATIC ${SOURCE_FILES})

# target_compile_options(sysy PUBLIC ${CMAKE_C_FLAGS} -flto)
# target_compile_options(sysy PUBLIC ${CMAKE_C_FLAGS} -emit-llvm -S)