#include <stdlib.h>
#include <termios.h>
#include "term.h"
#include "dispatch.h"
#include "jobs.h"
#include "files.h"

static void cleanup (void)
{
  term_restore ();
}

int main (int argc, char **argv)
{
  files_init();
  term_init ();
  atexit (cleanup);
  jobs_init ();
  dispatch_init ();

  for (;;)
    if (!fgwait())
      {
	check_jobs();
	prompt_and_execute ();
      }

  return 0;
}
