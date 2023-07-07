set(SOURCE_FILES "opt.cpp")

add_library(opt ${SOURCE_FILES}) 

target_include_directories(opt PRIVATE ${CMAKE_SOURCE_DIR}/src/ir)