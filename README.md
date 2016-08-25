AliEn Work Queue integration
============================

Submit [ALICE](https://alice.cern.ch) pilot jobs to Work Queue. Configure an
[AliEn](https://alien.cern.ch) Grid site using [Work Queue from
CCTools](http://ccl.cse.nd.edu/software/manuals/workqueue.html) as a backend to
manage jobs, without the need for a complex resource management system.


Instant gratification
---------------------

Assumptions:

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

This will install it under `~/awq`.


Start Work Queue foremen on the head node
-----------------------------------------

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
    WQ_NUM_FOREMEN=12 alien_wq_worker.sh start-foremen

By default they will connect to a master on `localhost:9094`. If you want to
change the master, prepend `WQ_MASTER=host:port` (port is mandatory).

Foremen will be started in the background. They can be terminated with:

    alien_wq_worker.sh stop-foremen

Used ports are by defaut from `9080` to `9080+$WQ_NUM_FOREMEN-1`. See the
[advanced usage](#advanced-functionalities) for configuring this paramenter and
more.


Start AliEn Work Queue and the AliEn CE on the head node
--------------------------------------------------------

Now start the AliEn CE service, again *after* loading the alien-workqueue
environment (which is necessary for AliEn to pick our fake `condor_q` and
`condor_submit`):

    source $HOME/awq/etc/env.sh
    alien StartCE

Now it is recommended you open a "screen" to start alien-workqueue. Open a
screen:

    screen -S awq_screen

Inside the screen:

    source $HOME/awq/etc/env.sh
    alien_work_queue

Terminate it with `Ctrl-C`.

Note that by default no [Work Queue
Catalog](http://ccl.cse.nd.edu/software/manuals/catalog.html) is updated. To
enable reporting to the catalog see the `--catalog-*` parameters under
[advanced usage](#advanced-functionalities).


Starting workers on execute nodes
---------------------------------

The procedure is similar.

    source $HOME/awq/etc/env.sh
    WQ_NUM_FOREMEN=12 WQ_FOREMEN=foremen_host alien_wq_worker.sh start-workers

Set `$WQ_FOREMEN` to the hostname or IP address of the host running foremen.

To stop them:

    alien_wq_worker.sh stop-workers


Advanced functionalities
------------------------

The following variables can be defined when starting `alien_wq_worker.sh`:

* `WQ_MASTER`: `host:port` of the Work Queue master, _i.e._ the
  `alien_work_queue` process. Port is mandatory. Defaults to `127.0.0.1:9094`.
* `WQ_FOREMEN`: host (IP or hostname) where foremen are running. Defaults to
  `127.0.0.1`.
* `WQ_NUM_FOREMEN`: number of foremen. Defaults to `2`.
* `WQ_MASTER_BASEPORT`: first listening port of the foremen. Every foreman will
  listen to `$WQ_MASTER_BASEPORT + index`. Defaults to `9080`.
* `WQ_WORKDIR`: writable temporary directory for Work Queue foremen and/or
  workers. Defaults to `~/.alien_wq_tmp`. Does not need to be created in
  advance.
* `WQ_DRAIN`: if this file exists (by default `~/.alien_wq_drain`) it puts
  workers and foremen in drain mode, _i.e._ when current jobs are done, exits.

The following options are available to `alien_work_queue`:

* `--job-wrapper <full_path>`: specify a path to an executable (a simple script
  will do) used to load the AliEn job agent. The script must at some point load
  it with something like `exec "$@"` (therefore by maintaining the PID). Before
  doing this process it can, for instance, set the environment properly. We
  provide an example, installed under `<prefix>/etc/alien_job_wrapper.sh`, which
  is used to conditionally run the job agent through
  [Parrot](http://ccl.cse.nd.edu/software/parrot/) if the
  [CVMFS](https://cernvm.cern.ch/portal/filesystem) mount point is not available
  in the job environment.
* `--work-dir <full_path>`: working directory (defaults to `/tmp/awq`). If you
  set this parameter you must also `export AWQ_WORKDIR` for making it visible
  to the fake `condor_*` commands.
* `--catalog-host <host>`, `--catalog-port <port>`: if specified, report
  information to the specified Work Queue Catalog server. Port is optional and
  defaults to `9097`. Project name can be optionally specified with
  `--project-name`.
* `--project-name`: project name reported to the catalog server, if enabled.
  Defaults to `alien_jobs`.


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

all running on a single box we can easily scale from zero to 10k concurrently
running jobs within minutes!

Moreover, with the [mesos-workqueue](https://github.com/alisw/mesos-workqueue)
tool we have developed we can run the ALICE AliEn middleware on
[Mesos](http://mesos.apache.org/)-provided resources too.


Acknowledgements
----------------

Thanks to the CCTools people for making such a great tool!
