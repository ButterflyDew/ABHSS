cmake_minimum_required(VERSION 3.16)
project(ABHSS_Short CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Threads REQUIRED)

# libstdc++ 在 GCC 9/10 中声明了整数 from_chars，但尚未提供可链接的浮点
# 重载。编译并链接一次探针，使大图读取器能在这些 Linux 工具链上自动回退
# 到 strtod，而现代工具链继续使用更快的浮点 from_chars。
include(CheckCXXSourceCompiles)
check_cxx_source_compiles([[
    #include <charconv>
    int main() {
        const char text[] = "1.25";
        double value = 0.0;
        const auto result = std::from_chars(
            text, text + 4, value, std::chars_format::general);
        return result.ptr == text + 4 ? 0 : 1;
    }
]] GST_FLOAT_FROM_CHARS_LINKS)
if(GST_FLOAT_FROM_CHARS_LINKS)
    add_compile_definitions(GST_HAVE_FLOAT_FROM_CHARS=1)
else()
    message(STATUS "Floating-point std::from_chars unavailable; using strtod graph-weight parser")
endif()

option(ABHSS_ENABLE_IPO "Enable IPO/LTO for Release paper binaries when supported" ON)
set(ABHSS_IPO_SUPPORTED FALSE)
if(ABHSS_ENABLE_IPO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ABHSS_IPO_SUPPORTED
                        OUTPUT ABHSS_IPO_ERROR
                        LANGUAGES CXX)
    if(NOT ABHSS_IPO_SUPPORTED)
        message(WARNING "IPO/LTO is unavailable for this toolchain: ${ABHSS_IPO_ERROR}")
    endif()
endif()

# 两个 helper 让三个正式本地二进制使用同一编译策略，同时避免把本项目的
# 警告选项强加给不可修改的第三方 header。
function(abhss_enable_project_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

function(abhss_enable_release_ipo target)
    if(ABHSS_IPO_SUPPORTED)
        set_property(TARGET ${target}
                     PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    endif()
endfunction()

# 论文计时统一使用 Release/O2，不依赖 IDE 默认值。
if(MSVC)
    add_compile_options(/utf-8)
    foreach(config_name RELEASE RELWITHDEBINFO)
        # CMake/用户可能预置 /O1；论文策略固定追加 /O2，并依赖 MSVC
        # “最后一个冲突选项生效”的规则统一实际优化级别。
        if(NOT CMAKE_CXX_FLAGS_${config_name} MATCHES "(^| )/O2($| )")
            string(APPEND CMAKE_CXX_FLAGS_${config_name} " /O2")
        endif()
    endforeach()
    if(NOT CMAKE_CXX_FLAGS_RELEASE MATCHES "(^| )/Ob")
        string(APPEND CMAKE_CXX_FLAGS_RELEASE " /Ob2")
    endif()
else()
    foreach(config_name RELEASE RELWITHDEBINFO)
        # GCC/Clang 的 CMake Release 默认通常含 -O3。显式把 -O2 追加在
        # 其后，确保文档、Windows 和 Linux 的正式性能策略真正一致。
        if(NOT CMAKE_CXX_FLAGS_${config_name} MATCHES "(^| )-O2($| )")
            string(APPEND CMAKE_CXX_FLAGS_${config_name} " -O2")
        endif()
    endforeach()
endif()

set(COMMON_SOURCES
    src/main.cpp
    src/common/graph_io.cpp
    src/common/query_io.cpp
    src/common/query_feasibility.cpp
    src/common/output_manager.cpp
    src/common/memory_usage.cpp)

set(ABHSS_SOURCES
    src/abhss/preprocess.cpp
    src/abhss/core.cpp
    src/abhss/pipeline.cpp
    src/abhss/forward.cpp
    src/abhss/solver.cpp
    src/abhss/adjoint.cpp)

add_executable(pruneddp ${COMMON_SOURCES} src/pruneddp/pruneddp.cpp)
target_compile_definitions(pruneddp PRIVATE ABHSS_BUILD_PRUNEDDP=1)

add_executable(dpbf ${COMMON_SOURCES} src/dpbf/dpbf.cpp)
target_compile_definitions(dpbf PRIVATE ABHSS_BUILD_DPBF=1)

set(GROUP_STEINER_TREE_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/GroupSteinerTree/ysgraph_20210203")
set(VENDORED_BOOST_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/boost_1_85_0")
set(GPU4GST_PRUNEDDP_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/GPU4GST-sigmod/code/PrunedDP++/include")
if(EXISTS "${GROUP_STEINER_TREE_HEADERS}/graph_hash_of_mixed_weighted/graph_hash_of_mixed_weighted_Basic_and_BasicPlus_vertex_edge_weighted.h")
    if(EXISTS "${VENDORED_BOOST_HEADERS}/boost/heap/fibonacci_heap.hpp")
        add_executable(basic_plus ${COMMON_SOURCES} src/baselines/basic_plus.cpp)
        target_compile_definitions(basic_plus PRIVATE ABHSS_BUILD_BASIC_PLUS=1)
        target_include_directories(basic_plus PRIVATE
            "${GROUP_STEINER_TREE_HEADERS}"
            "${VENDORED_BOOST_HEADERS}")
        # 2021 上游 header 在全局写入 `using namespace std`。只把 adapter
        # translation unit 降为 C++14，避免 C++17 `std::byte` 与 Windows SDK
        # 的全局 `byte` 冲突；共用 runner 和仓库其余部分仍保持 C++17。
        if(MSVC)
            set_source_files_properties(src/baselines/basic_plus.cpp
                PROPERTIES COMPILE_OPTIONS "/std:c++14")
        else()
            set_source_files_properties(src/baselines/basic_plus.cpp
                PROPERTIES COMPILE_OPTIONS "-std=c++14")
        endif()
    else()
        message(STATUS "Basic+ adapter disabled: third_party/boost_1_85_0 is missing")
    endif()
endif()

if(EXISTS "${GPU4GST_PRUNEDDP_HEADERS}/CPUNONHOP.h" AND
   EXISTS "${VENDORED_BOOST_HEADERS}/boost/heap/fibonacci_heap.hpp")
    add_executable(gpu4gst_pruneddp_artifact
        ${COMMON_SOURCES} src/baselines/gpu4gst_pruneddp.cpp)
    target_compile_definitions(gpu4gst_pruneddp_artifact PRIVATE
        ABHSS_BUILD_GPU4GST_PRUNEDDP=1)
    target_include_directories(gpu4gst_pruneddp_artifact PRIVATE
        "${GPU4GST_PRUNEDDP_HEADERS}"
        "${VENDORED_BOOST_HEADERS}")
endif()

add_executable(abhss ${COMMON_SOURCES} ${ABHSS_SOURCES})
target_compile_definitions(abhss PRIVATE ABHSS_BUILD_ABHSS=1)

foreach(target pruneddp dpbf abhss)
    target_link_libraries(${target} PRIVATE Threads::Threads)
    # ABHSS 的基础/增强配置由同一个可执行文件在查询开始前选定。对所有正式
    # 计时方法统一启用 Release IPO，避免把编译策略误当作算法增强。
    abhss_enable_release_ipo(${target})
    abhss_enable_project_warnings(${target})
    if(WIN32)
        target_link_libraries(${target} PRIVATE psapi)
    endif()
endforeach()

if(TARGET basic_plus)
    target_link_libraries(basic_plus PRIVATE Threads::Threads)
    if(WIN32)
        target_link_libraries(basic_plus PRIVATE psapi)
    endif()
endif()

if(TARGET gpu4gst_pruneddp_artifact)
    target_link_libraries(gpu4gst_pruneddp_artifact PRIVATE Threads::Threads)
    if(WIN32)
        target_link_libraries(gpu4gst_pruneddp_artifact PRIVATE psapi)
    endif()
endif()

add_executable(prepare_gpu4gst tools/gpu4gst_data/prepare_gpu4gst.cpp)
add_executable(build_imdb_graph tools/data/build_imdb_graph.cpp)
add_executable(audit_query_feasibility tools/data/audit_query_feasibility.cpp)
foreach(target prepare_gpu4gst build_imdb_graph audit_query_feasibility)
    abhss_enable_project_warnings(${target})
endforeach()

include(CTest)
if(BUILD_TESTING)
    add_executable(graph_io_regression
        tests/graph_io_regression.cpp
        src/common/graph_io.cpp)
    target_compile_definitions(graph_io_regression PRIVATE
        GST_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME fast_graph_io_structure
             COMMAND graph_io_regression)
    abhss_enable_project_warnings(graph_io_regression)

    add_executable(query_io_regression
        tests/query_io_regression.cpp
        src/common/query_io.cpp)
    target_compile_definitions(query_io_regression PRIVATE
        GST_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME query_io_validation
             COMMAND query_io_regression)
    abhss_enable_project_warnings(query_io_regression)

    add_executable(abhss_zero_weight_regression
        tests/abhss_zero_weight_regression.cpp
        src/common/query_feasibility.cpp
        ${ABHSS_SOURCES})
    target_link_libraries(abhss_zero_weight_regression PRIVATE Threads::Threads)
    add_test(NAME abhss_zero_weight_witness
             COMMAND abhss_zero_weight_regression)
    abhss_enable_project_warnings(abhss_zero_weight_regression)

    add_executable(abhss_configuration_regression
        tests/abhss_configuration_regression.cpp
        src/common/query_feasibility.cpp
        ${ABHSS_SOURCES})
    target_link_libraries(abhss_configuration_regression PRIVATE Threads::Threads)
    add_test(NAME abhss_configuration_exactness
             COMMAND abhss_configuration_regression)
    abhss_enable_project_warnings(abhss_configuration_regression)
endif()
