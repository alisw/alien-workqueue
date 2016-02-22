#!/bin/bash
AWQ=${AWQ:-/tmp/aa}
mkdir -p $AWQ/queue
mkdir -p output
N=$1
for ((I=0; I<N; I++)); do
  echo executable = $PWD/agent.sh > $AWQ/queue/job$I
  echo output = $PWD/output/agent-$I.txt >> $AWQ/queue/job$I
  echo 'environment=CONDOR_ID=$(Cluster).$(Process);;ALIEN_CM_AS_LDAP_PROXY=alihltcloud.cern.ch:8084;ALIEN_JOBAGENT_ID=1377455.0;ALIEN_ALICE_CM_AS_LDAP_PROXY=alihltcloud.cern.ch:8084;THISVAR=' >> $AWQ/queue/job$I
done
