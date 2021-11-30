cmake_minimum_required(VERSION 3.20)

project(DigitalStageConnectorService LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT WIN32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif ()
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
if (NOT TARGET DigitalStageConnectorCore)
    find_package(DigitalStageConnectorCore REQUIRED)
endif (NOT TARGET DigitalStageConnectorCore)
if (NOT TARGET DigitalStageAuth)
    find_package(DigitalStage REQUIRED)
endif (NOT TARGET DigitalStageAuth)
# DeviceId - Get unique device ID
if (NOT TARGET DeviceId)
    find_package(DeviceId REQUIRED)
endif (NOT TARGET DeviceId)
# UDP Discovery - find other devices in local network
if (NOT TARGET udp-discovery)
    find_package(udp-discovery REQUIRED)
endif (NOT TARGET udp-discovery)
# pLog -Logger
if (NOT TARGET plog)
    find_package(plog REQUIRED)
endif ()


#################################################
#
#   Configure artifact
#
#################################################
add_executable(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "digital-stage-client")
target_sources(${PROJECT_NAME}
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/auth_cli.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AuthIO.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/RemoteAuthService.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/RemoteAuthService.cpp
        )
target_include_directories(${PROJECT_NAME}
        PRIVATE
        ${udp-discovery_INCLUDE_DIR}
        )
target_link_libraries(${PROJECT_NAME}
        PRIVATE
        DigitalStageConnectorCore
        DeviceId::DeviceId
        udp-discovery
        DigitalStage::Auth
        )
target_compile_definitions(${PROJECT_NAME} PRIVATE SIGNUP_URL="${SIGNUP_URL}")
target_compile_definitions(${PROJECT_NAME} PRIVATE AUTH_URL="${AUTH_URL}")
target_compile_definitions(${PROJECT_NAME} PRIVATE API_URL="${API_URL}")
target_compile_definitions(${PROJECT_NAME} PRIVATE STAGE_URL="${STAGE_URL}")
target_compile_definitions(${PROJECT_NAME} PRIVATE SETTINGS_URL="${SETTINGS_URL}")
set_target_properties(${PROJECT_NAME} PROPERTIES
        OUTPUT_NAME "ds-connector")


#################################################
#
#   Installation
#
#################################################
include(GNUInstallDirs)
install(
        TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin COMPONENT Runtime
        LIBRARY DESTINATION lib COMPONENT Library
        ARCHIVE DESTINATION lib COMPONENT Library
        RESOURCE DESTINATION share COMPONENT Resource
)


#################################################
#
#   Pack
#
#################################################
set(CPACK_OUTPUT_CONFIG_FILE "${CMAKE_BINARY_DIR}/CPackConfigService.cmake")
set(CPACK_PACKAGE_NAME "Digital Stage Connector Service")
set(CPACK_PACKAGE_VENDOR "Tobias Hegemann")
set(CPACK_PACKAGE_DESCRIPTION "Background service to connect with Digital Stage via P2P")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Background service to connect with Digital Stage via P2P")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://www.digital-stage.org")
set(CPACK_PACKAGE_CONTACT "Tobias Hegemann <tobias.hegemann@digital-stage.org")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Digital Stage Connector Service")
set(CPACK_PACKAGE_ICON ${CMAKE_CURRENT_LIST_DIR}/assets/logo.png)
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_LIST_DIR}/../LICENSE)
set(CPACK_RESOURCE_FILE_README ${CMAKE_CURRENT_LIST_DIR}/../README.md)

set(CPACK_COMPONENTS_ALL Runtime Library Resource)

if (WIN32)
    # Windows
    set(CPACK_PACKAGE_FILE_NAME "Headless-Connector-${CMAKE_PROJECT_VERSION}-win64")
    include(InstallRequiredSystemLibraries)
    set(CPACK_GENERATOR "ZIP")
elseif (APPLE)
    # Apple
    set(CPACK_PACKAGE_FILE_NAME "Headless-Connector-${CMAKE_PROJECT_VERSION}-macOS")
    include(InstallRequiredSystemLibraries)
    set(CPACK_GENERATOR "ZIP")
elseif (UNIX)
    # Linux
    set(CPACK_PACKAGE_FILE_NAME "Headless-Connector-${CMAKE_PROJECT_VERSION}-linux")
    include(InstallRequiredSystemLibraries)
    set(CPACK_GENERATOR "ZIP")
endif ()

include(CPack)