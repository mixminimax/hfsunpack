# ---------------------------------------------------------------------------
# monocypher (deps/monocypher)
#
# Bundled single-translation-unit crypto library, built and linked statically.
# Exposes a 'monocypher' target with its include directory attached.
# ---------------------------------------------------------------------------
add_library(monocypher STATIC deps/monocypher/monocypher.c)
target_include_directories(monocypher INTERFACE deps/monocypher)
