cmake_minimum_required(VERSION 3.20)

project(DigitalStageConnectorApp LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT WIN32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif ()
set(CMAKE_PROJECT_VERSION "0.0.1")
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
if (NOT TARGET Qt6 AND NOT TARGET Qt5)
    find_package(Qt6 COMPONENTS Core Widgets LinguistTools REQUIRED)
endif (NOT TARGET Qt6 AND NOT TARGET Qt5)
# Keychain
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/keychain EXCLUDE_FROM_ALL)


#################################################
#
#   Configure artifact
#
#################################################
set(target ${PROJECT_NAME})
add_executable(${target} MACOSX_BUNDLE)
qt_create_translation(QM_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/i18n/DigitalStage_de.ts
        ${CMAKE_CURRENT_SOURCE_DIR}/i18n/DigitalStage_fr.ts)
set_source_files_properties(DigitalStage_de.ts DigitalStage_fr.ts
        PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/i18n")
if (QT_VERSION GREATER_EQUAL 6)
    qt_add_translations(${target} TS_FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/i18n/DigitalStage_de.ts
            ${CMAKE_CURRENT_SOURCE_DIR}/i18n/DigitalStage_fr.ts)
    set_target_properties(${target} PROPERTIES OUTPUT_NAME "Digital Stage Connector")
endif (QT_VERSION GREATER_EQUAL 6)
if (APPLE)
    set(APP_ICON ${CMAKE_CURRENT_SOURCE_DIR}/assets/appicon.icns)
    set_source_files_properties(${APP_ICON} PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
endif (APPLE)
set_target_properties(${target} PROPERTIES
        AUTOMOC ON
        AUTORCC ON
        AUTOUIC ON
        AUTORCC_OPTIONS "--compress;9"
        )
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
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/App.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/App.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/TrayIcon.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/TrayIcon.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/LoginDialog.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/LoginDialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/LoginDialog.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/src/KeyStore.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/KeyStore.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/resources.qrc
        ${APP_ICON}
        ${QM_FILES})
target_include_directories(${target} PRIVATE ${Qt_INCLUDE_DIRS})
target_link_libraries(${target}
        PRIVATE
        DigitalStageConnectorCore
        DeviceId::DeviceId
        Qt::Widgets
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
set(INSTALLER_NAME "Install Connector")
set_target_properties(${target} PROPERTIES OUTPUT_NAME ${BUNDLE_NAME})
set(CPACK_OUTPUT_CONFIG_FILE "${CMAKE_BINARY_DIR}/CPackConfigApp.cmake")
set(OUTPUT_NAME ${BUNDLE_NAME})
set(CPACK_PACKAGE_VENDOR "Tobias Hegemann")
set(CPACK_PACKAGE_NAME ${INSTALLER_NAME})
set(CPACK_PACKAGE_CONTACT "Tobias Hegemann <tobias.hegemann@googlemail.com")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/delude88")
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
if (WIN32)
    # Windows
    install(TARGETS ${target} RUNTIME DESTINATION bin)
    set_target_properties(${target}
            PROPERTIES
            CMAKE_WIN32_EXECUTABLE ON)
    # Bundle Library Files
    if (CMAKE_BUILD_TYPE_UPPER STREQUAL "DEBUG")
        set(WINDEPLOYQT_ARGS --debug)
    else ()
        set(WINDEPLOYQT_ARGS --release)
    endif ()

    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
    add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/winqt/"
            COMMAND "${CMAKE_COMMAND}" -E
            env PATH="${_qt_bin_dir}" "${WINDEPLOYQT_EXECUTABLE}"
            ${WINDEPLOYQT_ARGS}
            --verbose 0
            --no-compiler-runtime
            --no-opengl-sw
            --dir "${CMAKE_CURRENT_BINARY_DIR}/winqt/"
            $<TARGET_FILE:${target}>
            COMMENT "Deploying Qt..."
            )
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/winqt/" DESTINATION bin)
    set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
    include(InstallRequiredSystemLibraries)
elseif (APPLE)
    # Apple
    set(CMAKE_MACOSX_BUNDLE ON)
    #set(CMAKE_MACOSX_RPATH ON)
    install(TARGETS ${target} BUNDLE DESTINATION .
            RUNTIME DESTINATION .)
    set_target_properties(${target}
            PROPERTIES
            CMAKE_MACOSX_BUNDLE ON
            BUNDLE True
            MACOSX_BUNDLE_GUI_IDENTIFIER de.tobiashegemann.digital-stage.app
            MACOSX_BUNDLE_BUNDLE_NAME ${BUNDLE_NAME}
            MACOSX_BUNDLE_COPYRIGHT MIT
            MACOSX_BUNDLE_BUNDLE_VERSION ${CMAKE_PROJECT_VERSION}
            MACOSX_BUNDLE_SHORT_VERSION_STRING ${CMAKE_PROJECT_VERSION}
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/MacOSXBundleInfo.plist.in
            MACOSX_BUNDLE_ICON_FILE appicon.icns)
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${_qt_bin_dir}")
    # First use macdeployqt to bundle the qt libs, but do NOT sign yet
    add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${MACDEPLOYQT_EXECUTABLE}"
            "$<TARGET_FILE_DIR:${target}>/../.."
            -always-overwrite
            -verbose=2
            -sign-for-notarization=${CODESIGN_CERTIFICATE_NAME}
            COMMENT "Running macdeployqt.."
            VERBATIM
            )
    include(InstallRequiredSystemLibraries)
    #if (CODESIGN_CERTIFICATE_NAME)
        #install(CODE "
        #    message(STATUS \"Signing bundle\")
        #    execute_process(COMMAND codesign --force --deep --sign \"${CODESIGN_CERTIFICATE_NAME}\" \"\${CMAKE_INSTALL_PREFIX}/${BUNDLE_NAME}.app\")"
        #        DESTINATION .)
        #set(CPACK_BUNDLE_APPLE_CERT_APP ${CODESIGN_CERTIFICATE_NAME})
    #endif (CODESIGN_CERTIFICATE_NAME)
    set(CPACK_DMG_VOLUME_NAME ${CPACK_PACKAGE_NAME})
    set(CPACK_DMG_DS_STORE_SETUP_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/Packaging_DMGSetup.scpt)
    set(CPACK_DMG_BACKGROUND_IMAGE ${CMAKE_CURRENT_LIST_DIR}/assets/DMGBackground.tif)
    set(CPACK_GENERATOR "DragNDrop")
elseif (UNIX)
    # Linux
    install(TARGETS ${target} RUNTIME DESTINATION bin)
    # Debian
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${PROJECT_URL}")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
    find_program(LINUXDEPLOY_EXECUTABLE linuxdeploy linuxdeploy-x86_64.AppImage HINTS "${_qt_bin_dir}")
    if (LINUXDEPLOY_EXECUTABLE)
        if (CMAKE_VERSION VERSION_LESS 3.13)
            linuxdeployqt("${CPACK_PACKAGE_DIRECTORY}/_CPack_Packages/Linux/AppImage" "share/applications/example.desktop")
        else ()
            set(CPACK_GENERATOR "External;${CPACK_GENERATOR}")
            configure_file(${CMAKE_CURRENT_SOURCE_DIR}/CPackLinuxDeployQt.cmake.in "${CMAKE_BINARY_DIR}/CPackExternal.cmake")
            set(CPACK_EXTERNAL_PACKAGE_SCRIPT "${CMAKE_BINARY_DIR}/CPackExternal.cmake")
        endif ()
    endif ()
endif ()
# Icons
set(icon.path icons/hicolor/64x64/apps)
set(icon.files assets/icon-64x64.png)
set(iconsvg.path icons/hicolor/scalable/apps)
set(iconsvg.files assets/logo.svg)
foreach (items IN ITEMS icon iconsvg)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/${${items}.files}
            DESTINATION share/${${items}.path}
            PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
endforeach ()
if (CMAKE_BUILD_TYPE_UPPER MATCHES "^(DEBUG|RELWITHDEBINFO)$")
    set(CPACK_STRIP_FILES FALSE)
else ()
    set(CPACK_STRIP_FILES TRUE)
endif ()
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
    file(SHA1 ${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy clang_tidy_sha1)
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
        message(AUTHOR_WARNING "clazy not found.")
    endif ()
endif ()