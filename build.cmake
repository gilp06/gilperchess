execute_process(
    COMMAND cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    RESULT_VARIABLE res
)

if(NOT res EQUAL 0)
    message(FATAL_ERROR "CMake Configuration Failed")
endif()

execute_process(
    COMMAND cmake --build build
    RESULT_VARIABLE res
)

if(NOT res EQUAL 0)
    message(FATAL_ERROR "Build failed")
endif()
