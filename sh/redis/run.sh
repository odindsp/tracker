#!/bin/sh

pkill -9 redis
rm -rf nodes-*

./redis-server redis.conf 
./redis-server 7001.conf
./redis-server 7002.conf
./redis-server 7003.conf
./redis-trib.rb create 127.0.0.1:7001 127.0.0.1:7002 127.0.0.1:7003

