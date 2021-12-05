# hiredis连接池设计

## TODO:

1、连接空闲检测, 定时回收空闲的连接
2、发布订阅

#### memory leak check
valgrind --tool=memcheck --leak-check=full  ./test
