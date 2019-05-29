cmake_minimum_required(VERSION 3.5)
project(promql)

set(CMAKE_CXX_STANDARD 17)

set(TOPDIR ${PROJECT_SOURCE_DIR})

include_directories(
    include
    3rdparty/easyloggingpp/src
    3rdparty/bptree/include
    3rdparty/Simple-Web-Server
)

add_subdirectory(3rdparty/googletest/googletest)
enable_testing()
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

set(SOURCE_FILES
    src/value.cpp
    src/index/index_server.cpp
    src/index/index_tree.cpp
    src/parse/ast.cpp
    src/parse/executor.cpp
    src/parse/lexer.cpp
    src/parse/parser.cpp
    src/parse/printer.cpp)
            
set(HEADER_FILES
    include/common.h
    include/labels.h
    include/value.h
    include/index/index_server.h
    include/index/index_tree.h
    include/parse/ast.h
    include/parse/executor.h
    include/parse/lexer.h
    include/parse/parser.h
    include/parse/printer.h
    include/parse/token.h
)

set(EXT_SOURCE_FILES 
    3rdparty/easyloggingpp/src/easylogging++.cc
)

add_subdirectory(3rdparty/bptree)
add_subdirectory(3rdparty/Simple-Web-Server)

set(LIBRARIES
    pthread
    atomic
    simple-web-server
    bptree
)
 
add_executable(promql src/main.cpp ${SOURCE_FILES} ${HEADER_FILES} ${EXT_SOURCE_FILES})
target_link_libraries(promql ${LIBRARIES}) 

set(TEST_SOURCE_FILES
    tests/main.cpp
    tests/parse/lexer_test.cpp
    tests/parse/parser_test.cpp)
    
add_executable(promql_unit_tests ${SOURCE_FILES} ${EXT_SOURCE_FILES} ${TEST_SOURCE_FILES})
target_link_libraries(promql_unit_tests gtest gtest_main ${LIBRARIES})
add_test(promql_unit_tests promql_unit_tests)

