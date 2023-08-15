# Require Flex & Bison as generator
# find_package(FLEX 2.5.4 REQUIRED)
# find_package(BISON 2.4.1 REQUIRED)

set(PARSER_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
# set(PARSER_DIR "${CMAKE_CURRENT_BINARY_DIR}")

set(LEXER_CPP "${PARSER_DIR}/tokens.cpp")
set(LEXER_DEF "${PARSER_DIR}/tokens.hpp")
set(PARSER_CPP "${PARSER_DIR}/parser.cpp")
set(PARSER_DEF "${PARSER_DIR}/parser.hpp")

# Generate tokenizer & parser via Flex & Bison
# flex_target(LEXER "tokens.l" "${LEXER_CPP}" DEFINES_FILE "${LEXER_DEF}")
# bison_target(PARSER "parser.y" "${PARSER_CPP}" DEFINES_FILE "${PARSER_DEF}")
# add_flex_bison_dependency(LEXER PARSER)

set(SOURCE_FILES "${LEXER_CPP}" "${PARSER_CPP}" "ast.cpp")
set(PARSER_INCLUDE ${PARSER_DIR} ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)

add_library(parser STATIC ${SOURCE_FILES})
target_include_directories(parser PRIVATE "${PARSER_INCLUDE}")
