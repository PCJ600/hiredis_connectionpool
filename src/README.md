# hiredis connection pool demo

## TODO:
1、检测连接空闲, 定时回收空闲连接
(问题: 当前连接池采用FIFO存储空闲连接, 考虑只有一个线程频繁调DB接口从连接池获取连接的场景, 会导致无法有效地检测到空闲连接)


## 环境
debian 10 amd64
源码安装redis, hiredis, libevent:
redis 6.0.10
hiredis 1.0.2
libevent 2.0.18-stable

## compile and run
cd build 
cmake ../ && make -j4
./test_connpool

## memory leak check
valgrind --tool=memcheck --leak-check=full  ./test_connpool

### 其他问题
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
