set(SOURCE_FILES ConstSpread.cpp BasicOperation.cpp LoopInvariant.cpp CombineInstr.cpp SimplifyJump.cpp opt.cpp DeleteDeadCode.cpp)

add_library(opt ${SOURCE_FILES}) 

target_link_libraries(opt PRIVATE ir)
target_include_directories(opt PRIVATE ${CMAKE_SOURCE_DIR}/src/ir)