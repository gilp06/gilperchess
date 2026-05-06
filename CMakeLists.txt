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
        src/utils.c
        src/uci.c
        src/hashtable.c
        src/search.c
        src/eval.c
)
target_compile_options(gilperchess PRIVATE -Wall -march=native -O3 -flto -funroll-loops)
 
