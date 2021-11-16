cmake_minimum_required(VERSION 3.20)

project(client LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT WIN32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif ()
set(CMAKE_PROJECT_VERSION "0.0.1")
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_LIST_DIR}/cmake/modules)

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
add_compile_definitions(USE_ONLY_NATIVE_DEVICES)
option(USE_RT_AUDIO "Use RtAudio as audio engine" ON)
option(GENERATE_TS "Update and generate language files (requires QT6::LinguistTools)" OFF)

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
#  - resource compiling
include(${CMAKE_CURRENT_LIST_DIR}/deps/cmrc/CMakeRC.cmake)
#  - posix or win32 threads
find_package(Threads REQUIRED)
#  - libds
set(TECKOS_BUILD_SOURCES OFF CACHE BOOL "Using only local sources for all dependencies")
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/libds EXCLUDE_FROM_ALL)
#  - libdatachannel
if (UNIX AND NOT APPLE)
    set(USE_SYSTEM_SRTP ON)
    set(USE_GNUTLS OFF)
endif ()
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/libdatachannel EXCLUDE_FROM_ALL)
#  - libdeviceid
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/libdeviceid EXCLUDE_FROM_ALL)
#  - udp-discovery-cpp
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/udp-discovery-cpp EXCLUDE_FROM_ALL)
#  - HRTF Engine
#add_compile_definitions(USE_UPC_WITHOUT_MEMORY)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/3dti_AudioToolkit EXCLUDE_FROM_ALL)
#  - plog
if (NOT TARGET plog)
    include(FetchContent)
    FetchContent_Declare(
            plog
            GIT_REPOSITORY https://github.com/SergiusTheBest/plog.git
    )
    FetchContent_MakeAvailable(plog)
endif ()
#  - rtaudio
if (APPLE)
    #find_package(Jack)
    set(RTAUDIO_BUILD_STATIC_LIBS ON CACHE BOOL "Enabling static build for RtAudio" FORCE)
    set(RTAUDIO_API_JACK OFF CACHE BOOL "Disabling Jack on MacOS" FORCE)
    set(RTAUDIO_API_PULSE OFF CACHE BOOL "Disabling Pulse on MacOS" FORCE)
endif ()
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/rtaudio EXCLUDE_FROM_ALL)
#  - oneTBB
#add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/oneTBB EXCLUDE_FROM_ALL)


#################################################
#
#   Configure artifact - common lib
#
#################################################
add_library(${PROJECT_NAME} STATIC EXCLUDE_FROM_ALL)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_include_directories(
        ${PROJECT_NAME}
        INTERFACE $<BUILD_INTERFACE:${${PROJECT_NAME}_SOURCE_DIR}/src/core>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_sources(${PROJECT_NAME}
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Client.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Client.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/utils/CMRCFileBuffer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/utils/conversion.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/utils/cp1252_to_utf8.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/utils/ServiceDiscovery.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/utils/RingBuffer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/audio/AudioIO.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/audio/AudioIO.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/audio/AudioRenderer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/audio/AudioRenderer.tpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/audio/AudioMixer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/audio/AudioMixer.tpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/webrtc/ConnectionService.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/webrtc/ConnectionService.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/webrtc/PeerConnection.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/webrtc/PeerConnection.cpp
        )
if (USE_RT_AUDIO)
    message(STATUS "Using RtAudio as audio engine")
    target_sources(${PROJECT_NAME}
            PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src/core/audio/RtAudioIO.h
            ${CMAKE_CURRENT_SOURCE_DIR}/src/core/audio/RtAudioIO.cpp
            )
    target_compile_definitions(${PROJECT_NAME}
            PUBLIC
            USE_RT_AUDIO)
    target_link_libraries(${PROJECT_NAME}
            PUBLIC
            rtaudio
            )
else ()
    message(STATUS "Using MiniAudio as audio engine")
    target_sources(${PROJECT_NAME}
            PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src/core/utils/miniaudio.h
            ${CMAKE_CURRENT_SOURCE_DIR}/src/core/audio/MiniAudioIO.h
            ${CMAKE_CURRENT_SOURCE_DIR}/src/core/audio/MiniAudioIO.cpp
            )
    target_include_directories(${PROJECT_NAME}
            PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/${CMAKE_CURRENT_LIST_DIR}/deps/miniaudio"
            )
endif ()

#################################################
#
#   Resource management
#
#################################################
file(GLOB RESOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/*.3dti-brir
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/*.3dti-hrtf
        )
cmrc_add_resource_library(${PROJECT_NAME}-resources
        NAMESPACE clientres
        WHENCE resources
        ${RESOURCE_FILES}
        )


#################################################
#
#   Inclusions & Linking
#
#################################################
target_include_directories(${PROJECT_NAME}
        PUBLIC
        "/${CMAKE_CURRENT_LIST_DIR}/deps/udp-discovery-cpp"
        )
target_link_libraries(${PROJECT_NAME}
        PUBLIC
        DigitalStage::ApiStatic
        DigitalStage::AuthStatic
        DeviceId::DeviceId
        #TBB::tbb
        Threads::Threads
        udp-discovery
        3dtiAudioToolkitStatic
        LibDataChannel::LibDataChannelStatic
        ${PROJECT_NAME}-resources
        plog
        )


#################################################
#
#   Configure artifact - CLI Application
#
#################################################
add_executable(${PROJECT_NAME}-cli)
set_target_properties(${PROJECT_NAME}-cli PROPERTIES OUTPUT_NAME "digital-stage-client")
target_sources(${PROJECT_NAME}-cli
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main_cli.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cli/auth_cli.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cli/AuthIO.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cli/RemoteAuthService.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cli/RemoteAuthService.cpp
        )
target_link_libraries(${PROJECT_NAME}-cli
        PRIVATE
        ${PROJECT_NAME}::${PROJECT_NAME}
        )


#################################################
#
#   Configure artifact - GUI Application
#
#################################################
find_package(Qt6 COMPONENTS Core Widgets LinguistTools REQUIRED)
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
    set(APP_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/appicon.icns)
    set_source_files_properties(${APP_ICON} PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
endif (APPLE)
set_property(TARGET ${PROJECT_NAME}_app PROPERTY AUTOMOC ON)
set_property(TARGET ${PROJECT_NAME}_app PROPERTY AUTORCC ON)
set_property(TARGET ${PROJECT_NAME}_app PROPERTY AUTOUIC ON)
target_sources(${PROJECT_NAME}_app
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main_app.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app/App.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app/App.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app/TrayIcon.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app/TrayIcon.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app/LoginDialog.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app/LoginDialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app/LoginDialog.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app/KeyStore.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app/KeyStore.cpp
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
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://www.digital-stage.org")
    set(CPACK_PACKAGE_CONTACT "Tobias Hegemann <tobias.hegemann@digital-stage.org")

    if (APPLE)
        #################################################
        #
        #   Apple: Fix @rpaths, codesigning and packing
        #
        #################################################
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
            set(ENV{CODESIGN_ALLOCATE} /Applications/Xcode.app/Contents/Developer/usr/bin/codesign_allocate)
            add_custom_command(TARGET ${PROJECT_NAME}_app POST_BUILD
                    COMMAND
                    codesign
                    --force
                    --deep
                    --sign "${CODESIGN_CERTIFICATE_NAME}"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}_app>/../.."
                    COMMENT "Signing app bundle using certificate ${M_SIGN_CERT_NAME}..."
                    VERBATIM
                    )
            add_custom_command(TARGET ${PROJECT_NAME}-cli POST_BUILD
                    COMMAND
                    codesign
                    --force
                    --deep
                    --sign "${CODESIGN_CERTIFICATE_NAME}"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}-cli>"
                    COMMENT "Signing app bundle using certificate ${APPLE_CERT}..."
                    VERBATIM
                    )
        endif (CODESIGN_CERTIFICATE_NAME)
        set(CPACK_GENERATOR "DragNDrop")
        set(CPACK_DMG_VOLUME_NAME "DigitalStageInstaller")
        set(CPACK_DMG_DS_STORE_SETUP_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/cmake/Packaging_DMGSetup.scpt)
        set(CPACK_DMG_BACKGROUND_IMAGE ${CMAKE_CURRENT_LIST_DIR}/resources/DMGBackground.tif)
        set(CPACK_SOURCE_STRIP_FILES "")
    elseif (WIN32)
        #################################################
        #
        #   Windows: Packing
        #
        #################################################
        set(CPACK_GENERATOR "ZIP")
    else ()
        #################################################
        #
        #   Linux: Packing
        #
        #################################################
        set(CPACK_PACKAGE_NAME "ds-client")
        set(CPACK_GENERATOR "DEB")
    endif ()
endif ()
