# Copyright Contributors to the Open Shading Language project.
# SPDX-License-Identifier: BSD-3-Clause

set(BSDL_SOURCE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(BSDL_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
set(BSDL_GENERATED_LUT_DIR "${BSDL_GENERATED_INCLUDE_DIR}/BSDL")
set(BSDL_GENERATED_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

set(BSDL_GENERATED_LUT_HEADERS
    "${BSDL_GENERATED_LUT_DIR}/SPI/microfacet_tools_luts.h"
    "${BSDL_GENERATED_LUT_DIR}/SPI/bsdf_clearcoat_luts.h"
    "${BSDL_GENERATED_LUT_DIR}/SPI/bsdf_dielectric_front_luts.h"
    "${BSDL_GENERATED_LUT_DIR}/SPI/bsdf_dielectric_back_luts.h"
    "${BSDL_GENERATED_LUT_DIR}/SPI/bsdf_thinlayer_luts.h"
    "${BSDL_GENERATED_LUT_DIR}/MTX/bsdf_contysheen_luts.h"
    "${BSDL_GENERATED_LUT_DIR}/MTX/bsdf_zeltnersheen_luts.h"
    "${BSDL_GENERATED_LUT_DIR}/MTX/bsdf_dielectric_reflfront_luts.h"
    "${BSDL_GENERATED_LUT_DIR}/MTX/bsdf_dielectric_bothfront_luts.h"
    "${BSDL_GENERATED_LUT_DIR}/MTX/bsdf_dielectric_bothback_luts.h"
)

add_library(bsdl INTERFACE)
add_library(BSDL::BSDL ALIAS bsdl)
set_property(TARGET bsdl PROPERTY EXPORT_NAME BSDL)

target_compile_features(bsdl INTERFACE cxx_std_17)
target_include_directories(bsdl
    INTERFACE
        $<BUILD_INTERFACE:${BSDL_SOURCE_INCLUDE_DIR}>
        $<BUILD_INTERFACE:${BSDL_GENERATED_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(bsdl INTERFACE Imath::Imath)

add_library(bsdl_bootstrap INTERFACE)
target_compile_features(bsdl_bootstrap INTERFACE cxx_std_17)
target_include_directories(bsdl_bootstrap
    INTERFACE
        "${BSDL_SOURCE_INCLUDE_DIR}"
        "${BSDL_GENERATED_INCLUDE_DIR}"
)
target_link_libraries(bsdl_bootstrap INTERFACE Imath::Imath)

find_package(Threads REQUIRED)

add_executable(bsdl_genluts src/genluts.cpp)
add_executable(BSDL::genluts ALIAS bsdl_genluts)
target_link_libraries(bsdl_genluts PRIVATE bsdl_bootstrap Threads::Threads)

add_custom_command(
    OUTPUT ${BSDL_GENERATED_LUT_HEADERS}
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "${BSDL_GENERATED_LUT_DIR}/SPI"
            "${BSDL_GENERATED_LUT_DIR}/MTX"
    COMMAND $<TARGET_FILE:bsdl_genluts> "${BSDL_GENERATED_LUT_DIR}"
    DEPENDS bsdl_genluts
    COMMENT "Generating BSDL lookup tables"
    VERBATIM
)
add_custom_target(bsdl_generate_luts ALL DEPENDS ${BSDL_GENERATED_LUT_HEADERS})
add_dependencies(bsdl bsdl_generate_luts)

set(BSDL_ACESCG_LUT_SOURCE "${BSDL_GENERATED_SOURCE_DIR}/jakobhanika_ACEScg.cpp")
add_executable(bsdl_jakobhanika_luts src/jakobhanika_luts.cpp)
add_executable(BSDL::jakobhanika_luts ALIAS bsdl_jakobhanika_luts)
target_link_libraries(bsdl_jakobhanika_luts PRIVATE Threads::Threads)

add_custom_command(
    OUTPUT "${BSDL_ACESCG_LUT_SOURCE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BSDL_GENERATED_SOURCE_DIR}"
    COMMAND $<TARGET_FILE:bsdl_jakobhanika_luts> 64 "${BSDL_ACESCG_LUT_SOURCE}" ACEScg
    DEPENDS bsdl_jakobhanika_luts
    COMMENT "Generating BSDL Jakob-Hanika ACEScg lookup table"
    VERBATIM
)
add_library(bsdl_spectral_luts STATIC "${BSDL_ACESCG_LUT_SOURCE}")
target_compile_features(bsdl_spectral_luts PRIVATE cxx_std_17)
target_include_directories(bsdl_spectral_luts PRIVATE "${BSDL_SOURCE_INCLUDE_DIR}")
target_link_libraries(bsdl INTERFACE bsdl_spectral_luts)

install(DIRECTORY "${BSDL_SOURCE_INCLUDE_DIR}/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY "${BSDL_GENERATED_LUT_DIR}/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/BSDL")
install(TARGETS bsdl bsdl_spectral_luts
        EXPORT BSDLTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(EXPORT BSDLTargets
        FILE BSDLTargets.cmake
        NAMESPACE BSDL::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/BSDL")
