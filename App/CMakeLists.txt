cmake_minimum_required(VERSION 3.20)

project(app LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT WIN32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif ()
set(CMAKE_PROJECT_VERSION "0.0.1")
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_LIST_DIR}/../cmake/modules)
include(TestBigEndian)
TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
if (IS_BIG_ENDIAN)
    add_compile_definitions(BIG_ENDIAN)
    message(STATUS "BIG_ENDIAN")
else ()
    add_compile_definitions(LITTLE_ENDIAN)
    message(STATUS "LITTLE_ENDIAN")
endif ()


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
if (NOT TARGET DeviceId)
    find_package(DeviceId REQUIRED)
endif (NOT TARGET DeviceId)
if (NOT TARGET DigitalStage::AuthStatic)
    find_package(DigitalStage::AuthStatic REQUIRED)
endif (NOT TARGET DigitalStage::AuthStatic)
# UDP Discovery - find other devices in local network
if (NOT TARGET udp-discovery)
    find_package(udp-discovery REQUIRED)
endif (NOT TARGET udp-discovery)
# pLog -Logger
if (NOT TARGET plog)
    find_package(plog REQUIRED)
endif ()
# QT6
if (NOT TARGET Qt6)
    find_package(Qt6 COMPONENTS Core Widgets LinguistTools REQUIRED)
endif (NOT TARGET Qt6)
# Keychain
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/keychain EXCLUDE_FROM_ALL)


#################################################
#
#   Configure artifact
#
#################################################
add_executable(${PROJECT_NAME} MACOSX_BUNDLE)
qt_create_translation(QM_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/i18n/DigitalStage_de.ts
        ${CMAKE_CURRENT_SOURCE_DIR}/i18n/DigitalStage_fr.ts)
set_source_files_properties(DigitalStage_de.ts DigitalStage_fr.ts
        PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/i18n")
qt_add_translations(${PROJECT_NAME} TS_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/i18n/DigitalStage_de.ts
        ${CMAKE_CURRENT_SOURCE_DIR}/i18n/DigitalStage_fr.ts)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "Digital Stage Connector")
if (APPLE)
    set(APP_ICON ${CMAKE_CURRENT_SOURCE_DIR}/assets/appicon.icns)
    set_source_files_properties(${APP_ICON} PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
endif (APPLE)
set_property(TARGET ${PROJECT_NAME} PROPERTY AUTOMOC ON)
set_property(TARGET ${PROJECT_NAME} PROPERTY AUTORCC ON)
set_property(TARGET ${PROJECT_NAME} PROPERTY AUTOUIC ON)
target_compile_definitions(${PROJECT_NAME} PRIVATE SIGNUP_URL="${SIGNUP_URL}")
target_compile_definitions(${PROJECT_NAME} PRIVATE AUTH_URL="${AUTH_URL}")
target_compile_definitions(${PROJECT_NAME} PRIVATE API_URL="${API_URL}")
target_compile_definitions(${PROJECT_NAME} PRIVATE STAGE_URL="${STAGE_URL}")
target_compile_definitions(${PROJECT_NAME} PRIVATE SETTINGS_URL="${SETTINGS_URL}")
target_sources(${PROJECT_NAME}
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
target_include_directories(${PROJECT_NAME} PRIVATE ${Qt6_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME}
        PRIVATE
        core
        DeviceId::DeviceId
        Qt6::Widgets
        keychain
        DigitalStage::AuthStatic
        )
if (APPLE)
    enable_language(OBJC)
    find_library(CORE_FOUNDATION Foundation)
    find_library(AV_FOUNDATION AVFoundation)
    target_sources(${PROJECT_NAME}
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/macos.h
            ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/macos_internal.mm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/macos_internal.h
            )
    target_link_libraries(${PROJECT_NAME}
            PRIVATE
            ${CORE_FOUNDATION}
            ${AV_FOUNDATION}
            )
endif (APPLE)


################################################
#
#   Bundling
#
#################################################
set_target_properties(${PROJECT_NAME} PROPERTIES
        BUNDLE True
        MACOSX_BUNDLE_GUI_IDENTIFIER de.tobiashegemann.digital-stage.app
        MACOSX_BUNDLE_BUNDLE_NAME App
        MACOSX_BUNDLE_COPYRIGHT MIT
        MACOSX_BUNDLE_BUNDLE_VERSION ${CMAKE_PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${CMAKE_PROJECT_VERSION}
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/MacOSXBundleInfo.plist.in
        MACOSX_BUNDLE_ICON_FILE appicon.icns
        )


#################################################
#
#   Installation
#
#################################################
install(
        TARGETS ${PROJECT_NAME}
        EXPORT "${TARGETS_EXPORT_NAME}"
        BUNDLE DESTINATION . COMPONENT Runtime
)
if (CMAKE_BUILD_TYPE MATCHES Release)
    if (APPLE)
        find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${_qt_bin_dir}")
        # First use macdeployqt to bundle the qt libs, but do NOT sign yet
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND "${MACDEPLOYQT_EXECUTABLE}"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/../.."
                -always-overwrite
                COMMENT "Running macdeployqt..."
                )
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND
                ${CMAKE_INSTALL_NAME_TOOL}
                -change
                "@rpath/libkeychain.dylib"
                "@executable_path/../Frameworks/libkeychain.dylib"
                "$<TARGET_FILE:${PROJECT_NAME}>"
                COMMENT "Fixing libkeychain rpath"
                VERBATIM
                )
        install(CODE "
            include(BundleUtilities)
            #message(STATUS \"Fixing bundle\")
            #fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/Digital Stage Connector.app\" \"\" \"$<TARGET_FILE_DIR:${PROJECT_NAME}>/../plugins\")
            message(STATUS \"Signing bundle\")
            execute_process(COMMAND codesign --force --deep --sign \"${CODESIGN_CERTIFICATE_NAME}\" \"\${CMAKE_INSTALL_PREFIX}/Digital Stage Connector.app\")"
                DESTINATION .
                COMPONENT Runtime)
    endif (APPLE)
endif ()


#################################################
#
#   Pack
#
#################################################
set(CPACK_PACKAGE_NAME "Digital Stage Connector")
set(CPACK_PACKAGE_VENDOR "Tobias Hegemann")
set(CPACK_PACKAGE_DESCRIPTION "Background service to connect with Digital Stage via P2P")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Background service to connect with Digital Stage via P2P")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://www.digital-stage.org")
set(CPACK_PACKAGE_CONTACT "Tobias Hegemann <tobias.hegemann@digital-stage.org")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Digital Stage Connector")
set(CPACK_PACKAGE_ICON ${CMAKE_CURRENT_LIST_DIR}/assets/logo.png)
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_LIST_DIR}/../LICENSE)
set(CPACK_RESOURCE_FILE_README ${CMAKE_CURRENT_LIST_DIR}/../README.md)
set(CPACK_COMPONENTS_ALL Runtime)

if (UNIX)
    if (APPLE)
        # macOS
        set(CPACK_DMG_VOLUME_NAME "Install Connector")
        set(CPACK_DMG_DS_STORE_SETUP_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/Packaging_DMGSetup.scpt)
        set(CPACK_DMG_BACKGROUND_IMAGE ${CMAKE_CURRENT_LIST_DIR}/assets/DMGBackground.tif)
        set(CPACK_BUNDLE_APPLE_CERT_APP ${CODESIGN_CERTIFICATE_NAME})
        set(CPACK_SOURCE_STRIP_FILES "")
        set(CPACK_GENERATOR "DragNDrop")
    else ()
        # Linux
        set(CPACK_GENERATOR "ZIP")
    endif ()
else ()
    # Windows
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
    set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
    include(InstallRequiredSystemLibraries)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/qtDeploy/"
            COMMAND "${CMAKE_COMMAND}" -E
            env PATH="${_qt_bin_dir}" "${WINDEPLOYQT_EXECUTABLE}"
            --verbose 0
            --no-compiler-runtime
            --no-angle
            --no-webkit2
            --no-quick-import
            --no-translations
            --dir "${CMAKE_CURRENT_BINARY_DIR}/qtDeploy/" $<TARGET_FILE:${PROJECT_NAME}>
            COMMENT "Deploying Qt..."
            )

    install(
            DIRECTORY "${CMAKE_BINARY_DIR}/qtDeploy/"
            DESTINATION .
    )
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_GENERATOR "NSIS")
endif (UNIX)

include(CPack)


