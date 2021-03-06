#!/bin/bash -e
WQ_MASTER=${WQ_MASTER:-"127.0.0.1:9094"}
WQ_FOREMEN=${WQ_FOREMEN:-"127.0.0.1"}
WQ_NUM_FOREMEN=${WQ_NUM_FOREMEN:-"2"}
WQ_MASTER_BASEPORT=${WQ_MASTER_BASEPORT:-"9080"}
WQ_WORKDIR=${WQ_WORKDIR:-"$HOME/.alien_wq_tmp"}
WQ_DRAIN=${WQ_DRAIN:-"$HOME/.alien_wq_drain"}
WQ_SINGLETASK=${WQ_SINGLETASK:-"0"}
WQ_IDLETIMEOUT=${WQ_IDLETIMEOUT:-"100"}
WQ_NORESPAWN=${WQ_NORESPAWN:-"0"}
[[ ! -z "$USER" ]] || USER=$(whoami)
[[ $WQ_DEBUG == 1 ]] && set -x

work_queue_worker --help > /dev/null 2>&1 || { echo "Work Queue not found!" >&2; exit 1; }
[[ $WQ_SINGLETASK == 1 ]] \
  && { work_queue_worker --help 2>&1 | grep -q -- --single-task || { echo "This Work Queue does not support --single-task!" >&2; exit 1; }; } \
  || WQ_SINGLETASK=

pid_desc() {
  local CHILDREN=$(ps -o pid= --ppid "$1")
  for PID in $CHILDREN; do
    pid_desc $PID
  done
  echo $CHILDREN
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
    set -x
    while [[ 1 ]]; do
      TIME0=$(date --utc +%s)
      mkdir -p $WQ_WORKDIR
      work_queue_worker --cores 1                                         \
                        --debug all                                       \
                        --single-shot                                     \
                        --workdir $WQ_WORKDIR                             \
                        ${WQ_IDLETIMEOUT:+--idle-timeout=$WQ_IDLETIMEOUT} \
                        ${WQ_SINGLETASK:+--single-task}                   \
                        $WQ_FOREMEN `pick_port`                           \
                        2>&1 | cond_redir
      [[ $WQ_NORESPAWN == 1 || -e $WQ_DRAIN ]] && break
      [[ $((`date --utc +%s`-TIME0)) -lt 30 ]] && sleep 30
    done
  ;;
  stop-*)
    mkdir -p $WQ_DRAIN
    WQSERVICE=${1#*-}
    case $WQSERVICE in
      foremen) GREP=--foreman ;;
      workers) GREP=--cores ;;
      *) false ;;
    esac
    PIDS=$(ps -e -o user,pid,command | grep $USER | grep -v grep | \
      grep work_queue_worker | grep -- $GREP | awk '{print $2}')
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
      workers) NW=$WQ_NUM_WORKERS
               [[ -z $NW ]] && NW=$(grep -c bogomips /proc/cpuinfo) || true ;;
      *) false ;;
    esac
    NW=$(( NW-`$0 count-$WQSERVICE` ))
    echo "Number of Work Queue $WQSERVICE to start: $NW" >&2
    for ((I=0; I<NW; I++)); do
      nohup $0 $WQSERVICE $I > /dev/null 2>&1 &
    done
  ;;
  count-*)
    WQSERVICE=${1#*-}
    case $WQSERVICE in
      foremen) GREP=--foreman ;;
      workers) GREP=--cores ;;
      *) false ;;
    esac
    ps -e -o user,pid,command | grep $USER | grep -v grep | \
      grep work_queue_worker | grep -c -- $GREP || true
  ;;
  drain)
    mkdir -p $WQ_DRAIN
  ;;
  foremen)
    while [[ 1 ]]; do
      TIME0=$(date --utc +%s)
      INDEX=$2
      work_queue_worker --foreman                                      \
                        --foreman-port $((WQ_MASTER_BASEPORT + INDEX)) \
                        --workdir $WQ_WORKDIR                          \
                        `echo $WQ_MASTER | sed -e 's/:/ /' `           \
                        > /dev/null 2>&1
      [[ -e $WQ_DRAIN ]] && break
      [[ $((`date --utc +%s`-TIME0)) -lt 30 ]] && sleep 30
    done
  ;;
  *)
    echo "$1: invalid command" >&2
    false
  ;;
esac
