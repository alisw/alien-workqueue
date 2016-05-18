#include <iostream>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <cstdio>
#include <vector>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <cassert>
#include <ctime>
#include <regex.h>
#include <unistd.h>
extern "C" {
  #include <work_queue.h>
}

#define debug(...) fprintf(stderr, __VA_ARGS__); fputc('\n', stderr)
#define AWQ_DIRMODE (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)
#define AWQ_WAIT_TASK 5
#define AWQ_MAX_TASK_WAIT 30
#define AWQ_JOB_LIMIT 10000

int is_file(const std::string &path) {
  static struct stat statbuf;
  stat(path.c_str(), &statbuf);
  return S_ISREG(statbuf.st_mode);
}

void write_stats(struct work_queue *q) {
  static char buf[10];
  struct work_queue_stats stats;
  int index = 0;
  work_queue_get_stats(q, &stats);
  debug("WORKERS: init: %d / ready: %d / busy: %d",
        stats.workers_init, stats.workers_ready, stats.workers_busy);
  debug("TASKS: waiting: %d / complete: %d / running: %d",
        stats.tasks_waiting, stats.tasks_complete, stats.tasks_running);
  debug("writing stats");
  mkdir("/tmp/aa", AWQ_DIRMODE);
  mkdir("/tmp/aa/stat", AWQ_DIRMODE);
  std::ofstream of("/tmp/aa/stat/tmp");
  of << "WAITING=" << stats.tasks_waiting << std::endl;
  of << "RUNNING=" << stats.tasks_running << std::endl;
  of << "TIMESTAMP=" << time(NULL) << std::endl;
  of.close();
  rename("/tmp/aa/stat/tmp", "/tmp/aa/stat/latest");  // atomic
}

void watch_queue(struct work_queue *q, std::vector<int> &taskids) {
  debug("watching for new jobs in spool");
  const char *qdir = "/tmp/aa/queue";
  DIR *dp = opendir(qdir);
  struct dirent *ep;
  std::stringstream jobfn_s;
  regex_t rkv;
  int rkv_v = regcomp(&rkv, "^\\s*([A-Za-z]+)\\s*=\\s*(.*)\\s*$", REG_EXTENDED);
  regmatch_t m[3];
  assert(rkv_v == 0);
  assert(dp != NULL);
  while (ep = readdir(dp)) {
    if (taskids.size() >= AWQ_JOB_LIMIT) {
      debug("job limit reached");
      break;
    }
    jobfn_s.str(std::string());
    jobfn_s << qdir << "/" << ep->d_name;
    if (!is_file(jobfn_s.str())) continue;
    debug(jobfn_s.str().c_str());
    std::string jobfn = jobfn_s.str();
    std::ifstream fh(jobfn.c_str());
    std::string line;
    std::string script, output, env;
    std::string key, val;
    while (std::getline(fh, line)) {
      if (regexec(&rkv, line.c_str(), 3, m, 0) == 0) {
        key = line.substr(m[1].rm_so, m[1].rm_eo-m[1].rm_so);
        val = line.substr(m[2].rm_so, m[2].rm_eo-m[2].rm_so);
        if (key == "executable") script = val;
        else if (key == "output") output = val;
        else if (key == "environment") env = val;
      }
    }
    fh.close();
    if (script.empty() || output.empty()) {
      debug("file %s unknown, erasing", jobfn.c_str());
      unlink(jobfn.c_str());
    }
    else {
      if (unlink(jobfn.c_str())) {
        debug("cannot delete %s, not adding %s",
              jobfn.c_str(), script.c_str());
      }
      else {
        debug("adding script %s to the queue, output on %s",
              script.c_str(), output.c_str());
        struct work_queue_task *t = work_queue_task_create("env PATH=/bin:/usr/bin LD_LIBRARY_PATH= ./agent.sh > log 2>&1");
        work_queue_task_specify_file(t, script.c_str(), "agent.sh",
                                     WORK_QUEUE_INPUT, WORK_QUEUE_NOCACHE);
        work_queue_task_specify_file(t, output.c_str(), "log",
                                     WORK_QUEUE_OUTPUT, WORK_QUEUE_NOCACHE);
        work_queue_task_specify_cores(t, 1);
        work_queue_task_specify_max_retries(t, 0);

        size_t oldpos = 0;
        size_t pos = 0;
        size_t eq;
        env.append(";");
        while ((pos = env.find(';', pos)) != std::string::npos) {
          if (oldpos != pos) {
            std::string key = env.substr(oldpos, pos-oldpos);
            if ((eq = key.find('=', 0)) == std::string::npos) continue;
            std::string val = key.substr(eq+1);
            key.erase(eq);
            work_queue_task_specify_enviroment_variable(t, key.c_str(), val.c_str());
            debug("task environment: %s = %s", key.c_str(), val.c_str());
          }
          oldpos = ++pos;
        }

        work_queue_submit(q, t);
        taskids.push_back(t->taskid);
        debug("submitted task %d - output on %s", t->taskid, output.c_str());
      }
    }
  }
  closedir(dp);
}

bool loop(struct work_queue *q, std::vector<int> &taskids) {
  bool draining = (access("/tmp/aa/drain", F_OK) != -1);
  if (!draining) watch_queue(q, taskids);
  else debug("drain mode: not accepting new jobs");

  debug("waiting for tasks to finish");
  struct work_queue_task *t;
  time_t tbeg, tend;
  tbeg = time(NULL);
  int elap;
  while ((elap = (int)difftime(time(NULL), tbeg)) < AWQ_MAX_TASK_WAIT &&
         !work_queue_empty(q)) {
    write_stats(q);
    debug("waiting up to %d seconds for a task to complete", AWQ_WAIT_TASK);
    t = work_queue_wait(q, AWQ_WAIT_TASK);
    if (!t) continue;
    taskids.erase(std::remove(taskids.begin(), taskids.end(), t->taskid),
                  taskids.end());
    work_queue_task_delete(t);
    debug("one off, %llu left", taskids.size());
    tend = time(NULL);
  }

  write_stats(q);
  if (draining && work_queue_empty(q)) {
    debug("draining finished, exiting");
    return false;
  }

  if ((elap = AWQ_MAX_TASK_WAIT-elap) > 0) {
    debug("pausing %d more seconds", elap);
    sleep(elap);
  }

  return true;
}

int main(int argn, char *argv[]) {
  debug("starting alien-workqueue");

  struct work_queue *q = work_queue_create(9094);
  mkdir("/tmp/aa", AWQ_DIRMODE);
  mkdir("/tmp/aa/queue", AWQ_DIRMODE);
  debug("listening on port %d", work_queue_port(q));
  std::vector<int> taskids;

  while (loop(q, taskids));
  return 0;
}
