CCTOOLS := /cvmfs/alice.cern.ch/x86_64-2.6-gnu-4.1.2/Packages/cctools/v5.3.1-2

awq: awq.cpp
	g++ -g -O2 $^ -o $@ -I$(CCTOOLS)/include/cctools -L$(CCTOOLS)/lib -lwork_queue -ldttools -lm
