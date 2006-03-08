#ifndef JOS_INC_SYS_RESOURCE_H
#define JOS_INC_SYS_RESOURCE_H

struct rlimit {
    rlim_t rlim_cur;
    rlim_t rlim_max;
};

struct rusage {
    struct timeval ru_utime;
    struct timeval ru_stime;
};

#define RUSAGE_SELF 0
#define RUSAGE_CHILDREN (-1)

enum __rlimit_resource
{
  RLIMIT_CPU = 0,
  RLIMIT_FSIZE = 1,
  RLIMIT_DATA = 2,
  RLIMIT_STACK = 3,
  RLIMIT_CORE = 4,
  RLIMIT_RSS = 5,
  RLIMIT_NPROC = 6,
  RLIMIT_NOFILE = 7,
  RLIMIT_OFILE = RLIMIT_NOFILE, /* BSD name for same.  */
  RLIMIT_MEMLOCK = 8,
  RLIMIT_AS = 9,
  RLIMIT_LOCKS = 10,
  RLIMIT_SIGPENDING = 11,
  RLIMIT_MSGQUEUE = 12,
  RLIMIT_NICE = 13,
  RLIMIT_RTPRIO = 14,
  RLIMIT_NLIMITS = 15,
  RLIM_NLIMITS = RLIMIT_NLIMITS
};

#define RLIM_INFINITY 0xffffffffffffffffUL

int getrlimit(int resource, struct rlimit *rlim);
int getrusage(int who, struct rusage *usage);
int setrlimit(int resource, const struct rlimit *rlim);

#endif
