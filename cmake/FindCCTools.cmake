# Find module for CCTools.
# Variables:
#  - CCTOOLS_FOUND: TRUE if found
#  - CCTOOLS_LIBPATH: library path
#  - CCTOOLS_INCLUDE_DIR: include dir
include(FindPackageHandleStandardArgs)
find_library(CCTOOLS_LIBPATH work_queue
             PATHS "${CCTOOLS}/lib" NO_DEFAULT_PATH)
find_path(CCTOOLS_INCLUDE_DIR work_queue.h
          PATHS "${CCTOOLS}/include/cctools" NO_DEFAULT_PATH)
get_filename_component(CCTOOLS_LIBPATH ${CCTOOLS_LIBPATH} DIRECTORY)
find_package_handle_standard_args(CCTools DEFAULT_MSG
                                  CCTOOLS_LIBPATH CCTOOLS_INCLUDE_DIR)
include_directories(${CCTOOLS_INCLUDE_DIR})
link_directories(${CCTOOLS_LIBPATH})
