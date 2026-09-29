# The pinned queue can lose a wakeup between its predicate check and wait:
# push() changes the tail without acquiring the condition variable's head mutex.
# Reuse its existing synchronized notification after releasing the tail mutex.
# Keep the downloaded source intact and scope the corrected header to MCUT.
# The historical filename/output path is retained for Linux package provenance;
# the lost-wakeup also reproduces on macOS and this correction is platform-neutral.
set(_mcut_queue_header "${mcut_SOURCE_DIR}/include/mcut/internal/tpool.h")
file(READ "${_mcut_queue_header}" _mcut_queue_contents)
set(_mcut_queue_original "        data_cond.notify_one();\n    }\n\n    void wait_and_pop(T& value)")
set(_mcut_queue_replacement "        disrupt_wait_for_data();\n    }\n\n    void wait_and_pop(T& value)")
string(FIND "${_mcut_queue_contents}" "${_mcut_queue_original}" _mcut_queue_patch_at)
if(_mcut_queue_patch_at EQUAL -1)
    message(FATAL_ERROR "MCUT Linux queue fix requires the pinned tpool.h implementation")
endif()
string(REPLACE "${_mcut_queue_original}" "${_mcut_queue_replacement}"
    _mcut_queue_contents "${_mcut_queue_contents}")
set(_mcut_queue_include "${CMAKE_CURRENT_BINARY_DIR}/mcut-linux-include")
set(_mcut_queue_output "${_mcut_queue_include}/mcut/internal/tpool.h")
set(_mcut_queue_previous "")
if(EXISTS "${_mcut_queue_output}")
    file(READ "${_mcut_queue_output}" _mcut_queue_previous)
endif()
if(NOT _mcut_queue_previous STREQUAL _mcut_queue_contents)
    file(MAKE_DIRECTORY "${_mcut_queue_include}/mcut/internal")
    file(WRITE "${_mcut_queue_output}" "${_mcut_queue_contents}")
endif()
target_include_directories(mcut BEFORE PRIVATE "${_mcut_queue_include}")
