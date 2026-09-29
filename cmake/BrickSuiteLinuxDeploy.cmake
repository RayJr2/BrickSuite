# Qt Creator lists custom CMake targets in its Build Steps target selector.
find_package(Python3 QUIET COMPONENTS Interpreter)
set(BRICKSUITE_LINUX_DEPLOY_SOURCES "" CACHE PATH
    "Optional cache of matching Qt/ICU license sources for local Linux Deploy")
mark_as_advanced(BRICKSUITE_LINUX_DEPLOY_SOURCES)
get_filename_component(_bricksuite_deploy_qt "${Qt6_DIR}/../../.." ABSOLUTE)
set(_bricksuite_deploy_options)
if(BRICKSUITE_LINUX_DEPLOY_SOURCES)
    list(APPEND _bricksuite_deploy_options --sources "${BRICKSUITE_LINUX_DEPLOY_SOURCES}")
endif()
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_target(Deploy
        COMMAND "${CMAKE_COMMAND}" "-DCONFIG=${CMAKE_BUILD_TYPE}"
            -P "${CMAKE_CURRENT_LIST_DIR}/RequireLinuxDeployRelease.cmake"
        VERBATIM)
elseif(Python3_Interpreter_FOUND)
    add_custom_target(Deploy
        COMMAND "${CMAKE_COMMAND}" "-DCONFIG=$<CONFIG>"
            -P "${CMAKE_CURRENT_LIST_DIR}/RequireLinuxDeployRelease.cmake"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/deployment/linux/local_deploy.py"
            --source "${CMAKE_SOURCE_DIR}" --build "${CMAKE_BINARY_DIR}"
            --binary-dir "$<TARGET_FILE_DIR:BrickSuite>"
            --qt "${_bricksuite_deploy_qt}" --qt-version "${Qt6Core_VERSION}"
            --crypto "${OPENSSL_CRYPTO_LIBRARY}" --compiler "${CMAKE_CXX_COMPILER}"
            --config "$<CONFIG>" --version "${PROJECT_VERSION}"
            ${_bricksuite_deploy_options}
        USES_TERMINAL VERBATIM
        COMMENT "Create a local Linux development package in the build directory's deploy folder")
    if(CMAKE_CONFIGURATION_TYPES OR CMAKE_BUILD_TYPE STREQUAL "Release")
        add_dependencies(Deploy BrickSuite BrickSuiteMeshBooleanWorker)
    endif()
else()
    add_custom_target(Deploy
        COMMAND "${CMAKE_COMMAND}" -DMISSING_PYTHON=ON
            -P "${CMAKE_CURRENT_LIST_DIR}/RequireLinuxDeployRelease.cmake"
        VERBATIM)
endif()
