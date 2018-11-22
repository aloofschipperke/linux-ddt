#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <termios.h>
#include <sys/utsname.h>
#include "jobs.h"
#include "term.h"

#define VERSION "0"

char *_runame = 0;
char *_xuname = 0;

char *runame(void)
{
  return _runame;
}

void version(char *unused)
{
  struct utsname luname = { 0 };
  char ttyname[32];

  uname(&luname);

  fprintf(stderr, "%s %s.%s DDT.%s.\r\n",
	  luname.nodename,
	  luname.sysname,
	  luname.release,
	  VERSION);
  if (!ttyname_r(0, ttyname, 32))
    fprintf(stderr, "%s\r\n", ttyname);
}

void sstatus(char *unused)
{
  double lavg[3];
  int n;
  fprintf(stderr, "%d Lusers, Load Avgs = ", 1);
  if (getloadavg(lavg, 3) == -1)
    fputs("unknown", stderr);
  else
    fprintf(stderr, "%.02f %.02f %.02f", lavg[0], lavg[1], lavg[2]);
  fputs("\r\n", stderr);
}

void greet(void)
{
  version(NULL);
  sstatus(NULL);
  fputs("\r\nFor brief information type :help\r\n"
	"For a list of colon commands, type :? and press Enter.\r\n"
	"\r\nHappy hacking!\r\n", stderr);
}

void outtest (char *ignore)
{
  massacre(NULL);
}

void logout (char *ignore)
{
  fputs("\r\n", stderr);

  if (_runame)
    outtest(NULL);

  exit (0);
}

void intest (char *ignore)
{
  fputs("\r\n", stderr);
}

void login_as (char *name)
{
  if (_runame)
    {
      fprintf(stderr, "\r\nAlready logged in.\r\n");
      return;
    }
  _runame = strdup(name);
  _xuname = strdup(name);
  clobrf = 1;
  genjfl = 1;

  intest(NULL);
}

void chuname (char *name)
{
  if (_runame)
    {
      outtest(NULL);
      free(_runame);
      _runame = 0;
      if (_xuname)
	{
	  free(_xuname);
	  _xuname = 0;
	}
    }
  clear(NULL);
  version(NULL);
  login_as(name);
}
