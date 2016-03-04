alien-workqueue
===============

Submit AliEn agents to Work Queue. Used to configure an AliEn site using Work
Queue as a backend to manage jobs.

Instant gratification
---------------------

We will be assuming that:

* CCTools comes from `/cvmfs/alice.cern.ch/x86_64-2.6-gnu-4.1.2/Packages/cctools/v5.3.1-2`
* alien-workqueue will be installed to `$HOME/awq`

You need to install [CCTools](http://ccl.cse.nd.edu/software/downloadfiles.php)
first.

Once you have done that, clone, configure and install:

    git clone https://github.com/dberzano/alien-workqueue
    cd alien-workqueue
    mkdir build
    cd build
    cmake .. -DCMAKE_INSTALL_PREFIX=$HOME/awq -DCCTOOLS=/cvmfs/alice.cern.ch/x86_64-2.6-gnu-4.1.2/Packages/cctools/v5.3.1-2
    make install


Starting Work Queue workers and foremen
---------------------------------------

In our simple setup we have:

* alien-workqueue acting as a Work Queue master
* a layer of Work Queue foremen to offload jobs control (they will act as a
  worker with respect to the master, and as a master with respect to the
  workers)

The AliEn submit node must be configured as a Condor node. alien-workqueue
comes with a fake `condor_q` and `condor_submit` that will enable the AliEn
Condor backend to perform Work Queue submission as if it were a pure Condor
site.

Start the foremen as follows (use `WQ_NUM_FOREMEN` for the number of desired
foremen):

    source $HOME/awq/etc/env.sh
    env WQ_NUM_FOREMEN=12 alien_wq_worker.sh start-foremen

By default they will connect to a master on `localhost:9094`. If you want to
change the master, use `WQ_MASTER=host:port` (port is mandatory).

Foremen will be started in the background. They can be terminated with:

    alien_wq_worker.sh stop

Used ports are from 9080 to 9080+`$WQ_NUM_FOREMEN`.

Now start the AliEn CE service, again *after* loading the alien-workqueue
environment (which is necessary for AliEn to pick our fake `condor_q` and
`condor_submit`):

    source $HOME/awq/etc/env.sh
    alien StartCE


Starting workers on execute nodes
---------------------------------

The procedure is similar.

    source $HOME/awq/etc/env.sh
    env WQ_NUM_FOREMEN=12 WQ_FOREMEN=foremen_host alien_wq_worker.sh start-workers

Set `$WQ_FOREMEN` to the hostname or IP address of the host running foremen.

To stop them:

    alien_wq_worker.sh stop


Motivation and use
------------------

alien-workqueue enables Work Queue to be used as a (fake) Condor backend for
ALICE AliEn job submission. Since we are submitting pilot jobs we do not need a
complex job scheduling mechanism and Work Queue is lightweight enough for us.

We are currently using alien-workqueue in production for opportunistically
using the ALICE High Level Trigger clusters outside data taking. With this
extremely simple approach, constituted by:

* 12 foremen
* alien-workqueue
* the AliEn VoBox services

all running on a single box we can easily scale to 10k concurrently running
jobs.

Moreover, with the [mesos-workqueue](https://github.com/alisw/mesos-workqueue)
tool we have developed we can run the ALICE AliEn middleware on
[Mesos](http://mesos.apache.org/).


Acknowledgements
----------------

Thanks to the CCTools people for making such a great tool!
