cmake_minimum_required(VERSION 3.20)

project(DigitalStageConnectorCore LANGUAGES CXX)

#################################################
#
#   Common configuration
#
#################################################
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
#   Environment
#
#################################################
add_compile_definitions(USE_ONLY_NATIVE_DEVICES)
add_compile_definitions(USE_ONLY_WEBRTC_DEVICES)
option(USE_RT_AUDIO "Use RtAudio as audio engine" ON)


#################################################
#
#   Dependencies
#
#################################################
include(${CMAKE_CURRENT_LIST_DIR}/deps/cmrc/CMakeRC.cmake)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/3dti_AudioToolkit EXCLUDE_FROM_ALL)
find_package(Threads REQUIRED)
if (NOT TARGET DigitalStageApi)
    find_package(DigitalStage REQUIRED)
endif (NOT TARGET DigitalStageApi)
set(NO_WEBSOCKET ON CACHE BOOL "" FORCE)
set(NO_MEDIA ON CACHE BOOL "" FORCE)
set(NO_TESTS ON CACHE BOOL "" FORCE)
set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
if (UNIX AND NOT APPLE)
    set(USE_SYSTEM_SRTP ON CACHE BOOL "" FORCE)
    set(USE_GNUTLS OFF CACHE BOOL "" FORCE)
endif ()
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/libdatachannel EXCLUDE_FROM_ALL)

if (NOT TARGET plog)
    find_package(plog REQUIRED)
endif ()


#################################################
#
#   Configure artifact
#
#################################################
add_library(${PROJECT_NAME} STATIC)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

include(TestBigEndian)
TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
if (IS_BIG_ENDIAN)
    target_compile_definitions(${PROJECT_NAME} PUBLIC DS_BIG_ENDIAN)
    message(STATUS "DS_BIG_ENDIAN")
else ()
    target_compile_definitions(${PROJECT_NAME} PUBLIC DS_LITTLE_ENDIAN)
    message(STATUS "DS_LITTLE_ENDIAN")
endif ()

target_include_directories(
        ${PROJECT_NAME}
        INTERFACE $<BUILD_INTERFACE:${${PROJECT_NAME}_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_sources(${PROJECT_NAME}
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Client.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Client.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/CMRCFileBuffer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/conversion.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/cp1252_to_utf8.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/ServiceDiscovery.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/RingBuffer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/AudioIO.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/AudioIO.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/AudioRenderer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/AudioRenderer.tpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/webrtc/ConnectionService.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/webrtc/ConnectionService.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/webrtc/PeerConnection.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/webrtc/PeerConnection.cpp
        )
if (USE_RT_AUDIO)
    message(STATUS "Using RtAudio as audio engine")
    set(RTAUDIO_BUILD_STATIC_LIBS ON CACHE BOOL "Enabling static build for RtAudio" FORCE)
    if (APPLE)
        set(RTAUDIO_API_JACK OFF CACHE BOOL "Disabling Jack on MacOS" FORCE)
        set(RTAUDIO_API_PULSE OFF CACHE BOOL "Disabling Pulse on MacOS" FORCE)
    endif ()
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/rtaudio EXCLUDE_FROM_ALL)
    target_sources(${PROJECT_NAME}
            PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/RtAudioIO.h
            ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/RtAudioIO.cpp
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
            ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/miniaudio.h
            ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/MiniAudioIO.h
            ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/MiniAudioIO.cpp
            )
    target_include_directories(${PROJECT_NAME}
            PUBLIC
            "${CMAKE_CURRENT_LIST_DIR}/deps/miniaudio"
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
target_link_libraries(${PROJECT_NAME}
        PUBLIC
        DigitalStage::ApiStatic
        3dtiAudioToolkitStatic
        plog::plog
        Threads::Threads
        LibDataChannel::LibDataChannelStatic
        ${PROJECT_NAME}-resources
        )


#################################################
#
#   Installation
#
#################################################
#export(
#        TARGETS ${PROJECT_NAME} 3dtiAudioToolkitStatic datachannel-static ${PROJECT_NAME}-resources usrsctp juice-static cmrc-base
#        NAMESPACE ${PROJECT_NAME}::
#        FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake")