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

#################################################
#
#   Environment configuration
#
#################################################
add_compile_definitions(SIGNUP_URL="https://dev.dstage.org/account/signup")
add_compile_definitions(AUTH_URL="https://auth.dstage.org")
add_compile_definitions(API_URL="wss://api.dstage.org")
add_compile_definitions(STAGE_URL="https://dev.dstage.org/stage")
add_compile_definitions(SETTINGS_URL="https://dev.dstage.org/devices")

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
#   Dependencies
#
#################################################
#   - openssl (for libds, here only fixing macOS paths)
if (APPLE)
    if (EXISTS "/usr/local/opt/openssl/")
        set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl/")
    else ()
        include("${PROJECT_SOURCE_DIR}/cmake/BrewResolver.cmake")
    endif ()
endif (APPLE)
#  - libds
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/libds EXCLUDE_FROM_ALL)
#  - libdeviceid
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/libdeviceid EXCLUDE_FROM_ALL)
#  - udp-discovery-cpp
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/udp-discovery-cpp EXCLUDE_FROM_ALL)
#  - plog
if (NOT TARGET plog)
    include(FetchContent)
    FetchContent_Declare(
            plog
            GIT_REPOSITORY https://github.com/SergiusTheBest/plog.git
    )
    FetchContent_MakeAvailable(plog)
endif ()
#  - QT6
find_package(Qt6 COMPONENTS Core Widgets LinguistTools REQUIRED)


#################################################
#
#   Configure artifact
#
#################################################
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/keychain EXCLUDE_FROM_ALL)
add_executable(${PROJECT_NAME}_app MACOSX_BUNDLE)
qt_create_translation(QM_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/i18n/DigitalStage_de.ts
        ${CMAKE_CURRENT_SOURCE_DIR}/i18n/DigitalStage_fr.ts)
set_source_files_properties(DigitalStage_de.ts DigitalStage_fr.ts
        PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/i18n")
qt_add_translations(${PROJECT_NAME}_app TS_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/i18n/DigitalStage_de.ts
        ${CMAKE_CURRENT_SOURCE_DIR}/i18n/DigitalStage_fr.ts)
set_target_properties(${PROJECT_NAME}_app PROPERTIES OUTPUT_NAME "Digital Stage Connector")
if (APPLE)
    set(APP_ICON ${CMAKE_CURRENT_SOURCE_DIR}/assets/appicon.icns)
    set_source_files_properties(${APP_ICON} PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
endif (APPLE)
set_property(TARGET ${PROJECT_NAME}_app PROPERTY AUTOMOC ON)
set_property(TARGET ${PROJECT_NAME}_app PROPERTY AUTORCC ON)
set_property(TARGET ${PROJECT_NAME}_app PROPERTY AUTOUIC ON)
target_sources(${PROJECT_NAME}_app
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
target_include_directories(${PROJECT_NAME}_app PRIVATE ${Qt6_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME}_app
        PRIVATE
        ${PROJECT_NAME}::${PROJECT_NAME}
        Qt6::Widgets
        keychain
        )
if (APPLE)
    enable_language(OBJC)
    find_library(CORE_FOUNDATION Foundation)
    find_library(AV_FOUNDATION AVFoundation)
    target_sources(${PROJECT_NAME}_app
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/app/utils/macos.h
            ${CMAKE_CURRENT_SOURCE_DIR}/src/app/utils/macos_internal.mm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/app/utils/macos_internal.h
            )
    target_link_libraries(${PROJECT_NAME}_app
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
set_target_properties(${PROJECT_NAME}_app PROPERTIES
        BUNDLE True
        MACOSX_BUNDLE_GUI_IDENTIFIER de.tobiashegemann.digital-stage.app
        MACOSX_BUNDLE_BUNDLE_NAME App
        MACOSX_BUNDLE_COPYRIGHT MIT
        MACOSX_BUNDLE_BUNDLE_VERSION ${CMAKE_PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${CMAKE_PROJECT_VERSION}
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in
        MACOSX_BUNDLE_ICON_FILE appicon.icns
        )


if (CMAKE_BUILD_TYPE MATCHES Release)
    if (APPLE)
        find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${_qt_bin_dir}")
        # First use macdeployqt to bundle the qt libs, but do NOT sign yet
        add_custom_command(TARGET ${PROJECT_NAME}_app POST_BUILD
                COMMAND "${MACDEPLOYQT_EXECUTABLE}"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}_app>/../.."
                -appstore-compliant
                -always-overwrite
                COMMENT "Running macdeployqt..."
                )
        add_custom_command(TARGET ${PROJECT_NAME}_app POST_BUILD
                COMMAND
                ${CMAKE_INSTALL_NAME_TOOL}
                -change
                "@rpath/libkeychain.dylib"
                "@executable_path/../Frameworks/libkeychain.dylib"
                "$<TARGET_FILE:${PROJECT_NAME}_app>"
                COMMENT "Fixing libkeychain rpath"
                VERBATIM
                )
        if (CODESIGN_CERTIFICATE_NAME)
            message(STATUS "Codesinging using ${CODESIGN_CERTIFICATE_NAME}")
            set(ENV{CODESIGN_ALLOCATE} /Applications/Xcode.app/Contents/Developer/usr/bin/codesign_allocate)
            add_custom_command(TARGET ${PROJECT_NAME}_app POST_BUILD
                    COMMAND
                    codesign
                    --force
                    --deep
                    --sign "${CODESIGN_CERTIFICATE_NAME}"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}_app>/../.."
                    COMMENT "Signing app bundle using certificate ${CODESIGN_CERTIFICATE_NAME}..."
                    VERBATIM
                    )
            #[[add_custom_command(TARGET ${PROJECT_NAME}-cli POST_BUILD
                    COMMAND
                    codesign
                    --force
                    --deep
                    --sign "${CODESIGN_CERTIFICATE_NAME}"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}-cli>"
                    COMMENT "Signing app bundle using certificate ${CODESIGN_CERTIFICATE_NAME}..."
                    VERBATIM
                    )]]
        endif (CODESIGN_CERTIFICATE_NAME)
    endif (APPLE)


    #################################################
    #
    #   Pack
    #
    #################################################
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://www.digital-stage.org")
    set(CPACK_PACKAGE_CONTACT "Tobias Hegemann <tobias.hegemann@digital-stage.org")

    # DragNDrop
    set(CPACK_DMG_VOLUME_NAME "DigitalStageInstaller")
    set(CPACK_DMG_DS_STORE_SETUP_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/Packaging_DMGSetup.scpt)
    set(CPACK_DMG_BACKGROUND_IMAGE ${CMAKE_CURRENT_LIST_DIR}/assets/DMGBackground.tif)
    set(CPACK_SOURCE_STRIP_FILES "")


    if (UNIX)
        if (APPLE)
            # macOS
            set(CPACK_GENERATOR "DragNDrop")
        else ()
            # Linux
            set(CPACK_GENERATOR "DEB")
        endif ()
    else ()
        # Win32
        set(CPACK_PACKAGE_NAME "ds-client")
        set(CPACK_GENERATOR "ZIP")
    endif ()
    include(CPack)
endif ()


