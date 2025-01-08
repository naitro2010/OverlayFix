######## target
add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_${CMAKE_CXX_STANDARD})

find_path(CommonLibSSEPath "include/REL/Relocation.h"
                PATHS   "${CMAKE_SOURCE_DIR}/extern/CommonLibSSE"
                        "${CMAKE_SOURCE_DIR}/extern/CommonLibSSE-NG"
                        "${CMAKE_SOURCE_DIR}/external/CommonLibSSE"
                        "${CMAKE_SOURCE_DIR}/external/CommonLibSSE-NG" NO_DEFAULT_PATH)
find_path(CommonLibVRPath "include/REL/Relocation.h"
                PATHS   "${CMAKE_SOURCE_DIR}/extern/CommonLibVR"
                        "${CMAKE_SOURCE_DIR}/external/CommonLibVR" NO_DEFAULT_PATH)
if(EXISTS "${CommonLibSSEPath}")
        set(BUILD_TESTS OFF CACHE BOOL "Disable CommonLibSSE build tests")
        add_subdirectory("${CommonLibSSEPath}" CommonLibSSE EXCLUDE_FROM_ALL)
        target_link_libraries(${PROJECT_NAME} PRIVATE CommonLibSSE::CommonLibSSE)
        target_include_directories(${PROJECT_NAME} PRIVATE CommonLibSSE::CommonLibSSE)
elseif(EXISTS "${CommonLibVRPath}" AND ENABLE_SKYRIM_VR)
        set(BUILD_SKYRIMVR ON CACHE BOOL "Build VR")
        add_subdirectory("${CMAKE_SOURCE_DIR}/extern/CommonLibVR" CommonLibVR EXCLUDE_FROM_ALL)
        target_link_libraries(${PROJECT_NAME} PRIVATE CommonLibVR::CommonLibVR)
        target_include_directories(${PROJECT_NAME} PRIVATE CommonLibVR::CommonLibVR)
        target_compile_definitions(CommonLibVR PUBLIC SKYRIMVR)
else()
        ##use vcpkg
        find_package(CommonLibSSE REQUIRED)
        target_link_libraries(${PROJECT_NAME} PRIVATE CommonLibSSE::CommonLibSSE)
        target_include_directories(${PROJECT_NAME} PRIVATE CommonLibSSE::CommonLibSSE)
endif()

if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE /Zi)
        target_link_options(${PROJECT_NAME} PRIVATE "$<$<CONFIG:RELEASE>:/DEBUG:FULL;/INCREMENTAL:NO;/OPT:REF,ICF>")
endif()

target_include_directories(${PROJECT_NAME}
        PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/cmake>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/extern/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external/include>
        $<INSTALL_INTERFACE:src>)

target_include_directories(${PROJECT_NAME}
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/extern/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external/include>
        )

if(EXISTS "${CMAKE_SOURCE_DIR}/src/PCH.h")
    target_precompile_headers(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/src/PCH.h")
endif()