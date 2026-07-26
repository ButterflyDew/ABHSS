cmake_minimum_required(VERSION 3.16)
project(ABHSS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

add_executable(abhss
    src/main.cpp
    src/common/graph_io.cpp
    src/common/query_io.cpp
    src/common/query_feasibility.cpp
    src/abhss/preprocess.cpp
    src/abhss/core.cpp
    src/abhss/forward.cpp
    src/abhss/adjoint.cpp
    src/abhss/solver.cpp)

if(MSVC)
    target_compile_options(abhss PRIVATE /utf-8 /W4)
else()
    target_compile_options(abhss PRIVATE -Wall -Wextra -Wpedantic)
endif()
