cmake_minimum_required(VERSION 3.21)

set(SOURCE_FILES "riscv.cpp" instruction.cpp optimize.cpp
    backend.cpp regalloc.cpp regAnalysis.cpp LoopInfo.cpp)

add_library(riscv STATIC ${SOURCE_FILES})

target_link_libraries(riscv PRIVATE ir opt)
include_directories(riscv PRIVATE ${CMAKE_SOURCE_DIR}/src/ir)
include_directories(riscv PRIVATE ${CMAKE_SOURCE_DIR}/src/opt)