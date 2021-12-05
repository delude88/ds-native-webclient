cmake_minimum_required(VERSION 3.20)

project(app LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT WIN32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif ()
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_LIST_DIR}/../cmake/modules)


#################################################
#
#   Environment configuration
#
#################################################
set(SIGNUP_URL "https://dev.dstage.org/account/signup" CACHE STRING "Url link to sign up page")
set(AUTH_URL "https://auth.dstage.org" CACHE STRING "Auth server url")
set(API_URL "wss://api.dstage.org" CACHE STRING "API server websocket url (wss://...)")
set(STAGE_URL "https://dev.dstage.org/stage" CACHE STRING "Url link to active stage page")
set(SETTINGS_URL "https://dev.dstage.org/devices" CACHE STRING "Url link to settings page")


#################################################
#
#   Dependencies
#
#################################################
# DeviceId - Get unique device ID
if (NOT TARGET DigitalStageConnectorCore)
    find_package(DigitalStageConnectorCore REQUIRED)
endif (NOT TARGET DigitalStageConnectorCore)
if (NOT TARGET DeviceId)
    find_package(DeviceId REQUIRED)
endif (NOT TARGET DeviceId)
if (NOT TARGET DigitalStageAuth)
    find_package(DigitalStage REQUIRED)
endif (NOT TARGET DigitalStageAuth)
# UDP Discovery - find other devices in local network
if (NOT TARGET udp-discovery)
    find_package(udp-discovery REQUIRED)
endif (NOT TARGET udp-discovery)
# pLog -Logger
if (NOT TARGET plog)
    find_package(plog REQUIRED)
endif ()
if (NOT TARGET wxWidgets)
    set(wxBUILD_SHARED OFF)
    set(wxUSE_STL ON)
    set(wxBUILD_PRECOMP ON)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/wxWidgets EXCLUDE_FROM_ALL)
endif (NOT TARGET wxWidgets)
# Keychain
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/keychain EXCLUDE_FROM_ALL)


#################################################
#
#   Configure artifact
#
#################################################
set(target ${PROJECT_NAME})
add_executable(${target})
set_target_properties(${target} PROPERTIES OUTPUT_NAME "Digital Stage Connector")
set(RESOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/splash.png
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/logo-full.png
        )
if (APPLE)
    list(APPEND RESOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/appicon.icns
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/appicon.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/icon@2x.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/logo-full@2x.png)
    set_source_files_properties(${RESOURCES} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
endif (APPLE)
target_compile_definitions(${target}
        PRIVATE
        SIGNUP_URL="${SIGNUP_URL}"
        AUTH_URL="${AUTH_URL}"
        API_URL="${API_URL}"
        STAGE_URL="${STAGE_URL}"
        SETTINGS_URL="${SETTINGS_URL}"
        )
target_sources(${target}
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/App.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/App.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/TaskBarIcon.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/TaskBarIcon.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/LoginDialog.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/LoginDialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/KeyStore.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/KeyStore.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/forms/UI.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/forms/UI.cpp
        ${RESOURCES})
target_link_libraries(${target}
        PRIVATE
        DigitalStageConnectorCore
        DeviceId::DeviceId
        wx::core
        wx::base
        keychain
        DigitalStage::AuthStatic
        )
if (APPLE)
    enable_language(OBJC)
    find_library(CORE_FOUNDATION Foundation)
    find_library(AV_FOUNDATION AVFoundation)
    target_sources(${target}
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/macos.h
            ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/macos_internal.mm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/macos_internal.h
            )
    target_link_libraries(${target}
            PRIVATE
            ${CORE_FOUNDATION}
            ${AV_FOUNDATION}
            )
endif (APPLE)


################################################
#
#   Installation 'n' Bundling
#
#################################################
set(BUNDLE_NAME "Digital Stage Connector")
set_target_properties(${target} PROPERTIES OUTPUT_NAME ${BUNDLE_NAME})
set(CPACK_OUTPUT_CONFIG_FILE "${CMAKE_BINARY_DIR}/CPackConfigApp.cmake")
set(CPACK_PACKAGE_VENDOR "Tobias Hegemann")
set(CPACK_PACKAGE_CONTACT "Tobias Hegemann <tobias.hegemann@googlemail.com")
set(CPACK_PACKAGE_HOMEPAGE_URL ${PROJECT_HOMEPAGE_URL})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${BUNDLE_NAME})
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CPACK_PACKAGE_EXECUTABLES "${OUTPUT_NAME}" "${OUTPUT_NAME}")
set(CPACK_CREATE_DESKTOP_LINKS "${OUTPUT_NAME}")

install(TARGETS ${target}
        BUNDLE DESTINATION . COMPONENT ${target}
        RUNTIME DESTINATION . COMPONENT ${target}
        )
set(APPS \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${BUNDLE_NAME})

if (WIN32)
    # Windows
    set_target_properties(${target}
            PROPERTIES
            CMAKE_WIN32_EXECUTABLE ON)
    set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
    include(InstallRequiredSystemLibraries)
    install(CODE "
        include(BundleUtilities)
        fixup_bundle(\"${APPS}\" \"\" \"\")"
            COMPONENT ${target})
elseif (APPLE)
    # Apple
    set(CMAKE_MACOSX_RPATH ON)
    set_target_properties(${target}
            PROPERTIES
            CMAKE_MACOSX_BUNDLE ON
            MACOSX_BUNDLE True
            MACOSX_BUNDLE_GUI_IDENTIFIER de.tobiashegemann.digital-stage.app
            MACOSX_BUNDLE_BUNDLE_NAME ${BUNDLE_NAME}
            MACOSX_BUNDLE_COPYRIGHT MIT
            MACOSX_BUNDLE_BUNDLE_VERSION ${CMAKE_PROJECT_VERSION}
            MACOSX_BUNDLE_SHORT_VERSION_STRING ${CMAKE_PROJECT_VERSION}
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/MacOSXBundleInfo.plist.in
            MACOSX_BUNDLE_ICON_FILE appicon.icns)
    # First use macdeployqt to bundle the qt libs, but do NOT sign yet
    if (CODESIGN_CERTIFICATE_NAME)
        # Now codesign
        install(CODE "
            include(BundleUtilities)
            message(STATUS \"Fixup\")
            fixup_bundle(\"${APPS}.app\" \"\" \"\")
            message(STATUS \"Code-Signing bundle\")
            execute_process(COMMAND codesign --options runtime --sign \"${CODESIGN_CERTIFICATE_NAME}\" \"${APPS}.app\")"
                COMPONENT ${target})
        #set(CPACK_BUNDLE_APPLE_CERT_APP ${CODESIGN_CERTIFICATE_NAME})
        #set(CPACK_BUNDLE_APPLE_CODESIGN_PARAMETER "--force --deep --options runtime")
    else ()
        install(CODE "
        include(BundleUtilities)
            fixup_bundle(\"${APPS}.app\" \"\" \"\")"
                COMPONENT ${target})
    endif (CODESIGN_CERTIFICATE_NAME)
    include(InstallRequiredSystemLibraries)
    set(CPACK_DMG_DS_STORE_SETUP_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/Packaging_DMGSetup.scpt)
    set(CPACK_DMG_BACKGROUND_IMAGE ${CMAKE_CURRENT_LIST_DIR}/assets/DMGBackground.tif)
    set(CPACK_GENERATOR "DragNDrop")
elseif (UNIX)
    # Linux
    # Debian package
    include(GNUInstallDirs)
    install(TARGETS ${target}
            RUNTIME DESTINATION bin COMPONENT ${target}
            )
    set(CPACK_PACKAGE_CONTACT "tobias.hegemann@digital-stage.org")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Tobias Hegemann <${CPACK_PACKAGE_CONTACT}>")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${PROJECT_HOMEPAGE_URL}")
    set(CPACK_STRIP_FILES YES)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS YES)
    set(
            CPACK_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
            OWNER_READ OWNER_WRITE OWNER_EXECUTE
            GROUP_READ GROUP_EXECUTE
            WORLD_READ WORLD_EXECUTE
    )
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEB_COMPONENT_INSTALL YES)
    set(CPACK_GENERATOR "DEB")
    # App Image
    install(CODE "
        include(BundleUtilities)
            fixup_bundle(\"${APPS}\" \"\" \"\")"
            COMPONENT ${target})
endif ()
if (CMAKE_BUILD_TYPE_UPPER MATCHES "^ (DEBUG|RELWITHDEBINFO) $")
    set(CPACK_STRIP_FILES FALSE)
else ()
    set(CPACK_STRIP_FILES TRUE)
endif ()
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_ALL ${target})
include(CPack)


#################################################
#
#   Additional routines
#
#################################################
option(CMake_RUN_CLANG_TIDY "Run clang-tidy with the compiler." OFF)
if (CMake_RUN_CLANG_TIDY)
    if (CMake_SOURCE_DIR STREQUAL CMake_BINARY_DIR)
        message(FATAL_ERROR "CMake_RUN_CLANG_TIDY requires an out-of-source build!")
    endif ()
    find_program(CLANG_TIDY_COMMAND NAMES clang-tidy)
    if (NOT CLANG_TIDY_COMMAND)
        message(WARNING "CMake_RUN_CLANG_TIDY is ON but clang-tidy is not found!")
        set(CMAKE_CXX_CLANG_TIDY "" CACHE STRING "" FORCE)
    else ()
        set(CLANG_TIDY_CHECKS "-*,modernize-*")
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND};-checks=${CLANG_TIDY_CHECKS};-header-filter='${CMAKE_SOURCE_DIR}/src/*'")
    endif ()

    # Create a preprocessor definition that depends on .clang-tidy content so
    # the compile command will change when .clang-tidy changes.  This ensures
    # that a subsequent build re-runs clang-tidy on all sources even if they
    # do not otherwise need to be recompiled.  Nothing actually uses this
    # definition.  We add it to targets on which we run clang-tidy just to
    # get the build dependency on the .clang-tidy file.
    file(SHA1 ${CMAKE_CURRENT_LIST_DIR}/../.clang-tidy clang_tidy_sha1)
    set(CLANG_TIDY_DEFINITIONS "CLANG_TIDY_SHA1=${clang_tidy_sha1}")
    unset(clang_tidy_sha1)

endif ()

option(CMake_RUN_IWYU "Run include-what-you-use with the compiler." OFF)
if (CMake_RUN_IWYU)
    find_program(IWYU_COMMAND NAMES include-what-you-use iwyu)
    if (NOT IWYU_COMMAND)
        message(WARNING "CMake_RUN_IWYU is ON but include-what-you-use is not found!")
    else ()
        set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE
                "${IWYU_COMMAND};-Xiwyu;--mapping_file=${CMake_SOURCE_DIR}/Utilities/IWYU/mapping.imp;-w")
        list(APPEND CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${CMake_IWYU_OPTIONS})
    endif ()
endif ()

# Clazy is a Qt oriented code checker based on clang framework. Krazy's little brother.
set(CMake_RUN_CLAZY OFF CACHE BOOL "Add clazy check for builds")
if (ENABLE_CLAZY)
    find_program(CLAZY_EXECUTABLE NAMES clazy PATHS /usr/local/llvm/bin /usr/local/bin /opt/clazy/bin)
    if (CLAZY_EXECUTABLE)
        message(STATUS "clazy found: ${CLAZY_EXECUTABLE}")
    else ()
        message(AUTHOR_WARNING "clazy not found. ")
    endif ()
endif ()