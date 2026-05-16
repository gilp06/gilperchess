cmake_minimum_required(VERSION 3.10)

project(gilperchess C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)



add_library(incbin INTERFACE)
target_include_directories(incbin INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/incbin
)








add_executable(gilperchess)
target_sources(gilperchess 
    PRIVATE
        src/main.c
        src/board.c
        src/types.c
        src/move_gen.c
        src/move_select.c
        src/utils.c
        src/uci.c
        src/hashtable.c
        src/search.c
        src/eval.c
        src/nnue.c
)
target_compile_options(gilperchess PRIVATE -Wall -march=native -O3 -flto -funroll-loops)
target_link_libraries(gilperchess PRIVATE incbin)
 
string(TIMESTAMP BUILD_TIME "%Y%m%d-%H%M%S")

set(OLD_DIR ${CMAKE_BINARY_DIR}/old)

add_custom_command(TARGET gilperchess POST_BUILD   
    COMMAND ${CMAKE_COMMAND} -E make_directory ${OLD_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:gilperchess>
        ${OLD_DIR}/gilperchess-${BUILD_TIME}$<TARGET_FILE_SUFFIX:gilperchess>
)

file(GLOB NNUE_FILES "${CMAKE_SOURCE_DIR}/nnue/*.bin")

add_custom_command(
    TARGET gilperchess POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_BINARY_DIR}/nnue"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${NNUE_FILES}
            "${CMAKE_BINARY_DIR}/nnue"
)
