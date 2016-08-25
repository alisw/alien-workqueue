#!/bin/bash -e
export PATH=/bin:/usr/bin
export LD_LIBRARY_PATH=
export _LMFILES_=
if [[ $ALIEN_USE_PARROT == 1 ]]; then
  parrot_run -v > /dev/null 2>&1 || { echo "Note: Parrot not found, not using it"; exec "$@"; }
  ls /cvmfs/alice.cern.ch 2>&1 > /dev/null || { echo "Note: using Parrot for CVMFS"; exec parrot_run "$@"; }
fi
exec "$@"
