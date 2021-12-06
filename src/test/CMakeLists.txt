cmake_minimum_required(VERSION 3.13)

add_definitions("-g -Wall -Werror")

# testcase
add_executable(testcase_shortconn testcase_shortconn.c)
target_link_libraries(testcase_shortconn pthread hiredis)

# testcase
add_executable(testcase_singleconn testcase_singleconn.c)
target_link_libraries(testcase_singleconn pthread hiredis)
