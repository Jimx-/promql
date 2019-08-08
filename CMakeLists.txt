cmake_minimum_required(VERSION 3.5)
project(promql)

set(CMAKE_CXX_STANDARD 17)

option(PROMQL_BUILD_TESTS "set ON to build library tests" OFF)

set(TOPDIR ${PROJECT_SOURCE_DIR})
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${TOPDIR}/cmake")

set(INCLUDE_DIRS
    ${TOPDIR}/include
    ${TOPDIR}/3rdparty/Simple-Web-Server
    ${TOPDIR}/3rdparty/CTPL
    ${TOPDIR}/3rdparty/inja/single_include
    ${TOPDIR}/3rdparty/inja/third_party/include
)

include_directories(
    ${INCLUDE_DIRS}
)

set(SOURCE_FILES
    ${TOPDIR}/src/labels.cpp
    ${TOPDIR}/src/value.cpp
    ${TOPDIR}/src/parse/ast.cpp
    ${TOPDIR}/src/parse/executor.cpp
    ${TOPDIR}/src/parse/functions.cpp
    ${TOPDIR}/src/parse/lexer.cpp
    ${TOPDIR}/src/parse/parser.cpp
    ${TOPDIR}/src/parse/printer.cpp
    ${TOPDIR}/src/web/http_server.cpp)
            
set(HEADER_FILES
    ${TOPDIR}/include/promql/common.h
    ${TOPDIR}/include/promql/labels.h
    ${TOPDIR}/include/promql/value.h
    ${TOPDIR}/include/promql/parse/ast.h
    ${TOPDIR}/include/promql/parse/executor.h
    ${TOPDIR}/include/promql/parse/functions.h
    ${TOPDIR}/include/promql/parse/lexer.h
    ${TOPDIR}/include/promql/parse/parser.h
    ${TOPDIR}/include/promql/parse/printer.h
    ${TOPDIR}/include/promql/parse/token.h
    ${TOPDIR}/include/promql/web/http_server.h
)

set(EXT_SOURCE_FILES )

add_subdirectory(${TOPDIR}/3rdparty/Simple-Web-Server)

set(LIBRARIES
    pthread
    atomic
    simple-web-server
)
 
add_library(promql STATIC ${SOURCE_FILES} ${HEADER_FILES} ${EXT_SOURCE_FILES})
target_link_libraries(promql ${LIBRARIES})
target_include_directories(promql PUBLIC ${INCLUDE_DIRS})
install(TARGETS promql DESTINATION lib)

if (PROMQL_BUILD_TESTS)

add_subdirectory(${TOPDIR}/3rdparty/googletest/googletest)
enable_testing()
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

set(TEST_SOURCE_FILES
    tests/main.cpp
    tests/parse/lexer_test.cpp
    tests/parse/parser_test.cpp)
    
add_executable(promql_unit_tests ${EXT_SOURCE_FILES} ${TEST_SOURCE_FILES})
target_link_libraries(promql_unit_tests promql gtest gtest_main ${LIBRARIES})
add_test(promql_unit_tests promql_unit_tests)

endif()
