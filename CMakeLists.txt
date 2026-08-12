# Copyright Contributors to the Open Shading Language project.
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.20)

project(BSDL VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json")
    file(CREATE_LINK
        "${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json"
        SYMBOLIC
    )
endif()

include(GNUInstallDirs)
include(CTest)
include(CMakePackageConfigHelpers)

find_package(Imath CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

add_subdirectory(core)
add_subdirectory(bsdltool)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/BSDLConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/BSDLConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/BSDL"
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/BSDLConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/BSDLConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/BSDLConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/BSDL"
)

if(BUILD_TESTING)
    add_subdirectory(testsuite)
endif()
