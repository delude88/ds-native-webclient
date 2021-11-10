# - Try to find JACK
# Once done, this will define
#
#  Jack_FOUND - system has JACK
#  Jack_INCLUDE_DIRS - the JACK include directories
#  Jack_LIBRARIES - link these to use JACK

# Use pkg-config to get hints about paths

if (Jack_LIBRARIES AND Jack_INCLUDE_DIRS)
    set(Jack_FOUND TRUE)
else ()
    find_package(PkgConfig QUIET)
    if (PKG_CONFIG_FOUND)
        pkg_check_modules(Jack_PKGCONF jack)
    endif (PKG_CONFIG_FOUND)

    # Include dir
    find_path(Jack_INCLUDE_DIR
            NAMES jack/jack.h
            PATH_SUFFIXES include includes
            PATHS ${Jack_PKGCONF_INCLUDE_DIRS}
            )

    # Library
    find_library(Jack_LIBRARY
            NAMES jack jack64 libjack libjack64
            PATH_SUFFIXES lib
            PATHS ${Jack_PKGCONF_LIBRARY_DIRS}
            )

    find_package(PackageHandleStandardArgs)
    find_package_handle_standard_args(Jack DEFAULT_MSG Jack_LIBRARY Jack_INCLUDE_DIR)

    if (Jack_FOUND)
        set(Jack_LIBRARIES ${Jack_LIBRARY})
        set(Jack_INCLUDE_DIRS ${Jack_INCLUDE_DIR})
    endif (Jack_FOUND)

    mark_as_advanced(Jack_LIBRARY Jack_INCLUDE_DIR)
endif ()
