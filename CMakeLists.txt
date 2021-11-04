cmake_minimum_required(VERSION 3.8)

project(client LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_PROJECT_VERSION "1.0.0")

# Environment configuration
add_compile_definitions(SIGNUP_URL="https://beta.digital-stage.org/account/signup")
add_compile_definitions(AUTH_URL="https://auth.dstage.org")
add_compile_definitions(API_URL="wss://api.dstage.org")

include(TestBigEndian)
TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
if (IS_BIG_ENDIAN)
    add_compile_definitions(BIG_ENDIAN)
    message(STATUS "BIG_ENDIAN")
else ()
    add_compile_definitions(LITTLE_ENDIAN)
    message(STATUS "LITTLE_ENDIAN")
endif ()

# Special macOS routines
if (APPLE)
    if (EXISTS "/usr/local/opt/openssl/")
        set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl/")
    else ()
        include("${PROJECT_SOURCE_DIR}/cmake/BrewResolver.cmake")
    endif ()
    set(MACOSX_DEPLOYMENT_TARGET "10.11")
endif (APPLE)

# Dependencies
#  - RingBuffer
add_subdirectory(include/ringbuffer)
#  - HashMap
#add_subdirectory(include/HashMap)
#  - SPSCQueue
#add_subdirectory(include/SPSCQueue)
find_package(Threads REQUIRED)
#  - libds
add_subdirectory(include/libds)
#  - libdatachannel
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


# Configure artifact
if (APPLE)
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE)
else ()
    add_executable(${PROJECT_NAME})
endif ()
target_sources(${PROJECT_NAME}
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/auth.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Client.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Client.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/conversion.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/miniaudio.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/ringbuffer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/thread.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/AudioIO.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/AudioIO.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/MiniAudioIO.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/MiniAudioIO.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/webrtc/ConnectionService.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/webrtc/ConnectionService.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/webrtc/PeerConnection.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/webrtc/PeerConnection.cpp
        )
target_include_directories(${PROJECT_NAME}
        PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include/miniaudio"
        )
target_link_libraries(${PROJECT_NAME}
        PRIVATE
        DigitalStage::Api
        DigitalStage::Auth
        LibDataChannel::LibDataChannel
        DeviceId::DeviceId
        plog
        #Ringbuffer::Ringbuffer
        Threads::Threads
        )
if (NOT WIN32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -mavx2 -msse2")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
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
    set_target_properties(${PROJECT_NAME} PROPERTIES
            BUNDLE True
            MACOSX_BUNDLE_GUI_IDENTIFIER org.digital-stage.libds.Client
            MACOSX_BUNDLE_BUNDLE_NAME Client
            MACOSX_BUNDLE_COPYRIGHT MIT
            MACOSX_BUNDLE_BUNDLE_VERSION "0.1"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in
            )
endif (APPLE)

#[[
#  - libwebrtc
set(PREFER_LOCAL_WEBRTC ON CACHE BOOL "Prefer local webrtc build instead of fetching prebuilt")
if (PREFER_LOCAL_WEBRTC AND EXISTS "${PROJECT_SOURCE_DIR}/include/libwebrtc/src/out/Default")
    set(LIBWEBRTC_IS_LOCAL TRUE)
    message(STATUS "Using local built libwebrtc")
    set(LIBWEBRTC_INCLUDE_PATH "${PROJECT_SOURCE_DIR}/include/libwebrtc/src")
    set(LIBWEBRTC_BINARY_PATH "${PROJECT_SOURCE_DIR}/include/libwebrtc/src/out/Default/obj")
else ()
    set(LIBWEBRTC_IS_LOCAL FALSE)
    message(STATUS "Using pre-built libwebrtc")
    include("${PROJECT_SOURCE_DIR}/cmake/FetchWebRTC.cmake")
endif ()
message(STATUS "libwebrtc headers: ${LIBWEBRTC_INCLUDE_PATH}")
message(STATUS "libwebrtc binaries: ${LIBWEBRTC_BINARY_PATH}")
target_include_directories(${PROJECT_NAME}
        PUBLIC
        "${LIBWEBRTC_INCLUDE_PATH}"
        "${LIBWEBRTC_INCLUDE_PATH}/third_party/abseil-cpp"
        )
if (LIBWEBRTC_IS_LOCAL)
    target_link_libraries(${PROJECT_NAME}
            PRIVATE
            "${LIBWEBRTC_BINARY_PATH}/libwebrtc${CMAKE_STATIC_LIBRARY_SUFFIX}"
            )
else ()
    target_link_libraries(${PROJECT_NAME}
            PRIVATE
            "${LIBWEBRTC_BINARY_PATH}/libwebrtc_full${CMAKE_STATIC_LIBRARY_SUFFIX}"
            )
endif ()
# Compile definitions for libwebrtc.
target_compile_definitions(${PROJECT_NAME} PUBLIC
        $<$<NOT:$<PLATFORM_ID:Windows>>:WEBRTC_POSIX>
        $<$<PLATFORM_ID:Windows>:WEBRTC_WIN>
        $<$<PLATFORM_ID:Darwin>:WEBRTC_MAC>
        _TURN_OFF_PLATFORM_STRING
        _NO_ASYNCRTIMP
        )
]]