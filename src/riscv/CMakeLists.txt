set(SOURCE_FILES "riscv.cpp")

add_library(riscv STATIC ${SOURCE_FILES})

target_link_libraries(riscv PRIVATE ir)
target_include_directories(riscv PRIVATE ${CMAKE_SOURCE_DIR}/src/ir)