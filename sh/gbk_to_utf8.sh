#/bin/sh

src=(
'bid_filter.h'
'bid_filter_type.h'
'bid_flow_detail.h'
'confoperation.cpp'
'confoperation.h'
'errorcode.h'
'getlocation.h'
'init_context.cpp'
'init_context.h'
'json.c'
'json.h'
'json_util.cpp'
'json_util.h'
'rdkafka_operator.cpp'
'rdkafka_operator.h'
'redisimpclk.cpp'
'redisimpclk.h'
'selfsemphore.cpp'
'selfsemphore.h'
'server_log.h'
'setlog.cpp'
'setlog.h'
'tcp.cpp'
'tcp.h'
'type.h'
)

src2=(
'util.cpp'
'util.h'
'url_endecode.cpp'
'url_endecode.h'
)

src3=(
'main.cpp'
'tracker.cpp'
'tracker.h'
)

mkdir bak
rm -f src/*

for i in ${src[@]}
do
	iconv -f gbk -t utf8 /make/code/common/$i -o bak/$i
	./py_run.py bak/$i > src/$i
done

for i in ${src2[@]}
do
	cp /make/code/common/$i bak/$i
	./py_run.py bak/$i > src/$i
done

for i in ${src3[@]}
do
	cp /make/code/tracker/$i bak/$i
	./py_run.py bak/$i > src/$i
done

rm -r -f bak




