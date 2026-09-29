# Share staging, audits, signing, runtime probes and archives with macOS CI.
find_package(Python3 3.11 QUIET COMPONENTS Interpreter)
set(BRICKSUITE_MAC_DEPLOY_QT_SOURCE "" CACHE PATH "Matching Qt source tree for macOS package notices")
set(BRICKSUITE_MAC_DEPLOY_OPENSSL_SOURCE "" CACHE PATH "Matching OpenSSL source tree for macOS package notices")
get_filename_component(_bricksuite_mac_qt "${Qt6_DIR}/../../.." ABSOLUTE)
get_filename_component(_bricksuite_mac_ssl_lib "${OPENSSL_CRYPTO_LIBRARY}" DIRECTORY)
get_filename_component(_bricksuite_mac_ssl "${_bricksuite_mac_ssl_lib}/.." ABSOLUTE)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_target(Deploy
        COMMAND "${CMAKE_COMMAND}" "-DCONFIG=${CMAKE_BUILD_TYPE}"
            -P "${CMAKE_CURRENT_LIST_DIR}/RequireMacDeployRelease.cmake"
        VERBATIM)
elseif(NOT Python3_Interpreter_FOUND)
    add_custom_target(Deploy
        COMMAND "${CMAKE_COMMAND}" -DMISSING_PYTHON=ON
            -P "${CMAKE_CURRENT_LIST_DIR}/RequireMacDeployRelease.cmake"
        VERBATIM)
else()
    # A real CMake target supports Qt Creator's Make/Ninja generators without
    # parsing generator-specific linker commands. Never part of ALL or the ZIP.
    get_target_property(_bricksuite_mac_probe_sources BrickSuite SOURCES)
    list(REMOVE_ITEM _bricksuite_mac_probe_sources src/main.cpp)
    qt_add_executable(BrickSuitePackageProbe EXCLUDE_FROM_ALL
        deployment/macos/PackageProbe.cpp ${_bricksuite_mac_probe_sources})
    set_target_properties(BrickSuitePackageProbe PROPERTIES MACOSX_BUNDLE FALSE)
    foreach(_property LINK_LIBRARIES COMPILE_DEFINITIONS COMPILE_OPTIONS INCLUDE_DIRECTORIES)
        get_target_property(_value BrickSuite ${_property})
        if(_value)
            set_property(TARGET BrickSuitePackageProbe PROPERTY ${_property} "${_value}")
        endif()
    endforeach()
    target_include_directories(BrickSuitePackageProbe PRIVATE "${CMAKE_SOURCE_DIR}")
    bricksuite_stage_mcut_runtime(BrickSuitePackageProbe)
    add_custom_target(Deploy
        COMMAND "${CMAKE_COMMAND}" "-DCONFIG=$<CONFIG>"
            -P "${CMAKE_CURRENT_LIST_DIR}/RequireMacDeployRelease.cmake"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/deployment/macos/local_deploy.py"
            --build "${CMAKE_BINARY_DIR}" --binary-dir "$<TARGET_FILE_DIR:BrickSuiteMeshBooleanWorker>"
            --probe "$<TARGET_FILE:BrickSuitePackageProbe>"
            --qt "${_bricksuite_mac_qt}" --qt-version "${Qt6Core_VERSION}"
            --openssl "${_bricksuite_mac_ssl}" --openssl-version "${OPENSSL_VERSION}"
            --qt-source "${BRICKSUITE_MAC_DEPLOY_QT_SOURCE}"
            --openssl-source "${BRICKSUITE_MAC_DEPLOY_OPENSSL_SOURCE}"
            --lib3mf-source "${lib3mf_SOURCE_DIR}"
            --config "$<CONFIG>" --minimum "${CMAKE_OSX_DEPLOYMENT_TARGET}"
        DEPENDS BrickSuite BrickSuiteMeshBooleanWorker BrickSuitePackageProbe
        USES_TERMINAL VERBATIM
        COMMENT "Create a local macOS candidate in the build directory's deploy folder")
endif()
