#/bin/sh

zip -r tracker.zip ../*
scp tracker.zip lichunguang@192.168.30.30:/Users/lichunguang/Downloads/
rm -f tracker.zip

