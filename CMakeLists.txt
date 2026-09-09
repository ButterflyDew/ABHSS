cmake_minimum_required(VERSION 3.16)
project(ABHSS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

add_executable(abhss
    src/main.cpp
    src/io.cpp
    src/abhss/preprocess.cpp
    src/abhss/path_growth_upper.cpp
    src/abhss/core.cpp
    src/abhss/forward.cpp
    src/abhss/adjoint.cpp
    src/abhss/solver.cpp)

target_compile_options(abhss PRIVATE -Wall -Wextra -Wpedantic)
