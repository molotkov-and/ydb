#!/bin/sh
ps aux | grep "ydbd server" | grep -v "grep" | awk '{print $2}' | while read line;do kill $line;done
