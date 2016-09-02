#!/bin/bash -e
MINIENV=( "PATH=/bin:/usr/bin"
          "HOME=$HOME"
          "USER=$USER"
          "LANG=C"
          "LANGUAGE=C"
          "LC_ALL=C"
          "LC_COLLATE=C"
          "LC_CTYPE=C"
          "LC_MESSAGES=C"
          "LC_MONETARY=C"
          "LC_NUMERIC=C"
          "LC_TIME=C"
          "LC_ALL=C" )
if [[ $ALIEN_USE_PARROT == 1 ]]; then
  parrot_run -v > /dev/null 2>&1 || { echo "NOTE: Parrot not found, not using it"; exec env -i ${MINIENV[*]} "$@"; }
  ls /cvmfs/alice.cern.ch 2>&1 > /dev/null || { echo "NOTE: using Parrot for CVMFS"; exec parrot_run env -i ${MINIENV[*]} "$@"; }
fi
exec env -i ${MINIENV[*]} "$@"
