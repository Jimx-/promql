cmake_minimum_required(VERSION 3.5)
project(promql)

set(CMAKE_CXX_STANDARD 17)

set(TOPDIR ${PROJECT_SOURCE_DIR})
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${TOPDIR}/cmake")

FIND_PACKAGE(Boost COMPONENTS system filesystem iostreams)
if (NOT Boost_FOUND)
    message(FATAL_ERROR "Fatal error: Boost (version >= 1.55) required.")
else()
    message(STATUS "Setting up BOOST")
    message(STATUS " Includes - ${Boost_INCLUDE_DIRS}")
    message(STATUS " Library  - ${Boost_LIBRARY_DIRS}")
    message(STATUS " boost_system ${Boost_SYSTEM_LIBRARY}")
    message(STATUS " boost_filesystem ${Boost_FILESYSTEM_LIBRARY}")
    message(STATUS " boost_iostreams ${Boost_IOSTREAMS_LIBRARY}")
    include_directories(${Boost_INCLUDE_DIRS})
    link_directories(${Boost_LIBRARY_DIRS})
endif (NOT Boost_FOUND)

# Find libuuid
find_package(Libuuid REQUIRED)
if (NOT LIBUUID_FOUND)
   message(FATAL_ERROR "You might need to install libuuid first")
endif()

include_directories(
    include
    3rdparty/easyloggingpp/src
    3rdparty/bptree/include
    3rdparty/Simple-Web-Server
    tsdb/
    ${LIBUUID_INCLUDE_DIRS}
)

add_subdirectory(3rdparty/googletest/googletest)
enable_testing()
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

set(SOURCE_FILES
    src/value.cpp
    src/index/index_server.cpp
    src/index/index_tree.cpp
    src/index/series_manager.cpp
    src/parse/ast.cpp
    src/parse/executor.cpp
    src/parse/lexer.cpp
    src/parse/parser.cpp
    src/parse/printer.cpp
    src/web/http_server.cpp)
            
set(HEADER_FILES
    include/common.h
    include/labels.h
    include/value.h
    include/index/index_server.h
    include/index/index_tree.h
    include/index/series_manager.h
    include/parse/ast.h
    include/parse/executor.h
    include/parse/lexer.h
    include/parse/parser.h
    include/parse/printer.h
    include/parse/token.h
    include/web/http_server.h
)

set(EXT_SOURCE_FILES 
    3rdparty/easyloggingpp/src/easylogging++.cc
)

add_subdirectory(tsdb)
add_subdirectory(3rdparty/bptree)
add_subdirectory(3rdparty/Simple-Web-Server)

set(LIBRARIES
    pthread
    atomic
    simple-web-server
    bptree
    -Wl,--whole-archive
    Base
    Block
    Chunk
    Compact
    DB
    External
    Head
    Index
    Label
    Querier
    Tombstone
    TSDBUtil
    Wal
    -Wl,--no-whole-archive
    ${Boost_IOSTREAMS_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${LIBUUID_LIBRARY}
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

