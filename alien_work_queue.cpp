#include <iostream>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <cstdio>
#include <cstring>
#include <cerrno>
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

#define debug(...) { fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); }
#define die(...) { fputs("FATAL: ", stderr); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); exit(1); }
#define AWQ_DIRMODE (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)
#define AWQ_WAIT_TASK 5
#define AWQ_MAX_TASK_WAIT 30
#define AWQ_JOB_LIMIT 10000
#define AWQ_DEFAULT_WORKDIR "/tmp/awq"

int is_file(const std::string &path) {
  static struct stat statbuf;
  if (stat(path.c_str(), &statbuf)) return false;
  return S_ISREG(statbuf.st_mode);
}

int is_dir(const std::string &path) {
  static struct stat statbuf;
  if (stat(path.c_str(), &statbuf)) return false;
  return S_ISDIR(statbuf.st_mode);
}

void write_stats(struct work_queue *q, std::string &work_dir) {
  static char buf[10];
  struct work_queue_stats stats;
  int index = 0;
  work_queue_get_stats(q, &stats);
  debug("WORKERS: init: %d / ready: %d / busy: %d",
        stats.workers_init, stats.workers_ready, stats.workers_busy);
  debug("TASKS: waiting: %d / complete: %d / running: %d",
        stats.tasks_waiting, stats.tasks_complete, stats.tasks_running);
  debug("writing stats");
  std::string stat_dir = work_dir+"/stat";
  if (!is_dir(stat_dir) && mkdir(stat_dir.c_str(), AWQ_DIRMODE)) {
    die("cannot create stat dir %s: %s", stat_dir.c_str(), strerror(errno));
  }
  std::ofstream of((stat_dir+"/tmp").c_str());
  of << "WAITING=" << stats.tasks_waiting << std::endl;
  of << "RUNNING=" << stats.tasks_running << std::endl;
  of << "TIMESTAMP=" << time(NULL) << std::endl;
  of.close();
  rename((stat_dir+"/tmp").c_str(), (stat_dir+"/latest").c_str());  // atomic
}

void watch_queue(struct work_queue *q, std::vector<int> &taskids, std::string &job_wrapper, std::string &work_dir) {
  debug("watching for new jobs in spool");
  std::string queue_dir = work_dir+"/queue";
  DIR *dp = opendir(queue_dir.c_str());
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
    jobfn_s << queue_dir << "/" << ep->d_name;
    if (!is_file(jobfn_s.str())) continue;
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
        struct work_queue_task *t = work_queue_task_create(job_wrapper.empty() ?
                                                           "./agent.sh > log 2>&1":
                                                           "./wrapper.sh ./agent.sh > log 2>&1");
        if (!job_wrapper.empty()) {
          work_queue_task_specify_file(t, job_wrapper.c_str(), "wrapper.sh",
                                       WORK_QUEUE_INPUT, WORK_QUEUE_NOCACHE);
        }
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

bool loop(struct work_queue *q, std::vector<int> &taskids, std::string &job_wrapper,
          std::string &work_dir) {
  bool draining = is_file(work_dir+"/drain");
  if (!draining) watch_queue(q, taskids, job_wrapper, work_dir);
  else debug("drain mode: not accepting new jobs");

  debug("waiting for tasks to finish");
  struct work_queue_task *t;
  time_t tbeg, tend;
  tbeg = time(NULL);
  int elap;
  while ((elap = (int)difftime(time(NULL), tbeg)) < AWQ_MAX_TASK_WAIT &&
         !work_queue_empty(q)) {
    write_stats(q, work_dir);
    debug("waiting up to %d seconds for a task to complete", AWQ_WAIT_TASK);
    t = work_queue_wait(q, AWQ_WAIT_TASK);
    if (!t) continue;
    taskids.erase(std::remove(taskids.begin(), taskids.end(), t->taskid),
                  taskids.end());
    work_queue_task_delete(t);
    debug("one off, %lu left", (unsigned long)taskids.size());
    tend = time(NULL);
  }

  write_stats(q, work_dir);
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

  std::string job_wrapper;
  std::string work_dir = AWQ_DEFAULT_WORKDIR;
  {
    std::string curr, next;
    for (int i=1; i<argn; i++) {
      curr = argv[i];
      next = i+1 < argn ? argv[i+1] : "";
      if ((curr == "--job-wrapper") && !next.empty()) {
        job_wrapper = next; i++;
      }
      else if ((curr == "--work-dir") && !next.empty()) {
        work_dir = next; i++;
      }
      else die("argument %s unrecognized", curr.c_str());
    }
  }

  if (job_wrapper.empty()) debug("using job wrapper %s", job_wrapper.c_str());
  debug("using workdir %s", work_dir.c_str());

  struct work_queue *q = work_queue_create(9094);
  if (!is_dir(work_dir) && mkdir(work_dir.c_str(), AWQ_DIRMODE)) {
    die("cannot create workdir %s: %s", work_dir.c_str(), strerror(errno));
  }
  std::string queue_dir = work_dir+"/queue";
  if (!is_dir(queue_dir) && mkdir(queue_dir.c_str(), AWQ_DIRMODE)) {
    die("cannot create queue dir %s: %s", queue_dir.c_str(), strerror(errno));
  }
  debug("listening on port %d", work_queue_port(q));
  std::vector<int> taskids;

  while (loop(q, taskids, job_wrapper, work_dir));
  return 0;
}
