#!/bin/bash

set -e

_stop() {
  readarray -t OLD_PIDS<<<"$(ps -eo user,pid,cmd | grep 'out/dfs' | grep -v grep | awk '{ print $2 }')"
  
  for OLD_PID in "${OLD_PIDS[@]}"; do
    if [ ! -z "$OLD_PID" ]; then
      sudo kill -9 "$OLD_PID"
      echo "killed server w/ pid $OLD_PID"
    fi
  done
}

_start() {
  if [ -z $1 ] ; then
    echo 'tell me how many servers to spin up, p&ty'
    exit 1
  fi

  for ((i=1; i<=$1; ++i)); do
    out/dfs "dfs$i" "1000$i" &
  done
}

case "$1" in
  start)
    _start $2
    ;;
  stop)
    _stop
    ;;
  restart)
    _stop
    _start $2
    ;;
  *)
    echo "no action specified"
    echo "expected one of: start stop restart"
    exit 1
    ;;
esac
