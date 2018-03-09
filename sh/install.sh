#/bin/sh

pkill -9 app_tracker
sleep 1

spawn-fcgi -a "127.0.0.1" -p 4000 -f /make/tracker/app_tracker


