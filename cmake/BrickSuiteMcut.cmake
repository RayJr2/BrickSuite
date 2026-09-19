include(FetchContent)

# MCUT is a required shared-library dependency of BrickSuite print preparation.
# Pin mio first because MCUT v1.3.0 otherwise declares it from a moving branch.
FetchContent_Declare(mio
    GIT_REPOSITORY https://github.com/cutdigital/mio.git
    GIT_TAG 474d060bddd1d9a3e69c439e31ae6f3dae3d55ad
    GIT_SHALLOW FALSE)

set(MCUT_BUILD_AS_SHARED_LIB ON CACHE BOOL "Build MCUT as a shared library" FORCE)
set(MCUT_BUILD_WITH_COMPUTE_HELPER_THREADPOOL OFF CACHE BOOL "Disable MCUT helper threadpool" FORCE)
set(MCUT_BUILD_TESTS OFF CACHE BOOL "Build MCUT tests" FORCE)
set(MCUT_BUILD_TUTORIALS OFF CACHE BOOL "Build MCUT tutorials" FORCE)
set(MCUT_BUILD_DOCUMENTATION OFF CACHE BOOL "Build MCUT documentation" FORCE)

set(_bricksuite_mcut_patch)
if(WIN32 AND MINGW)
    set(_bricksuite_mcut_patch
        PATCH_COMMAND "${CMAKE_COMMAND}"
            -DMCUT_SOURCE_DIR=<SOURCE_DIR>
            -P "${CMAKE_CURRENT_LIST_DIR}/ApplyMcutMinGwPatch.cmake")
endif()

FetchContent_Declare(mcut
    GIT_REPOSITORY https://github.com/cutdigital/mcut.git
    GIT_TAG 047d75ffe6e33ede572cb25217047a4756188401
    GIT_SHALLOW FALSE
    ${_bricksuite_mcut_patch})
FetchContent_MakeAvailable(mcut)
target_include_directories(mcut INTERFACE "${mcut_SOURCE_DIR}/include")
target_compile_definitions(mcut INTERFACE MCUT_SHARED_LIB=1)

add_library(BrickSuiteMcut INTERFACE)
target_link_libraries(BrickSuiteMcut INTERFACE mcut)
target_compile_definitions(BrickSuiteMcut INTERFACE
    BRICKSUITE_MCUT_VERSION="1.2.0-047d75f")
add_library(BrickSuite::Mcut ALIAS BrickSuiteMcut)

function(bricksuite_stage_mcut_runtime target_name)
    if(WIN32)
        add_custom_command(TARGET "${target_name}" POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:mcut>" "$<TARGET_FILE_DIR:${target_name}>")
    endif()
endfunction()
