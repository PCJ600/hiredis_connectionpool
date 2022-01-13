# hiredis connection pool

## runtime environment
linux x86_64, install redis:
* redis 6.0.10

## compile and run

```
# 1、firstly, start redis-server
redis-server /etc/redis/redis.conf 

# 2、complie and run hiredis connection pool demo
cd build/
cmake ../ && make -j4
cd test/
./test_connpool
```

## memory leak check
```
valgrind --tool=memcheck --leak-check=full  ./test_connpool
```
