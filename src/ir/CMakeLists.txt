cmake_minimum_required(VERSION 3.21)

set(SOURCE_FILES "ir.cpp" "genIR.cpp")

add_library(ir STATIC ${SOURCE_FILES})

target_include_directories(ir PRIVATE "${PARSER_INCLUDE}")
