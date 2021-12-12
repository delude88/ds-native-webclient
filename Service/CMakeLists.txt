cmake_minimum_required(VERSION 3.20)

project(service LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT WIN32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif ()
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_LIST_DIR}/../cmake/modules)


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
        #${CMAKE_CURRENT_SOURCE_DIR}/src/RemoteAuthService.h
        #${CMAKE_CURRENT_SOURCE_DIR}/src/RemoteAuthService.cpp
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
        DigitalStage::AuthStatic
        )
target_compile_definitions(${PROJECT_NAME} PRIVATE SIGNUP_URL="${SIGNUP_URL}")
target_compile_definitions(${PROJECT_NAME} PRIVATE AUTH_URL="${AUTH_URL}")
target_compile_definitions(${PROJECT_NAME} PRIVATE API_URL="${API_URL}")
target_compile_definitions(${PROJECT_NAME} PRIVATE STAGE_URL="${STAGE_URL}")
target_compile_definitions(${PROJECT_NAME} PRIVATE SETTINGS_URL="${SETTINGS_URL}")
set_target_properties(${PROJECT_NAME} PROPERTIES
        OUTPUT_NAME "ds-connector"
        )


#################################################
#
#   Installation
#
#################################################
include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
        COMPONENT ${PROJECT_NAME}
        )

#################################################
#
#   Pack
#
#################################################
set(CPACK_OUTPUT_CONFIG_FILE "${CMAKE_BINARY_DIR}/CPackConfigService.cmake")
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
if (WIN32)
    # Windows - unsupported right now
    install(TARGETS ${PROJECT_NAME}
            RUNTIME DESTINATION . COMPONENT ${PROJECT_NAME}
            )
    set(CPACK_PACKAGE_FILE_NAME digital-stage-connector-service-win-${CMAKE_SYSTEM_PROCESSOR})
    include(InstallRequiredSystemLibraries)
    set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
    set(CPACK_GENERATOR "ZIP")
elseif (APPLE)
    # Apple
    include(InstallRequiredSystemLibraries)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../HOWTO.md ${CMAKE_CURRENT_BINARY_DIR}/ReadMe.txt COPYONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE ${CMAKE_CURRENT_BINARY_DIR}/License.txt COPYONLY)
    set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_BINARY_DIR}/License.txt)
    set(CPACK_RESOURCE_FILE_README ${CMAKE_CURRENT_BINARY_DIR}/ReadMe.txt)
    if (CODESIGN_CERTIFICATE_NAME)
        install(CODE "
            message(STATUS \"Code-Signing binary\")
            execute_process(COMMAND codesign --options runtime --sign \"${CODESIGN_CERTIFICATE_NAME}\" \"$<TARGET_FILE:${PROJECT_NAME}>\")"
                COMPONENT ${PROJECT_NAME})
    endif (CODESIGN_CERTIFICATE_NAME)
    set(CPACK_PACKAGE_FILE_NAME digital-stage-connector-service-macos-${CMAKE_SYSTEM_PROCESSOR})
    set(CPACK_PACKAGING_INSTALL_PREFIX /usr/local)
    set(CPACK_GENERATOR "productbuild")
elseif (UNIX)
    # Linux
    # Debian
    set(CPACK_PACKAGE_CONTACT "tobias.hegemann@digital-stage.org")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Tobias Hegemann <${CPACK_PACKAGE_CONTACT}>")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${PROJECT_HOMEPAGE_URL}")
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
    #set(CPACK_PACKAGE_FILE_NAME digital-stage-connector-service-linux-${CMAKE_SYSTEM_PROCESSOR})
endif ()
if (CMAKE_BUILD_TYPE_UPPER MATCHES "^(DEBUG|RELWITHDEBINFO)$")
    set(CPACK_STRIP_FILES FALSE)
else ()
    set(CPACK_STRIP_FILES TRUE)
endif ()
set(CPACK_COMPONENTS_ALL ${PROJECT_NAME})
include(CPack)