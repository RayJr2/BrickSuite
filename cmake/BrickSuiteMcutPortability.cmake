# Preserve the pinned source; generate only the two narrowly corrected files.
function(_bricksuite_mcut_replace input output before after)
    file(READ "${input}" contents)
    string(FIND "${contents}" "${before}" patch_at)
    if(patch_at EQUAL -1)
        message(FATAL_ERROR "MCUT portability correction requires the pinned implementation: ${input}")
    endif()
    string(REPLACE "${before}" "${after}" contents "${contents}")
    set(previous "")
    if(EXISTS "${output}")
        file(READ "${output}" previous)
    endif()
    if(NOT previous STREQUAL contents)
        get_filename_component(directory "${output}" DIRECTORY)
        file(MAKE_DIRECTORY "${directory}")
        file(WRITE "${output}" "${contents}")
    endif()
endfunction()

set(BRICKSUITE_MCUT_PORTABILITY_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/mcut-portability/include")
set(_mcut_preproc "${CMAKE_CURRENT_BINARY_DIR}/mcut-portability/preproc.cpp")
# default_random_engine means different engines in libc++ and libstdc++.
# Name the engine already used by the validated Linux path, retaining its seed.
_bricksuite_mcut_replace("${mcut_SOURCE_DIR}/source/preproc.cpp" "${_mcut_preproc}"
    "std::default_random_engine random_engine(1)" "std::minstd_rand0 random_engine(1)")
get_target_property(_mcut_sources mcut SOURCES)
list(REMOVE_ITEM _mcut_sources "${mcut_SOURCE_DIR}/source/preproc.cpp")
set_property(TARGET mcut PROPERTY SOURCES "${_mcut_sources}")
target_sources(mcut PRIVATE "${_mcut_preproc}")

# MCUT can generate non-finite projected coordinates during duplicate handling.
# Reject before computing a super-triangle or entering the unbounded KD search.
_bricksuite_mcut_replace(
    "${mcut_SOURCE_DIR}/include/mcut/internal/cdt/triangulate.h"
    "${BRICKSUITE_MCUT_PORTABILITY_INCLUDE}/mcut/internal/cdt/triangulate.h"
    "    detail::randGenerator.seed(9001); // ensure deterministic behavior"
    "    for (TVertexIter it = first; it != last; ++it) {\n        if (!std::isfinite(get_x_coord(*it)) || !std::isfinite(get_y_coord(*it)))\n            throw std::invalid_argument(\"non-finite CDT vertex\");\n    }\n\n    detail::randGenerator.seed(9001); // ensure deterministic behavior")
target_include_directories(mcut BEFORE PRIVATE "${BRICKSUITE_MCUT_PORTABILITY_INCLUDE}")

# Use separately rounded operations for MCUT, including its robust predicates.
# ARM's fused arithmetic otherwise changes the extraction path versus baseline
# x86 builds. These options do not propagate to application geometry or Qt.
if(MSVC)
    target_compile_options(mcut PRIVATE /fp:strict)
else()
    target_compile_options(mcut PRIVATE
        "$<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-ffp-contract=off>"
        "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-ffp-contract=off>")
endif()
