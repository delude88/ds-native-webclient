cmake_minimum_required(VERSION 3.8)

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
#  - plog
if (NOT TARGET plog)
    FetchContent_Declare(
            plog
            GIT_REPOSITORY https://github.com/SergiusTheBest/plog.git
            GIT_TAG 5b38cdf433b40986bdead473fcaad5a63ca6e32e # release-1.1.5-MIT
    )
    FetchContent_MakeAvailable(plog)
endif ()
#  - udp-discovery-cpp
add_subdirectory(include/udp-discovery-cpp)
#  - HRTF Engine
#add_compile_definitions(USE_UPC_WITHOUT_MEMORY)
add_subdirectory(include/3dti_AudioToolkit)
#  - rtaudio
if (APPLE)
    #find_package(Jack)
    set(RTAUDIO_API_JACK OFF)
    set(RTAUDIO_API_PULSE OFF)  # WHY?!?
endif ()
add_subdirectory(include/rtaudio)


#################################################
#
#   Configure artifact
#
#################################################
if (APPLE)
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE)
else ()
    add_executable(${PROJECT_NAME})
endif ()
target_sources(${PROJECT_NAME}
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Client.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Client.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/auth/auth_cli.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/auth/AuthIO.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/auth/RemoteAuthService.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/auth/RemoteAuthService.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/CMRCFileBuffer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/conversion.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/cp1252_to_utf8.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/miniaudio.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/ServiceDiscovery.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/AudioIO.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/AudioIO.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/MiniAudioIO.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/MiniAudioIO.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/RtAudioIO.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/RtAudioIO.cpp
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
    target_compile_definitions(${PROJECT_NAME}
            PRIVATE
            USE_RT_AUDIO)
endif ()


#################################################
#
#   Resource managment
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
        "${CMAKE_CURRENT_SOURCE_DIR}/include/miniaudio"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/udp-discovery-cpp"
        )
target_link_libraries(${PROJECT_NAME}
        PRIVATE
        DigitalStage::Api
        DigitalStage::Auth
        LibDataChannel::LibDataChannel
        DeviceId::DeviceId
        plog
        Threads::Threads
        udp-discovery
        3dtiAudioToolkit
        rtaudio
        ${PROJECT_NAME}-resources
        )
if (WITH_GUI)
    message(STATUS "Building with GUI")
    find_package(wxWidgets REQUIRED COMPONENTS core base)
    include(${wxWidgets_USE_FILE})
    target_compile_definitions(${PROJECT_NAME}
            PRIVATE
            WITH_GUI)
    target_sources(${PROJECT_NAME}
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/main_app.cpp
            )
    target_link_libraries(${PROJECT_NAME}
            PRIVATE
            ${wxWidgets_LIBRARIES})
else ()
    message(STATUS "Building headless")
    target_sources(${PROJECT_NAME}
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/main_headless.cpp
            )
endif ()
# Additions for macOS
if (APPLE)
    find_library(APPLICATION_SERVICES ApplicationServices)
    find_library(AUDIO_TOOLBOX AudioToolbox)
    find_library(AV_FOUNDATION AVFoundation)
    find_library(CORE_AUDIO CoreAudio)
    find_library(CORE_FOUNDATION Foundation)
    find_library(CORE_MEDIA CoreMedia)
    find_library(CORE_VIDEO CoreVideo)
    find_library(CORE_SERVICES CoreServices)

    enable_language(OBJC)
    target_sources(${PROJECT_NAME}
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/macos.h
            ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/macos_internal.mm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/macos_internal.h
            )
    target_link_libraries(${PROJECT_NAME} PUBLIC
            ${APPLICATION_SERVICES}
            ${AUDIO_TOOLBOX}
            ${AV_FOUNDATION}
            ${CORE_AUDIO}
            ${CORE_FOUNDATION}
            ${CORE_MEDIA}
            ${CORE_VIDEO}
            ${CORE_SERVICES}
            )
endif (APPLE)


#################################################
#
#   Bundling
#
#################################################
set_target_properties(${PROJECT_NAME} PROPERTIES
        BUNDLE True
        MACOSX_BUNDLE_GUI_IDENTIFIER org.digital-stage.libds.Client
        MACOSX_BUNDLE_BUNDLE_NAME Client
        MACOSX_BUNDLE_COPYRIGHT MIT
        MACOSX_BUNDLE_BUNDLE_VERSION "0.1"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in
        RESOURCE "${RESOURCE_FILES}"
        )