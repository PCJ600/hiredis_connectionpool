cmake_minimum_required(VERSION 3.13)

add_definitions("-g -O0 -Wall -Werror")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/hiredis-1.0.2
)

add_library(connpool SHARED hiredis_api.c hiredis_connpool.c hiredis_pubsub.c list.c)
target_link_libraries(connpool hiredis event pthread)

add_executable(test_connpool test_connpool.c)
target_link_libraries(test_connpool connpool)

add_subdirectory(test)
