cmake_minimum_required(VERSION 3.16)
project(ABHSS_Short CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Threads REQUIRED)

# 论文计时统一使用 Release/O2，不依赖 IDE 默认值。
if(MSVC)
    add_compile_options(/utf-8)
    foreach(config_name RELEASE RELWITHDEBINFO)
        if(NOT CMAKE_CXX_FLAGS_${config_name} MATCHES "(^| )/O[123x]")
            string(APPEND CMAKE_CXX_FLAGS_${config_name} " /O2")
        endif()
    endforeach()
    if(NOT CMAKE_CXX_FLAGS_RELEASE MATCHES "(^| )/Ob")
        string(APPEND CMAKE_CXX_FLAGS_RELEASE " /Ob2")
    endif()
else()
    foreach(config_name RELEASE RELWITHDEBINFO)
        if(NOT CMAKE_CXX_FLAGS_${config_name} MATCHES "(^| )-O[123s]")
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
    src/abhss/light.cpp
    src/abhss/heavy.cpp)

add_executable(pruneddp ${COMMON_SOURCES} src/pruneddp/pruneddp.cpp)
target_compile_definitions(pruneddp PRIVATE ABHSS_BUILD_PRUNEDDP=1)

# Probe-only twins keep diagnostics completely out of the formal timing
# binaries.  They execute the same algorithms and differ only by sparse phase
# markers/counters enabled for the 15-hour feasibility probe.
add_executable(pruneddp_probe ${COMMON_SOURCES} src/pruneddp/pruneddp.cpp)
target_compile_definitions(pruneddp_probe PRIVATE
    ABHSS_BUILD_PRUNEDDP=1 GST_ENABLE_PROBE_DIAGNOSTICS=1)

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
        # The 2021 upstream header places ``using namespace std`` at global
        # scope.  Compile only the adapter translation unit as C++14 so that
        # the later C++17 ``std::byte`` name cannot collide with Windows SDK's
        # global ``byte`` typedef.  The shared runner remains C++17.
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

add_executable(abhss_light ${COMMON_SOURCES} ${ABHSS_SOURCES})
target_compile_definitions(abhss_light PRIVATE ABHSS_BUILD_LIGHT=1)

add_executable(abhss_light_probe ${COMMON_SOURCES} ${ABHSS_SOURCES})
target_compile_definitions(abhss_light_probe PRIVATE
    ABHSS_BUILD_LIGHT=1 GST_ENABLE_PROBE_DIAGNOSTICS=1)

add_executable(abhss_light_no_early ${COMMON_SOURCES} ${ABHSS_SOURCES})
target_compile_definitions(abhss_light_no_early PRIVATE
    ABHSS_BUILD_LIGHT=1 ABHSS_BUILD_LIGHT_NO_EARLY=1 ABHSS_ABLATE_NO_EARLY=1)

add_executable(abhss_light_no_witness ${COMMON_SOURCES} ${ABHSS_SOURCES})
target_compile_definitions(abhss_light_no_witness PRIVATE
    ABHSS_BUILD_LIGHT=1 ABHSS_BUILD_LIGHT_NO_WITNESS=1 ABHSS_ABLATE_NO_WITNESS=1)

add_executable(abhss_heavy ${COMMON_SOURCES} ${ABHSS_SOURCES})
target_compile_definitions(abhss_heavy PRIVATE ABHSS_BUILD_HEAVY=1)

add_executable(abhss_heavy_probe ${COMMON_SOURCES} ${ABHSS_SOURCES})
target_compile_definitions(abhss_heavy_probe PRIVATE
    ABHSS_BUILD_HEAVY=1 GST_ENABLE_PROBE_DIAGNOSTICS=1)

add_executable(abhss_heavy_forward ${COMMON_SOURCES} ${ABHSS_SOURCES})
target_compile_definitions(abhss_heavy_forward PRIVATE ABHSS_BUILD_HEAVY_FORWARD=1)

foreach(target pruneddp pruneddp_probe dpbf abhss_light abhss_light_probe
        abhss_light_no_early abhss_light_no_witness abhss_heavy
        abhss_heavy_probe abhss_heavy_forward)
    target_link_libraries(${target} PRIVATE Threads::Threads)
    # Shared Light/Heavy modules expose useful cross-translation-unit inlining.
    # Apply the same Release IPO rule to every timed in-repository method so the
    # paper comparison keeps one compiler policy rather than favoring ABHSS.
    set_property(TARGET ${target}
                 PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
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
add_executable(compare_graphs tools/data/compare_graphs.cpp)
add_executable(build_jaccard_graph tools/data/build_jaccard_graph.cpp)
add_executable(build_movielens_graph tools/data/build_movielens_graph.cpp)
add_executable(build_rdf_graph tools/data/build_rdf_graph.cpp)
add_executable(build_imdb_graph tools/data/build_imdb_graph.cpp)
add_executable(build_aminer_dblp_graph tools/data/build_aminer_dblp_graph.cpp)
target_include_directories(build_aminer_dblp_graph PRIVATE "${VENDORED_BOOST_HEADERS}")
add_executable(audit_query_feasibility tools/data/audit_query_feasibility.cpp)

include(CTest)
if(BUILD_TESTING)
    add_executable(abhss_zero_weight_regression
        tests/abhss_zero_weight_regression.cpp
        src/common/query_feasibility.cpp
        ${ABHSS_SOURCES})
    target_link_libraries(abhss_zero_weight_regression PRIVATE Threads::Threads)
    add_test(NAME abhss_zero_weight_witness
             COMMAND abhss_zero_weight_regression)

    add_executable(abhss_variant_regression
        tests/abhss_variant_regression.cpp
        src/common/query_feasibility.cpp
        ${ABHSS_SOURCES})
    target_link_libraries(abhss_variant_regression PRIVATE Threads::Threads)
    add_test(NAME abhss_variant_exactness
             COMMAND abhss_variant_regression)
endif()
