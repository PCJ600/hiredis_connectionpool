# A Redis Connection Pool Based On Hiredis

## Installation Requirements
* Redis version 3.0 or above
* CMake verison 2.8 or above


## Compile And Run

firstly, start redis-server
```
redis-server /etc/redis/redis.conf 
```

build connection pool library: libconnpool.so
```
./build.sh
ls ./src/build/libconnpool.so
```

Run Redis connection pool demo
```
cd src/test
./test_connpool
```
