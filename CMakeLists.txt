cmake_minimum_required(VERSION 3.20)

project(client LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT WIN32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif ()
set(CMAKE_PROJECT_VERSION "1.0.0")
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_LIST_DIR}/cmake/modules)

#################################################
#
#   Environment configuration
#
#################################################
add_compile_definitions(SIGNUP_URL="https://beta.digital-stage.org/account/signup")
add_compile_definitions(AUTH_URL="https://auth.dstage.org")
add_compile_definitions(API_URL="wss://api.dstage.org")
option(WITH_GUI "Build with graphical user interface" OFF)
option(USE_RT_AUDIO "Use RtAudio as audio engine" ON)
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
include(include/cmrc/CMakeRC.cmake)
#  - posix or win32 threads
find_package(Threads REQUIRED)
#  - libds
add_subdirectory(include/libds)
#  - libdatachannel
if (UNIX AND NOT APPLE)
    set(USE_SYSTEM_SRTP ON)
    set(USE_GNUTLS OFF)
endif ()
add_subdirectory(include/libdatachannel)
#  - libdeviceid
add_subdirectory(include/libdeviceid)
#  - udp-discovery-cpp
add_subdirectory(include/udp-discovery-cpp)
#  - HRTF Engine
#add_compile_definitions(USE_UPC_WITHOUT_MEMORY)
add_subdirectory(include/3dti_AudioToolkit)
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
    set(RTAUDIO_API_JACK OFF)
    set(RTAUDIO_API_PULSE OFF)  # WHY?!?
endif ()
add_subdirectory(include/rtaudio)


#################################################
#
#   Configure artifact - common lib
#
#################################################
add_library(${PROJECT_NAME} src/gui/KeyStore.cpp src/gui/KeyStore.h)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_sources(${PROJECT_NAME}
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Client.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Client.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/auth/auth_cli.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/auth/AuthIO.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/auth/RemoteAuthService.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/auth/RemoteAuthService.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/CMRCFileBuffer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/conversion.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/cp1252_to_utf8.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/ServiceDiscovery.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/AudioIO.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/AudioIO.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/AudioRenderer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/AudioRenderer.tpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/AudioMixer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/AudioMixer.tpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/RingBuffer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/webrtc/ConnectionService.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/webrtc/ConnectionService.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/webrtc/PeerConnection.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/webrtc/PeerConnection.cpp
        )
if (USE_RT_AUDIO)
    message(STATUS "Using RtAudio as audio engine")
    target_sources(${PROJECT_NAME}
            PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/RtAudioIO.h
            ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/RtAudioIO.cpp
            )
    target_compile_definitions(${PROJECT_NAME}
            PUBLIC
            USE_RT_AUDIO)
    target_link_libraries(${PROJECT_NAME}
            PRIVATE
            rtaudio
            )
else ()
    message(STATUS "Using MiniAudio as audio engine")
    target_sources(${PROJECT_NAME}
            PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/miniaudio.h
            ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/MiniAudioIO.h
            ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/MiniAudioIO.cpp
            )
    target_include_directories(${PROJECT_NAME}
            PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/include/miniaudio"
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
        "${CMAKE_CURRENT_SOURCE_DIR}/include/udp-discovery-cpp"
        )
target_link_libraries(${PROJECT_NAME}
        PUBLIC
        DigitalStage::Api
        DigitalStage::Auth
        LibDataChannel::LibDataChannel
        DeviceId::DeviceId
        plog
        Threads::Threads
        udp-discovery
        3dtiAudioToolkit
        ${PROJECT_NAME}-resources
        )


#################################################
#
#   Configure artifact - CLI Application
#
#################################################
add_executable(${PROJECT_NAME}-cli)
target_sources(${PROJECT_NAME}-cli
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main_cli.cpp
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
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
find_package(Qt6 COMPONENTS Core Widgets LinguistTools REQUIRED)
add_subdirectory(include/keychain)
set(BUILD_WITH_QT6 ON)
if (APPLE)
    qt_add_executable(${PROJECT_NAME}-app MACOSX_BUNDLE)
else ()
    qt_add_executable(${PROJECT_NAME}-app)
endif ()
target_sources(${PROJECT_NAME}-app
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main_app.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/LoginDialog.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/LoginDialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/KeyStore.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/KeyStore.cpp
        )
target_link_libraries(${PROJECT_NAME}-app
        PRIVATE
        ${PROJECT_NAME}::${PROJECT_NAME}
        Qt6::Core
        Qt6::Widgets
        keychain
        )
if (APPLE)
    enable_language(OBJC)
    find_library(CORE_FOUNDATION Foundation)
    find_library(AV_FOUNDATION AVFoundation)
    target_sources(${PROJECT_NAME}-app
            PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/utils/macos.h
            ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/utils/macos_internal.mm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/utils/macos_internal.h
            )
    target_link_libraries(${PROJECT_NAME}-app
            PUBLIC
            ${CORE_FOUNDATION}
            ${AV_FOUNDATION}
            )
endif (APPLE)


#################################################
#
#   Bundling
#
#################################################
set_target_properties(${PROJECT_NAME}-app PROPERTIES
        BUNDLE True
        MACOSX_BUNDLE_GUI_IDENTIFIER de.tobiashegemann.digital-stage.app
        MACOSX_BUNDLE_BUNDLE_NAME App
        MACOSX_BUNDLE_COPYRIGHT MIT
        MACOSX_BUNDLE_BUNDLE_VERSION "0.1"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in
        )
set_target_properties(${PROJECT_NAME}-cli PROPERTIES
        BUNDLE True
        MACOSX_BUNDLE_GUI_IDENTIFIER de.tobiashegemann.digital-stage.cli
        MACOSX_BUNDLE_BUNDLE_NAME Client
        MACOSX_BUNDLE_COPYRIGHT MIT
        MACOSX_BUNDLE_BUNDLE_VERSION "0.1"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in
        )