include(FetchContent)

SET(WEBRTC_BRANCH "4472")
SET(WEBRTC_REVISION "33644-92ba70c")
SET(WEBRTC_TAG "${WEBRTC_BRANCH}-${WEBRTC_REVISION}")
if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    SET(OS "mac")
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    SET(OS "android")
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    SET(OS "windows")
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "iOS")
    SET(OS "ios")
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "watchOS")
    SET(OS "ios")
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "tvOS")
    SET(OS "ios")
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    SET(OS "linux")
endif ()
SET(ARCH ${CMAKE_SYSTEM_PROCESSOR})
if (ARCH STREQUAL "x86_64")
    SET(ARCH "x64")
endif ()
SET(URL "https://github.com/godotengine/webrtc-actions/releases/download/${WEBRTC_TAG}/webrtc-${WEBRTC_REVISION}-${OS}-${ARCH}.tar.gz")
FetchContent_Declare(
        libwebrtc
        URL ${URL}
        SOURCE_SUBDIR include
)
FetchContent_MakeAvailable(libwebrtc)
set(LIBWEBRTC_INCLUDE_PATH "${libwebrtc_SOURCE_DIR}/include")
set(LIBWEBRTC_BINARY_PATH "${libwebrtc_SOURCE_DIR}/lib/${ARCH}/${CMAKE_BUILD_TYPE}")