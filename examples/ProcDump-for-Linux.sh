#!/usr/bin/env bash

set -e

timeout 5s rgrep 'hello' ~ > /dev/null & 
pid=$!
echo "PID: $pid"
sleep 1s
$1 -p $pid
rm -f *_time_*