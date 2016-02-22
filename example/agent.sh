#!/bin/bash -e
SLEEP=$((RANDOM % 10 + 5))
file=proxy.txt
cat >$file <<EOF
*** IN REAL LIFE THIS CONTAINS A PROXY ***
EOF
chmod 0400 $file
export X509_USER_PROXY=$file
env
bash -x /cvmfs/alice.cern.ch/bin/alienv --alien-version v2-19-276 -alien -c ALIEN_DEBUG='-d:Trace' alien proxy-info
echo sleeping for $SLEEP seconds
sleep $SLEEP
