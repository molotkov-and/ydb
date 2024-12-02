#!/bin/sh
ps aux | grep "/home/molotkov-and/ydb/ydb/apps/ydbd/ydbd server" | grep -v "grep" | awk '{print $2}' | while read line;do kill $line;done
