cmake_minimum_required(VERSION 3.10)
project(liberg VERSION 0.1.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Optimization flags
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler optimization flags
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_C_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
    set(CMAKE_C_FLAGS_DEBUG "-O0 -g -Wall -Wextra -Wpedantic")
elseif(CMAKE_C_COMPILER_ID MATCHES "MSVC")
    set(CMAKE_C_FLAGS_RELEASE "/O2 /DNDEBUG")
    set(CMAKE_C_FLAGS_DEBUG "/Od /Zi /Wall")
endif()

# Library source files
set(LIBERG_SOURCES
    src/arena.c
    src/infofile.c
    src/infofile_arena.c
)

set(LIBERG_SIMD_SOURCES
    src/arena.c
    src/infofile.c
    src/infofile_arena.c
    src/infofile_simd.c
    src/infofile_arena_simd.c
    src/infofile_arena_simd_opt.c
    src/simple_parser.c
    src/infofile_simple.c
)

set(LIBERG_HEADERS
    include/arena.h
    include/infofile.h
    include/infofile_arena.h
    include/infofile_simd.h
    include/infofile_arena_simd.h
    include/infofile_arena_simd_opt.h
    include/simple_parser.h
    include/infofile_simple.h
)

# Create static library
add_library(liberg STATIC ${LIBERG_SOURCES} ${LIBERG_HEADERS})

# Create shared library
add_library(liberg_shared SHARED ${LIBERG_SOURCES} ${LIBERG_HEADERS})
set_target_properties(liberg_shared PROPERTIES OUTPUT_NAME liberg)

# Create SIMD-enabled static library
add_library(liberg_simd STATIC ${LIBERG_SIMD_SOURCES} ${LIBERG_HEADERS})
target_compile_options(liberg_simd PRIVATE -mavx2)

# Create SIMD-enabled shared library
add_library(liberg_simd_shared SHARED ${LIBERG_SIMD_SOURCES} ${LIBERG_HEADERS})
set_target_properties(liberg_simd_shared PROPERTIES OUTPUT_NAME liberg_simd)
target_compile_options(liberg_simd_shared PRIVATE -mavx2)

# Include directories
target_include_directories(liberg PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_include_directories(liberg_shared PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_include_directories(liberg_simd PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_include_directories(liberg_simd_shared PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Test executables
add_executable(test_arena test/test_arena.c)
target_link_libraries(test_arena PRIVATE liberg)

add_executable(test_infofile test/test_infofile.c)
target_link_libraries(test_infofile PRIVATE liberg)

# Correctness test executable (with SIMD support)
add_executable(test_correctness test/test_correctness.c)
target_link_libraries(test_correctness PRIVATE liberg_simd)

# Benchmark executable (with SIMD support)
add_executable(bench_infofile test/bench_infofile.c)
target_link_libraries(bench_infofile PRIVATE liberg_simd)

# Enable testing
enable_testing()
add_test(NAME infofile_tests COMMAND test_infofile)
add_test(NAME correctness_test COMMAND test_correctness)

# Installation rules
install(TARGETS liberg liberg_shared liberg_simd liberg_simd_shared
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${LIBERG_HEADERS} DESTINATION include/liberg)
