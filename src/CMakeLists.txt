# Copyright (c) 2026 Bitdancer (github.com/RealBitdancer).
# SPDX-License-Identifier: MIT

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(PUBLIC_INCLUDE_DIR ${opal_SOURCE_DIR}/include)

add_library(${PROJECT_NAME} STATIC)

target_sources(${PROJECT_NAME}
    PRIVATE
        opal.c
)

target_sources(${PROJECT_NAME}
    PUBLIC FILE_SET HEADERS
    BASE_DIRS ${PUBLIC_INCLUDE_DIR}
    FILES ${PUBLIC_INCLUDE_DIR}/${PROJECT_NAME}/opal.h
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    FOLDER ${PROJECT_NAME}
    VERSION ${PROJECT_VERSION}
)

target_compile_features(${PROJECT_NAME} PRIVATE c_std_11)

install(TARGETS ${PROJECT_NAME}
    EXPORT opalTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT opalTargets
    FILE opalTargets.cmake
    NAMESPACE opal::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/opal
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/opalConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/opalConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/opalConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/opal
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/opalConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/opalConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/opal
)

install(FILES
    ${opal_SOURCE_DIR}/LICENSE
    ${opal_SOURCE_DIR}/PUBLIC_DOMAIN_PROOF.txt
    ${opal_SOURCE_DIR}/THIRD_PARTY_LICENSES.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/opal
)
