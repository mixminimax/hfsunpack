# ---------------------------------------------------------------------------
# Decompression backend for inflate.c (zlib streams)
#
# HFS_INFLATE_BACKEND (CACHE STRING - shown as a drop-down in the CMake GUI):
#
#   AUTO               (default)
#       1. system libdeflate (CMake CONFIG package), else
#       2. system zlib       (CMake FindZLIB module), else
#       3. bundled libdeflate (deps/libdeflate/, built from source)
#
#   LIBDEFLATE         system libdeflate, else the bundled libdeflate.
#                      zlib is never used.
#
#   ZLIB               system zlib only. Fails the configure if not found.
#
#   BUNDLED_LIBDEFLATE always build the bundled libdeflate (deps/libdeflate/).
#
# The result is exposed as:
#   - linkable target: 'hfs_inflate'
#   - <build>/config.h generated from config.h.in, defining either
#     HFS_INFLATE_WITH_LIBDEFLATE or HFS_INFLATE_WITH_ZLIB.
#     inflate.c dispatches on that macro at compile time.
# ---------------------------------------------------------------------------
set(HFS_INFLATE_BACKEND "AUTO" CACHE STRING
    "Decompression backend: AUTO, LIBDEFLATE, ZLIB or BUNDLED_LIBDEFLATE")
set_property(CACHE HFS_INFLATE_BACKEND PROPERTY STRINGS
             AUTO
             LIBDEFLATE
             ZLIB
             BUNDLED_LIBDEFLATE)

string(TOUPPER "${HFS_INFLATE_BACKEND}" HFS_INFLATE_BACKEND_NORM)
if(NOT HFS_INFLATE_BACKEND_NORM MATCHES "^(AUTO|LIBDEFLATE|ZLIB|BUNDLED_LIBDEFLATE)$")
    message(FATAL_ERROR
            "HFS_INFLATE_BACKEND='${HFS_INFLATE_BACKEND}' is not valid. "
            "Expected one of: AUTO, LIBDEFLATE, ZLIB, BUNDLED_LIBDEFLATE.")
endif()

# Legacy: HFS_USE_BUNDLED_LIBDEFLATE=ON (removed option) maps to the same thing.
if(HFS_INFLATE_BACKEND_NORM STREQUAL "AUTO" AND HFS_USE_BUNDLED_LIBDEFLATE)
    set(HFS_INFLATE_BACKEND_NORM "BUNDLED_LIBDEFLATE")
    message(STATUS "HFS_USE_BUNDLED_LIBDEFLATE is set; "
                   "treating it as HFS_INFLATE_BACKEND=BUNDLED_LIBDEFLATE")
endif()

function(hfsunpack_add_bundled_libdeflate)
    # Build only the static library: no shared lib, no tools, no tests, no install.
    set(LIBDEFLATE_BUILD_STATIC_LIB ON  CACHE BOOL "" FORCE)
    set(LIBDEFLATE_BUILD_SHARED_LIB OFF CACHE BOOL "" FORCE)
    set(LIBDEFLATE_BUILD_GZIP       OFF CACHE BOOL "" FORCE)
    set(LIBDEFLATE_BUILD_TESTS      OFF CACHE BOOL "" FORCE)
    set(LIBDEFLATE_INSTALL          OFF CACHE BOOL "" FORCE)

    add_subdirectory(deps/libdeflate ${CMAKE_BINARY_DIR}/deps/libdeflate)
    add_library(hfs_inflate ALIAS libdeflate_static)
endfunction()

# Search for the candidates the selected backend allows.
if(HFS_INFLATE_BACKEND_NORM MATCHES "^(AUTO|LIBDEFLATE)$")
    find_package(libdeflate CONFIG QUIET)
endif()

if(HFS_INFLATE_BACKEND_NORM MATCHES "^(AUTO|ZLIB)$"
   AND NOT TARGET libdeflate::libdeflate_static
   AND NOT TARGET libdeflate::libdeflate)
    find_package(ZLIB QUIET)
endif()

# Select the backend.
if(TARGET libdeflate::libdeflate_static)
    add_library(hfs_inflate ALIAS libdeflate::libdeflate_static)
    set(HFS_INFLATE_WITH_LIBDEFLATE 1)
    message(STATUS "inflate backend: system libdeflate (${libdeflate_DIR})")
elseif(TARGET libdeflate::libdeflate)
    add_library(hfs_inflate ALIAS libdeflate::libdeflate)
    set(HFS_INFLATE_WITH_LIBDEFLATE 1)
    message(STATUS "inflate backend: system libdeflate (${libdeflate_DIR})")
elseif(TARGET ZLIB::ZLIB)
    add_library(hfs_inflate ALIAS ZLIB::ZLIB)
    set(HFS_INFLATE_WITH_ZLIB 1)
    message(STATUS "inflate backend: system zlib (${ZLIB_LIBRARIES})")
else()
    if(HFS_INFLATE_BACKEND_NORM STREQUAL "ZLIB")
        message(FATAL_ERROR
                "HFS_INFLATE_BACKEND=ZLIB was requested, but no system zlib "
                "was found. Install zlib or choose another backend "
                "(AUTO, LIBDEFLATE, BUNDLED_LIBDEFLATE).")
    endif()
    hfsunpack_add_bundled_libdeflate()
    set(HFS_INFLATE_WITH_LIBDEFLATE 1)
    if(HFS_INFLATE_BACKEND_NORM STREQUAL "BUNDLED_LIBDEFLATE")
        message(STATUS "inflate backend: bundled libdeflate (deps/libdeflate) [forced]")
    else()
        message(STATUS "inflate backend: bundled libdeflate (deps/libdeflate) [fallback]")
    endif()
endif()

# Render the backend selection for inflate.c.
configure_file(${CMAKE_CURRENT_LIST_DIR}/../config.h.in
               ${CMAKE_BINARY_DIR}/config.h)
