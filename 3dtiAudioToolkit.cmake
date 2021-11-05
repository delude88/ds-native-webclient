
find_package(Eigen3 REQUIRED)

add_library(3dtiAudioToolkit)
add_subdirectory(include/libsofa)
add_subdirectory(include/cereal)

file(GLOB_RECURSE TOOLKIT_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/include/3dti_AudioToolkit/3dti_ResourceManager/BRIR/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/3dti_AudioToolkit/3dti_ResourceManager/HRTF/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/3dti_AudioToolkit/3dti_ResourceManager/ILD/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/3dti_AudioToolkit/3dti_Toolkit/*.cpp
        )

target_sources(3dtiAudioToolkit
        PUBLIC
        ${TOOLKIT_SOURCES}
        )

target_include_directories(3dtiAudioToolkit
        PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include/3dti_AudioToolkit/3dti_ResourceManager"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/3dti_AudioToolkit/3dti_ResourceManager/third_party_libraries"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/3dti_AudioToolkit/3dti_Toolkit"
        ${EIGEN3_INCLUDE_DIR}
)
target_link_libraries(3dtiAudioToolkit
        PUBLIC
        cereal
        sofa
        ${Eigen_BINARY_DIR}
        )