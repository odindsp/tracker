
CC = g++
CFLAGS = -Wall -g
LIBS= -lfcgi -pthread -ljemalloc -lhiredis -lrdkafka -lcurl
SRCPATH=src/
OBJPATH=obj/
APP_NAME=odin_tracker

OBJECTS+=$(OBJPATH)main.o
OBJECTS+=$(OBJPATH)tracker.o

OBJECTS+=$(OBJPATH)adx_list.o
OBJECTS+=$(OBJPATH)adx_rbtree.o
OBJECTS+=$(OBJPATH)adx_string.o
OBJECTS+=$(OBJPATH)adx_alloc.o
OBJECTS+=$(OBJPATH)adx_queue.o
OBJECTS+=$(OBJPATH)adx_cache.o
OBJECTS+=$(OBJPATH)adx_dump.o
OBJECTS+=$(OBJPATH)adx_curl.o
OBJECTS+=$(OBJPATH)adx_time.o
OBJECTS+=$(OBJPATH)adx_log.o

OBJECTS+=$(OBJPATH)adx_conf_file.o
OBJECTS+=$(OBJPATH)adx_json.o
OBJECTS+=$(OBJPATH)json.o

OBJECTS+=$(OBJPATH)adx_kafka.o
OBJECTS+=$(OBJPATH)adx_flume.o
OBJECTS+=$(OBJPATH)adx_redis.o

OBJECTS+=$(OBJPATH)adx_util.o
OBJECTS+=$(OBJPATH)adx_dsp_control.o
OBJECTS+=$(OBJPATH)adx_link_jump.o

tracker : $(OBJECTS)
	$(CC) -o $(APP_NAME) $(OBJECTS) $(LIBS) $(CFLAGS)
	$(CC) -o app_tracker $(OBJECTS) $(LIBS) $(CFLAGS)

install:
	rm -f /usr/bin/$(APP_NAME)
	cp $(APP_NAME) /usr/bin/$(APP_NAME)

$(OBJPATH)%.o : $(SRCPATH)%.c
	gcc $(INCLUDE) -c $< -o $@ $(CFLAGS)

$(OBJPATH)%.o : $(SRCPATH)%.cpp
	$(CC) $(INCLUDE) -c $< -o $@ $(CFLAGS)
clean :
	@rm -f $(OBJECTS)
	@rm -f $(APP_NAME)
	@rm -f app_tracker

	

