cmake_minimum_required(VERSION 3.20)
project(hfsunpack)

# Third-party dependencies (see cmake/).
include(cmake/dependencies.cmake)

# ---------------------------------------------------------------------------
# hfs
# ---------------------------------------------------------------------------
add_library(hfs STATIC
    hfsKeyUtils.c
    hfsUtils.c
    utils.c
    hfs.c
    hfs.h
    inflate.c
    inflate.h
    packetReader.c
    dir.c
    xcontentDecrypt.c)

add_library(utils STATIC memoryPool.c)

# config.h (generated from config.h.in) selects the decompression backend.
target_include_directories(hfs PRIVATE ${CMAKE_BINARY_DIR})

target_link_libraries(hfs PUBLIC monocypher hfs_inflate utils)

# ---------------------------------------------------------------------------
# executables
# ---------------------------------------------------------------------------
add_executable(hfsunpack hfsunpack.c)
target_link_libraries(hfsunpack PRIVATE hfs)

# ---------------------------------------------------------------------------
# tests
# ---------------------------------------------------------------------------
option(HFS_BUILD_TESTS "Build hfsunpack tests" OFF)
if(HFS_BUILD_TESTS)
    enable_testing()

    # Round-trip test for the inflate wrapper (compress + decompress).
    add_executable(inflate_test test/inflate_test.c)
    target_include_directories(inflate_test
                               PRIVATE ${CMAKE_BINARY_DIR} # config.h
                                      ${CMAKE_SOURCE_DIR}) # inflate.h
    target_link_libraries(inflate_test PRIVATE hfs)
    add_test(NAME inflate_test COMMAND inflate_test)
endif()

# add_executable(keygen keygen.c)
