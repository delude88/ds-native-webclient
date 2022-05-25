cmake_minimum_required(VERSION 3.20)

project(app LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT WIN32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif ()
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_LIST_DIR}/../cmake/modules)
cmake_policy(SET CMP0068 NEW)


#################################################
#
#   Environment configuration
#
#################################################
set(SIGNUP_URL "https://web.dev.digital-stage.net/account/signup" CACHE STRING "Url link to sign up page")
set(AUTH_URL "https://auth.dev.digital-stage.net" CACHE STRING "Auth server url")
set(API_URL "wss://api.dev.digital-stage.net" CACHE STRING "API server websocket url (wss://...)")
set(STAGE_URL "https://web.dev.digital-stage.net/stage" CACHE STRING "Url link to active stage page")
set(SETTINGS_URL "https://web.dev.digital-stage.net/devices" CACHE STRING "Url link to settings page")
add_compile_definitions(USE_ONLY_WEBRTC_DEVICES)


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
if (NOT TARGET wxWidgets)
    set(wxBUILD_SHARED OFF)
    set(wxUSE_STL ON)
    set(wxBUILD_PRECOMP OFF)
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
            )
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
        plog::plog
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
            WIN32_EXECUTABLE TRUE)
    set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
    include(InstallRequiredSystemLibraries)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_PACKAGE_FILE_NAME digital-stage-connector-app-win-${CMAKE_SYSTEM_PROCESSOR})
elseif (APPLE)
    # Apple
    set(CMAKE_MACOSX_RPATH ON)
    set_target_properties(${target}
            PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_GUI_IDENTIFIER de.tobiashegemann.digital-stage.app
            MACOSX_BUNDLE_BUNDLE_NAME ${BUNDLE_NAME}
            MACOSX_BUNDLE_COPYRIGHT "Copyright (c) 2021 Tobias Hegemann"
            MACOSX_BUNDLE_BUNDLE_VERSION ${CMAKE_PROJECT_VERSION}
            MACOSX_BUNDLE_SHORT_VERSION_STRING ${CMAKE_PROJECT_VERSION}
            MACOSX_BUNDLE_LONG_VERSION_STRING ${CMAKE_PROJECT_VERSION}
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
            MACOSX_BUNDLE_INFO_STRING "Online zusammen proben und auffuehren"
            CPACK_BUNDLE_APPLE_ENTITLEMENTS ${CMAKE_CURRENT_SOURCE_DIR}/Entitlement.plist.in
            MACOSX_BUNDLE_ICON_FILE appicon.icns
            )
    # First use macdeployqt to bundle the qt libs, but do NOT sign yet
    if (CODESIGN_CERTIFICATE_NAME)
        # Now codesign
        install(CODE "
            include(BundleUtilities)
            message(STATUS \"Fixup\")
            fixup_bundle(\"${APPS}.app\" \"\" \"\")
            message(STATUS \"Code-Signing bundle\")
            execute_process(COMMAND codesign --force --timestamp --options runtime --deep --entitlements \"${CMAKE_CURRENT_LIST_DIR}/Digital Stage Connector.entitlements\" --sign \"${CODESIGN_CERTIFICATE_NAME}\" \"${APPS}.app\")"
                COMPONENT ${target})
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
    set(CPACK_PACKAGE_FILE_NAME digital-stage-connector-app-macos-${CMAKE_SYSTEM_PROCESSOR})
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
    #set(CPACK_PACKAGE_FILE_NAME digital-stage-connector-app-linux-${CMAKE_SYSTEM_PROCESSOR})
endif ()
if (CMAKE_BUILD_TYPE_UPPER MATCHES "^ (DEBUG|RELWITHDEBINFO) $")
    set(CPACK_STRIP_FILES FALSE)
else ()
    set(CPACK_STRIP_FILES TRUE)
endif ()
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_ALL ${target})
include(CPack)
