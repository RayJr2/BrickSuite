# Release prefix mapping intentionally makes __FILE__ relative. This existing
# structural UI test reads source beside __FILE__; run it from the checkout so
# its unchanged assertions still inspect the real implementation.
function(bricksuite_baseline_test_directories)
    if(TEST PrintingHelpUx)
        set_tests_properties(PrintingHelpUx PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    endif()
endfunction()

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    cmake_language(DEFER CALL bricksuite_baseline_test_directories)
endif()
