if(WIN32)
    set(EXECUTABLE empirical.exe)
else(WIN32)
    set(EXECUTABLE empirical)
endif(WIN32)

file(GLOB MDFILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.md")
list(REMOVE_ITEM MDFILES README.md)
foreach(f ${MDFILES})
  add_test(NAME test_${f}
           COMMAND ${CMAKE_BINARY_DIR}/${EXECUTABLE} --verify-markdown ${f}
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach(f ${MDFILES})

# TODO need a Windows version of the test script
if(NOT WIN32)
add_test(NAME test_emp
         COMMAND ./test_emp.sh ${CMAKE_BINARY_DIR}/${EXECUTABLE}
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif(NOT WIN32)
