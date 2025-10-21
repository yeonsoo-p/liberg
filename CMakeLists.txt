cmake_minimum_required(VERSION 3.10)
project(liberg VERSION 0.1.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Library source files
set(LIBERG_SOURCES
    src/erginfo.c
)

set(LIBERG_HEADERS
    include/erginfo.h
)

# Create static library
add_library(liberg STATIC ${LIBERG_SOURCES} ${LIBERG_HEADERS})

# Create shared library
add_library(liberg_shared SHARED ${LIBERG_SOURCES} ${LIBERG_HEADERS})
set_target_properties(liberg_shared PROPERTIES OUTPUT_NAME liberg)

# Include directories
target_include_directories(liberg PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_include_directories(liberg_shared PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Test executable
add_executable(test_erginfo test/test_erginfo.c)
target_link_libraries(test_erginfo PRIVATE liberg)

# Example executable
add_executable(example example/example.c)
target_link_libraries(example PRIVATE liberg)

# Enable testing
enable_testing()
add_test(NAME erginfo_tests COMMAND test_erginfo)

# Installation rules
install(TARGETS liberg liberg_shared
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${LIBERG_HEADERS} DESTINATION include/liberg)
