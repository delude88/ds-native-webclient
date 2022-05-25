cmake_minimum_required(VERSION 3.4)

project(SwiftApp LANGUAGES C Swift)
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_LIST_DIR}/../cmake/modules)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

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

add_executable(SwiftApp MACOSX_BUNDLE)
target_sources(SwiftApp
        PRIVATE
        appApp.swift
        ContentView.swift
        Assets.xcassets)
set_source_files_properties(Assets.xcassets PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources
        )
target_include_directories(SwiftApp
        PRIVATE
        "Preview Content")
target_compile_options(SwiftApp PRIVATE "$<$<COMPILE_LANGUAGE:C>:-Werror=objc-method-access>")