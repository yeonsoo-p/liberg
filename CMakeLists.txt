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
    src/string_simd.c
    src/erg.c
    src/thread_pool.c
)

set(LIBERG_HEADERS
    include/arena.h
    include/infofile.h
    include/string_simd.h
    include/erg.h
    include/thread_pool.h
)

# Create static library
add_library(liberg STATIC ${LIBERG_SOURCES} ${LIBERG_HEADERS})
target_compile_options(liberg PRIVATE -mavx2)
target_include_directories(liberg PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Create shared library
add_library(liberg_shared SHARED ${LIBERG_SOURCES} ${LIBERG_HEADERS})
set_target_properties(liberg_shared PROPERTIES OUTPUT_NAME liberg)
target_compile_options(liberg_shared PRIVATE -mavx2)
target_include_directories(liberg_shared PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Test executables
add_executable(test_arena test/test_arena.c)
target_link_libraries(test_arena PRIVATE liberg)

add_executable(test_infofile test/test_infofile.c)
target_link_libraries(test_infofile PRIVATE liberg)

add_executable(test_erg test/test_erg.c)
target_link_libraries(test_erg PRIVATE liberg)

add_executable(test_thread_pool test/test_thread_pool.c)
target_link_libraries(test_thread_pool PRIVATE liberg)

# Enable testing
enable_testing()
add_test(NAME arena_test COMMAND test_arena)
add_test(NAME infofile_test COMMAND test_infofile)
add_test(NAME erg_test COMMAND test_erg)

# Installation rules
install(TARGETS liberg liberg_shared
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${LIBERG_HEADERS} DESTINATION include/liberg)
