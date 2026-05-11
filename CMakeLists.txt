cmake_minimum_required(VERSION 3.10)

project(gilperchess C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
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
)
target_compile_options(gilperchess PRIVATE -Wall -march=native -O3 -flto -funroll-loops)
 
string(TIMESTAMP BUILD_TIME "%Y%m%d-%H%M%S")

set(OLD_DIR ${CMAKE_BINARY_DIR}/old)

add_custom_command(TARGET gilperchess POST_BUILD   
    COMMAND ${CMAKE_COMMAND} -E make_directory ${OLD_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:gilperchess>
        ${OLD_DIR}/gilperchess-${BUILD_TIME}$<TARGET_FILE_SUFFIX:gilperchess>
)
