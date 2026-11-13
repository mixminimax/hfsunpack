# ---------------------------------------------------------------------------
# Third-party dependencies
#
# Included from the top-level CMakeLists.txt. Each dependency lives in its
# own file and, once processed, provides a plain target name to link against
# (e.g. 'hfs_inflate', 'monocypher').
# ---------------------------------------------------------------------------
include(${CMAKE_CURRENT_LIST_DIR}/inflate.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/monocypher.cmake)
