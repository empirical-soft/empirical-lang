add_executable(conversion conversion.cpp)
add_test(test_conversion conversion)

file(GLOB STRTIME "${PROJECT_SOURCE_DIR}/thirdparty/strtime/*.c")
SET(TIMESTAMP_SRC "${PROJECT_SOURCE_DIR}/src/VVM/utils/timestamp.cpp")
add_executable(timestamp timestamp.cpp ${STRTIME} ${TIMESTAMP_SRC})
add_test(test_timestamp timestamp)

set(CSV_INFER_SRC "${PROJECT_SOURCE_DIR}/src/VVM/utils/csv_infer.cpp")
add_executable(csv_infer csv_infer.cpp ${STRTIME} ${TIMESTAMP_SRC}
               ${CSV_INFER_SRC})
add_test(NAME test_csv_infer
         COMMAND ${CMAKE_BINARY_DIR}/tests/VVM/utils/csv_infer
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
