#/bin/sh

# 启动
# ./zookeeper-server-start.sh ../config/zookeeper.properties
# ./kafka-server-start.sh ../config/server4.properties

# 创建topic
# ./kafka-topics.sh --create --zookeeper localhost:2181 --replication-factor 1 --partitions 1 --topic sun_test
# 创建 list
# ./kafka-topics.sh --list --zookeeper localhost:2181

# 发送端
# ./kafka-console-producer.sh --broker-list localhost:9092 --topic sun_test
#接收段
# ./kafka-console-consumer.sh --zookeeper localhost:2181 --topic sun_test --from-beginning

