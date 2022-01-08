# hiredis connection pool demo

## TODO:
1、complete subscribe/psubscribe API


## 环境
linux x86_64, install redis, hiredis:
* redis 6.0.10
* hiredis 1.0.2

## compile and run

```
redis-server /etc/redis/redis.conf # 1、firstly, start redis-server

cd build/
cmake ../ && make -j4
cd test/
./test_connpool
```

## memory leak check
```
valgrind --tool=memcheck --leak-check=full  ./test_connpool
```

### other problems
1、短连接过多场景, fd超过1024解决方法: 
```
ulimit -n 8192
```

2、短连接过多场景, TIME-WAIT快速回收:
```
net.ipv4.ip_local_port_range = 32768	60999
net.ipv4.tcp_max_tw_buckets = 16384
net.ipv4.tcp_tw_reuse = 1
net.ipv4.tcp_fin_timeout = 10
```

3、短连接过多场景, REDIS服务端连接数已满，拒绝连接
```
redis-cli config set maxclients 10000     # 设置最大连接数为10000
redis-cli set timeout 5                   # 设置服务端超时自动关闭连接
```
