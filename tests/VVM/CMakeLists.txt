# TODO need a Windows version of the test script
if(NOT WIN32)
set(EXECUTABLE empirical)
add_test(NAME test_vvm
         COMMAND ./test_vvm.sh ${CMAKE_BINARY_DIR}/${EXECUTABLE}
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif(NOT WIN32)
