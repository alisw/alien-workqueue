#!/bin/bash -ex
WQ_MASTER=127.0.0.1:9094
WQ_FOREMEN=${WQ_FOREMEN:-"127.0.0.1"}
WQ_NUM_FOREMEN=${WQ_NUM_FOREMEN:-"2"}
WQ_MASTER_BASEPORT=9080
WQ_WORKDIR=$HOME/WQ_WORKDIR
WQ_DRAIN=$HOME/WQ_DRAIN

which work_queue_worker > /dev/null

pid_desc() {
  local children=$(ps -o pid= --ppid "$1")
  for pid in $children; do
    pid_desc $pid
  done
  echo $children
}

pick_port() {
  local HASH=$( (echo $$; /sbin/ifconfig | grep -i hwaddr) | sha1sum | cut -d' ' -f1 | tr '[:lower:]' '[:upper:]' )
  local NHEX=$(echo "ibase=10;obase=16; $WQ_NUM_FOREMEN" | bc)
  local PORT=$(echo "obase=10;ibase=16; $HASH % $NHEX" | bc)
  echo $((PORT + WQ_MASTER_BASEPORT))
}

function cond_redir() {
  [[ "$WQ_DEBUG" == 1 ]] && cat || tee /dev/null > /dev/null
}

case "$1" in
  workers)
    while [[ 1 ]]; do
      TIME0=$(date --utc +%s)
      work_queue_worker --cores 1               \
                        --debug all             \
                        --single-shot           \
                        --workdir $WQ_WORKDIR   \
                        $WQ_FOREMEN `pick_port` \
                        2>&1 | cond_redir
      [[ -e $WQ_DRAIN ]] && break
      [[ $((`date --utc +%s`-TIME0)) -lt 30 ]] && sleep 30
    done
  ;;
  stop)
    mkdir -p $WQ_DRAIN
    PIDS=$(ps -e -o user,pid,command | grep $USER | grep -v grep | \
      grep work_queue_worker | awk '{print $2}')
    kill -TERM $PIDS 2> /dev/null || true
    [[ "$PIDS" != '' ]] && sleep 2
    for PID in $PIDS; do
      kill -9 $PID $(pid_desc $PID) 2> /dev/null || true
    done
    rm -rf $WQ_WORKDIR
  ;;
  start-*)  # start-foremen, start-workers
    rm -rf $WQ_DRAIN
    mkdir -p $WQ_WORKDIR
    WQSERVICE=${1#*-}
    case $WQSERVICE in
      foremen) NW=$WQ_NUM_FOREMEN ;;
      workers) NW=$(grep -c bogomips /proc/cpuinfo) ;;
      *) false ;;
    esac
    NW=$(( NW-`$0 count` ))
    echo "Number of Work Queue $WQSERVICE to start: $NW"
    for ((I=0; I<NW; I++)); do
      nohup $0 $WQSERVICE $I > /dev/null 2>&1 &
    done
  ;;
  count)
    ps -e -o user,pid,command | grep $USER | grep -v grep | \
      grep -c work_queue_worker || true
  ;;
  drain)
    mkdir -p $WQ_DRAIN
  ;;
  foremen)
    while [[ 1 ]]; do
      TIME0=$(date --utc +%s)
      INDEX=$2
      work_queue_worker --foreman \
                        --foreman-port $((WQ_MASTER_BASEPORT + INDEX)) \
                        --workdir $WQ_WORKDIR \
                        `echo $WQ_MASTER | sed -e 's/:/ /' ` \
                        > /dev/null 2>&1
      [[ -e $WQ_DRAIN ]] && break
      [[ $((`date --utc +%s`-TIME0)) -lt 30 ]] && sleep 30
    done
  ;;
esac
