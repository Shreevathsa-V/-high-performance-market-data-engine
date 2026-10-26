cmake_minimum_required(VERSION 3.16)

project(
    HighPerformanceMatchingEngine
    VERSION 1.0.0
    DESCRIPTION "C++20 market data feed handler and matching engine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/build/bin")

option(BUILD_TESTING "Build project tests" ON)
option(BUILD_BENCHMARKS "Build benchmark executables" ON)

# ------------------------------------------------------------
# Compiler warnings
# ------------------------------------------------------------

add_library(project_options INTERFACE)

target_compile_options(project_options INTERFACE
    -Wall
    -Wextra
    -Wpedantic
)

# ------------------------------------------------------------
# Include directories
#
# The project currently has core headers in the repository
# root and feed headers under include/feed/.
# ------------------------------------------------------------

add_library(project_includes INTERFACE)

target_include_directories(project_includes INTERFACE
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/include/core"
    "${PROJECT_SOURCE_DIR}/include/feed"
)

# ------------------------------------------------------------
# Matching engine
# ------------------------------------------------------------

add_library(order_book_core STATIC
    src/core/OrderBook.cpp
)

target_link_libraries(order_book_core PUBLIC
    project_options
    project_includes
)

# ------------------------------------------------------------
# Feed-processing pipeline
# ------------------------------------------------------------

add_library(feed_pipeline STATIC
    src/feed/Decoder.cpp
    src/feed/FeedHandler.cpp
    src/feed/StreamProcessor.cpp
)

target_link_libraries(feed_pipeline PUBLIC
    order_book_core
    project_options
    project_includes
)

# ------------------------------------------------------------
# Main matching-engine demonstration
# ------------------------------------------------------------

add_executable(order_book_demo
    src/main.cpp
)

target_link_libraries(order_book_demo PRIVATE
    order_book_core
)

# ------------------------------------------------------------
# Linux TCP servers
# ------------------------------------------------------------

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")

    add_executable(epoll_feed_server_lt
        src/feed/EpollTcpServer.cpp
    )

    target_link_libraries(epoll_feed_server_lt PRIVATE
        feed_pipeline
    )

    add_executable(epoll_feed_server_et
        src/feed/EpollTcpServer.cpp
    )

    target_compile_definitions(epoll_feed_server_et PRIVATE
        USE_EDGE_TRIGGERED
    )

    target_link_libraries(epoll_feed_server_et PRIVATE
        feed_pipeline
    )

    add_executable(tcp_feed_server
        src/feed/TcpServer.cpp
    )

    target_link_libraries(tcp_feed_server PRIVATE
        feed_pipeline
    )

endif()

# ------------------------------------------------------------
# Tests
# ------------------------------------------------------------

if(BUILD_TESTING)

    enable_testing()

    function(add_project_test target_name source_file)
        add_executable(${target_name} ${source_file})

        target_link_libraries(${target_name} PRIVATE
            feed_pipeline
        )

        add_test(
            NAME ${target_name}
            COMMAND $<TARGET_FILE:${target_name}>
        )
    endfunction()

    add_project_test(
        test_matching
        tests/unit/test_matching.cpp
    )

    add_project_test(
        test_order_types
        tests/unit/test_order_types.cpp
    )

    add_project_test(
        test_fok_levels
        tests/unit/test_fok_levels.cpp
    )

    add_project_test(
        test_randomized
        tests/unit/test_randomized.cpp
    )

    add_project_test(
        test_decoder
        tests/test_decoder.cpp
    )

    add_project_test(
        test_cancel_decoder
        tests/test_cancel_decoder.cpp
    )

    add_project_test(
        test_feed_handler
        tests/test_feed_handler.cpp
    )

    add_project_test(
        test_stream_processor
        tests/test_stream_processor.cpp
    )

endif()

# ------------------------------------------------------------
# Benchmarks
# ------------------------------------------------------------

if(BUILD_BENCHMARKS)

    function(add_project_benchmark target_name source_file)
        add_executable(${target_name} ${source_file})

        target_link_libraries(${target_name} PRIVATE
            feed_pipeline
        )
    endfunction()

    # Matching-engine latency benchmark from the project root.
    add_project_benchmark(
        benchmark_matching_latency
        benchmarks/benchmark_matching_latency.cpp
    )

    add_project_benchmark(
        profile_matching
        benchmarks/profile_matching.cpp
    )

    # Compare matching performance across price-level layouts.
    add_project_benchmark(
        benchmark_price_levels
        benchmarks/benchmark_price_levels.cpp
    )

    # Feed-pipeline latency benchmark.
    add_project_benchmark(
        benchmark_feed_latency
        benchmarks/benchmark_latency.cpp
    )

    # Other benchmark programs.
    set(BENCHMARK_SOURCES
        benchmarks/benchmark.cpp
        benchmarks/benchmark_add.cpp
        benchmarks/benchmark_cancel.cpp
        benchmarks/benchmark_decoder.cpp
        benchmarks/benchmark_event_logging.cpp
        benchmarks/benchmark_index_compare.cpp
        benchmarks/benchmark_index_robust.cpp
        benchmarks/benchmark_list_allocation.cpp
        benchmarks/benchmark_logging_compare.cpp
        benchmarks/benchmark_map_allocation.cpp
        benchmarks/benchmark_match_profile.cpp
        benchmarks/benchmark_node_pool.cpp
        benchmarks/benchmark_order_book.cpp
        benchmarks/benchmark_order_index.cpp
        benchmarks/benchmark_profile.cpp
        benchmarks/benchmark_stream_processor.cpp
    )

    foreach(source_file IN LISTS BENCHMARK_SOURCES)

        get_filename_component(
            benchmark_name
            "${source_file}"
            NAME_WE
        )

        add_project_benchmark(
            "${benchmark_name}"
            "${source_file}"
        )

    endforeach()

endif()
