if(WIN32)
    set(EXECUTABLE empirical.exe)
else(WIN32)
    set(EXECUTABLE empirical)
endif(WIN32)

add_test(NAME test_tutorial
         COMMAND ${CMAKE_BINARY_DIR}/${EXECUTABLE} --verify-markdown tutorial_core.md
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
