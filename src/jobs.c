#define _GNU_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <sys/ptrace.h>
#include <errno.h>
#include <termios.h>
#include "files.h"
#include "jobs.h"
#include "user.h"
#include "term.h"

#define MAXJOBS 8
#define MAXARGS 256

struct job jobs[MAXJOBS] = { 0 };
struct job *jobsend = &jobs[MAXJOBS];
struct job *currjob = 0;
struct job *fg = 0;

static char errstr[64];
static int pfd1[2], pfd2[2];

void jobs_init(void)
{
  if (pipe(pfd1) < 0 || pipe(pfd2) < 0) {
    fputs("failed creating pipes\r\n", stderr);
    exit(1);
  }
}

void errout(char *arg)
{
  int errno_ = errno;
  errstr[0] = 0;
  char *e = strerror_r(errno_, errstr, 64);
  if (arg)
    fprintf(stderr, " %s:", arg);
  fprintf(stderr, " %s\r\n", e);
}

static struct job *getjob(char *jname)
{
  for (struct job *j = jobs; j < jobsend; j++)
    if (j->state && strcmp(j->jname, jname) == 0)
      return j;
  return 0;
}

static char nextslot(void)
{
  for (char i = 0; i < MAXJOBS; i++)
    if (jobs[i].state != 0)
      return i;
  return -1;
}

static char getopenslot(void)
{
  for (char i = 0; i < MAXJOBS; i++)
    if (jobs[i].state == 0)
      return i;
  return -1;
}

void set_currjname (char *jname)
{
  if (currjob->jname) free(currjob->jname);
  currjob->jname = strdup(jname);
}

void show_currjob (char *arg)
{
  if (currjob)
    {
      if (*arg)
	set_currjname(arg);
      else
	list_currjob();
    }
  else if (*arg)
    fputs("Would set self name\r\n", stderr);
}

void list_currjob(void)
{
  if (currjob)
    fprintf(stderr, "\r\n%c %s %c %d\r\n",
	    '*',
	    currjob->jname, currjob->state, currjob->slot);
}

void next_job(void)
{
  for (struct job *j = jobs; j < jobsend; j++)
    if (j->state != 0 && j != currjob)
      {
	currjob = j;
	fprintf(stderr, " %s$j\r\n", currjob->jname);
	break;
      }
}

void listj(char *arg)
{
  fputs("\r\n", stderr);
  for (struct job *j = jobs; j < jobsend; j++)
    if (j->state != 0)
      fprintf(stderr, "%c %s %c %d\r\n",
	      (j != currjob)?' ':'*',
	      j->jname, j->state, j->slot);
}

void select_job(char *jname)
{
  struct job *j;
  char slot;
  if ((j = getjob(jname)))
    {
      currjob = j;
      fputs("\r\n", stderr);
      return;
    }

  if ((slot = getopenslot()) != -1)
    {
      j = &jobs[slot];
      j->jname = strdup(jname);
      j->jcl = NULL;
      j->state = '-';
      j->slot = slot;
      j->tmode = def_termios;
      j->proc.ufname.name = NULL;
      j->proc.ufname.devfd = -1;
      j->proc.ufname.dirfd = -1;
      j->proc.ufname.fd = -1;
      j->proc.argv = malloc(sizeof(char *) * 2);
      j->proc.argv[0] = NULL;
      j->proc.argv[1] = NULL;
      j->proc.env = malloc(sizeof(char *) * 2);
      j->proc.env[0] = NULL;
      j->proc.env[1] = NULL;
      j->proc.pid = 0;
      j->proc.status = 0;
      currjob = j;
      fputs("\r\n!\r\n", stderr);
    }
  else
    fprintf(stderr, " %d jobs already? ", MAXJOBS);
}

static void free_job(struct job *j)
{
  if (j->jname) free(j->jname);
  if (j->jcl) free(j->jcl);
  if (j->proc.ufname.name) free(j->proc.ufname.name);
  if (j->proc.argv) free(j->proc.argv);
  // if (j->proc.env) free(j->proc.env);
  j->jname = 0;
  j->jcl = 0;
  j->state = 0;
  j->proc.ufname.name = 0;
  j->proc.argv = 0;
}

void kill_job(struct job *j)
{
  int status;
  switch (j->state)
    {
    case '-':
      break;
    case '~':
    case 'p':
      ptrace(PTRACE_CONT, currjob->proc.pid, NULL, NULL);
    case 'r':
      errno = 0;
      if (kill(j->proc.pid, SIGTERM) == -1)
	errout("kill");
      else
      	if (waitpid(j->proc.pid, &status, WUNTRACED) == -1)
	  errout("waitpid");
      break;
    default:
      fputs("\r\nCan't do that yet.\r\n", stderr);
    }
  free_job(j);
}

void kill_currjob(char *arg)
{
  if (currjob)
    {
      kill_job(currjob);
      currjob = 0;
      char slot;
      if ((slot = nextslot()) != -1)
	{
	  currjob = &jobs[slot];
	  fprintf(stderr, "\r\n %s$j", currjob->jname);
	}
      fputs("\r\n", stderr);
    }
  else
    {
      fputs("\r\nPrompt login? here.\r\n", stderr);
    }
}

void massacre(char *arg)
{
  for (struct job *j = jobs; j < jobsend; j++)
    if (j->state)
      kill_job(j);
  currjob = 0;
}

void jclprt(char *notused)
{
  if (currjob)
    {
      char **argv = currjob->proc.argv;
      fputs("\r\n", stderr);
      argv++;
      while (*argv)
	{
	  fprintf(stderr, "%s ", *argv);
	  argv++;
	}
      fputs("\r\n", stderr);
    }
}

void jcl(char *argstr)
{
  if (!currjob)
    {
      fputs("\r\nTried to set self jcl\r\n", stderr);
      return;
    }

  if (currjob->jcl) free(currjob->jcl);
  if (currjob->proc.argv) {
    for (char **p = currjob->proc.argv; *p; p++)
      free(*p);
    free(currjob->proc.argv);
  }

  char *buf;
  if ((buf = (char *)malloc(strlen(argstr))) == NULL)
    {
      fputs("\r\nmalloc fail\r\n", stderr);
      return;
    }
  strcpy(buf, argstr);
  if ((currjob->proc.argv = malloc(MAXARGS * sizeof(char **))) == NULL)
    {
      fputs("\r\nmalloc fail\r\n", stderr);
      return;
    }

  int argc = 1;
  currjob->proc.argv[argc++] = strtok(buf, " \t");
  while (argc < MAXARGS
	 && ((currjob->proc.argv[argc] = strtok(NULL, " \t")) != NULL))
    currjob->proc.argv[++argc] == NULL;
  fputs("\r\n", stderr);
}

static inline int tell_parent(void)
{
  return (write(pfd2[1], "c", 1) != 1);
}

static inline int wait_parent(void)
{
  char	c;

  return ((read(pfd1[0], &c, 1) == 1) || (c == 'p'));
}

static inline int tell_child(void)
{
  return (write(pfd1[1], "p", 1) != 1);
}

static inline int wait_child(void)
{
  char	c;

  return ((read(pfd2[0], &c, 1) == 1) && (c == 'c'));
}

void child_load(void)
{
  tell_parent();
  wait_parent();

  errno = 0;
  if (ptrace(PTRACE_TRACEME, NULL, NULL, NULL) == -1) {
    errout("ptrace traceme");
    _exit(-1);
  }

  int fd;

  errno = 0;
  while ((fd = openat(AT_FDCWD, currjob->proc.ufname.name,
		      O_PATH | O_CLOEXEC | O_NOFOLLOW, O_RDONLY)) == -1)
    if (errno == EINTR)
      {
	errno = 0;
	continue;
      }
    else
      {
	errout("child openat");
	_exit(-1);
      }

  signal(SIGINT, SIG_DFL);
  signal(SIGQUIT, SIG_DFL);
  signal(SIGTSTP, SIG_DFL);
  signal(SIGTTIN, SIG_DFL);
  signal(SIGTTOU, SIG_DFL);
  signal(SIGCHLD, SIG_DFL);

  fexecve(fd, currjob->proc.argv, currjob->proc.env);
  errout("fexecve");
  _exit(-1);
}

void load_prog(char *name)
{
  if (!runame())
    {
      fputs("\r\n(Please Log In)\r\n\r\n:kill\r\n", stderr);
      return;
    }

  if (!currjob)
    {
      fputs("\r\nno current job\r\n", stderr);
      return;
    }

  if (currjob->state != '-')
    {
      fputs("\r\nJob already loaded\r\n", stderr);
      return;
    }

  if (currjob->proc.ufname.name) free(currjob->proc.ufname.name);
  currjob->proc.ufname.name = strdup(name);
  if (currjob->proc.argv[0]) free(currjob->proc.argv[0]);
  currjob->proc.argv[0] = strdup(name);

  errno = 0;
  pid_t childpid = fork();

  if (childpid == -1)
    {
      fputs("\r\nfork failed\r\n", stderr);
      return;
    }

  if (!childpid)
    child_load();

  tell_child();
  wait_child();

  int status = 0;
  waitpid(childpid, &status, 0);
  if (WIFEXITED(status))
    fprintf(stderr, "child exec failed. status=%d", WEXITSTATUS(status));
  else if (WIFSIGNALED(status))
    fprintf(stderr, "\r\nchild killed. signal=%d", WTERMSIG(status));
  else if (WIFSTOPPED(status))
    {
      currjob->proc.pid = childpid;
      setpgid(childpid, currjob->proc.pid);
      currjob->state = '~';
    }

  fputs("\r\n", stderr);
}

int fgwait(void)
{
  int status;
  
  if (!fg)
    return 0;
  
  waitpid(fg->proc.pid, &status, WUNTRACED);
  if (WIFEXITED(status))
    {
      if (WEXITSTATUS(status))
	fprintf(stderr, "\r\n:exit %d\r\n", WEXITSTATUS(status));
      free_job(fg);
    }
  else if (WIFSIGNALED(status))
    {
      fprintf(stderr, "\r\n:kill %d\r\n", WTERMSIG(status));
      free_job(fg);
    }
  else if (WIFSTOPPED(status))
    {
      fprintf(stderr, "\r\n%s stopped. signal=%d\r\n", fg->jname, WSTOPSIG(status));
      fg->state = 'p';
    }
  else
    fprintf(stderr, "\r\nfgwait status=%d\r\n", status);

  fg = 0;
  tcgetattr(0, &(currjob->tmode));
  tcsetpgrp(0, getpid());
  term_raw();

  return 1;
}

void check_jobs(void)
{
  pid_t child;
  int status;

  errno = 0;
  while ((child = waitpid(-1, &status, WNOHANG|WUNTRACED)) > 0)
    {
      for (struct job *j = jobs; j < jobsend; j++)
	{
	  if (j->proc.pid == child)
	    {
	      if (WIFEXITED(status))
		{
		  fprintf(stderr, ":exit %d %s$j\r\n", WEXITSTATUS(status), j->jname);
		  free_job(j);
		}
	      else if (WIFSIGNALED(status))
		{
		  fprintf(stderr, ":kill %d %s$j\r\n", WTERMSIG(status), j->jname);
		  free_job(j);
		}
	      else if (WIFSTOPPED(status))
		{
		  fprintf(stderr, "%s stopped. signal=%d\r\n", j->jname, WSTOPSIG(status));
		  j->state = 'p';
		}
	      else
		fprintf(stderr, "fgwait status=%d\r\n", status);

	      break;
	    }
	}
    }
  if (child == -1 && errno != ECHILD)
    {
      errout("checkjobs waitpid");
    }
}

void contin(char *unused)
{
  if (!currjob)
    fputs(" job? ", stderr);
  else
    switch (currjob->state)
      {
      case '-':
      case '~':
	fputs(" job never started? ", stderr);
	break;
      case 'p':
	currjob->state = 'r';
	ptrace(PTRACE_CONT, currjob->proc.pid, NULL, NULL);
      case 'r':
	tcsetpgrp(0, currjob->proc.pid);
	tcsetattr(0, TCSADRAIN, &(currjob->tmode));
	fg = currjob;
	break;
      default:
	fprintf(stderr, " unknown state %d? ", currjob->state);
      }
}

void proced(char *unused)
{
  fputs("\r\n", stderr);
  if (!currjob)
    fputs(" job? ", stderr);
  else
    switch (currjob->state)
      {
      case '-':
      case '~':
	fputs(" job never started? ", stderr);
	break;
      case 'p':
	currjob->state = 'r';
	ptrace(PTRACE_CONT, currjob->proc.pid, NULL, NULL);
      case 'r':
 	break;
      default:
	fprintf(stderr, " unknown state %d? ", currjob->state);
      }
}

void go(char *addr)
{
  fputs("\r\n", stderr);
  if (addr && *addr)
    fprintf(stderr, "Address Prefix for go: %s\r\n", addr);
  else if (!currjob)
    fputs(" job? ", stderr);
  else
    switch (currjob->state)
      {
      case '-':
	fputs(" no start addr? ", stderr);
	break;
      case '~':
      case 'p':
	tcsetpgrp(0, currjob->proc.pid);
	tcsetattr(0, TCSADRAIN, &(currjob->tmode));
	ptrace(PTRACE_CONT, currjob->proc.pid, NULL, NULL);
	fg = currjob;
	currjob->state = 'r';
	break;
      case 'r':
	tcsetpgrp(0, currjob->proc.pid);
	tcsetattr(0, TCSADRAIN, &(currjob->tmode));
	fg = currjob;
	break;
      default:
	fprintf(stderr, " unknown state %d? ", currjob->state);
      }
}

void gzp(char *addr)
{
  fputs("\r\n", stderr);
  if (addr && *addr)
    fprintf(stderr, "Address Prefix for gzp: %s\r\n", addr);
  else if (!currjob)
    fputs(" job? ", stderr);
  else
    switch (currjob->state)
      {
      case '-':
	fputs(" no start addr? ", stderr);
	break;
      case '~':
      case 'p':
	ptrace(PTRACE_CONT, currjob->proc.pid, NULL, NULL);
	currjob->state = 'r';
	break;
      case 'r':
	fputs(" already running? ", stderr);
	break;
      default:
	fprintf(stderr, " unknown state %d? ", currjob->state);
      }
}

void stop_currjob(void)
{
  if (currjob)
    {
      int status;
      errno = 0;
      if (kill(currjob->proc.pid, SIGSTOP) == -1)
	errout("sigstop");
      else
      	if (waitpid(currjob->proc.pid, &status, WUNTRACED) == -1)
	  errout("waitpid");
      currjob->state = 'p';
    }
  else
    fputs(" job? ", stderr);
}

