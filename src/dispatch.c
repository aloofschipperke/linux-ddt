#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include "term.h"
#include "ccmd.h"
#include "files.h"
#include "jobs.h"
#include "user.h"

#define PREFIX_MAXBUF 255
#define SUFFIX_MAXBUF 255

static const char *prompt = "*";

static int altmodes;
static char prefix[PREFIX_MAXBUF+1];
static int nprefix;
static char character;
static int done;

static void (**fn) (void);
static void (*plain[256]) (void);
static void (*alt[256]) (void);

#define BELL 07
#define FORMFEED 014
#define CTRL_Q 021
#define ALTMODE 033
#define RUBOUT 0177
#define CTRL_(c)	((c)-64)

static void echo (int ch)
{
  switch (ch)
    {
    case ALTMODE: fputc ('$', stderr); break;
    default: fputc (ch, stderr); break;
    }
}

static void unknown (void)
{
  fprintf (stderr, "?\n");
  done = 1;
}

static void arg (void)
{
  if (nprefix < PREFIX_MAXBUF)
    {
      prefix[nprefix++] = character;
      prefix[nprefix] = 0;
    }
  else
    fputc(BELL, stderr);
}

static void altmode (void)
{
  altmodes++;
  fn = alt; 
}

static void rubout (void)
{
  if (!(altmodes || nprefix)) {
    fputs("?? ", stderr);
    return;
  }
  fprintf (stderr, "\010 \010");
  if (altmodes)
    {
      if (!--altmodes)
	fn = plain;
    }
  else
      nprefix--;
}

static char *suffix (void)
{
  static char string[SUFFIX_MAXBUF+1];
  int n = 0;
  char ch;

  string[0] = 0;

  while ((ch = term_read ()) != '\r')
    if (n < SUFFIX_MAXBUF)
      {
	if (isprint(ch))
	  {
	    echo (ch);
	    string[n++] = ch;
	    string[n] = 0;
	  }
	else
	  switch (ch)
	    {
	    case CTRL_Q:	/* quote next char */
	      if (n < SUFFIX_MAXBUF)
		{
		  fputs("^Q", stderr);
		  ch = term_read ();
		  fputs("\010 \010\010 \010", stderr);
		  echo (ch);
		  string[n++] = ch;
		  string[n] = 0;
		}
	      else
		fputc(BELL, stderr);
	      break;
	    case RUBOUT:
	      if (n)
		{
		  fprintf (stderr, "\010 \010");
		  string[--n] = 0;
		}
	      else
		return NULL;
	      break;
	    default:
	      fputc(BELL, stderr);
	    }
      }
    else
      fputc(BELL, stderr);

  while (n--)
    if (string[n] == ' ')
      string[n] = 0;
    else
      break;

  return string;
}

static void colon (void)
{
  if (!nprefix)
    {
      char *cmdline = suffix();
      if (cmdline != NULL)
	ccmd(cmdline);
      else			/* user rubbed out : */
	{
	  fprintf (stderr, "\010 \010");
	  return;
	}
    }
  else
    fprintf(stderr, "\r\nSymbol or block prefix: %s\r\n", prefix);

  done = 1;
}

static void login (void)
{
  if (altmodes > 1)
    logout (NULL);
  else
    login_as(prefix);
  done = 1;
}

static void raid (void)
{
  if (altmodes > 1)
    listj(NULL);
  else
    fprintf(stderr, "\r\na raid command %s\r\n", prefix);
  done = 1;
}

static void start (void)
{
  go(prefix);
  done = 1;
}

static void print_args (void)
{
  fprintf (stderr, "\n\rArgs: %s\r\n", prefix);
}

static void formfeed (void)
{
  clear(NULL);
  fputs (prefix, stderr);
}

static void job (void)
{
  if (altmodes > 1)
    {
      if (nprefix)
	set_currjname(prefix);
      else
	show_currjob(prefix);
      done = 1;
      return;
    }

  if (nprefix)
    select_job(prefix);
  else
    next_job();

  done = 1;
}

static void cont (void)
{
  contin(NULL);
}

static void proceed (void)
{
  proced(NULL);
}

void dispatch_init (void)
{
  int i;

  for (i = 0; i < 256; i++)
    {
      plain[i] = unknown;
      alt[i] = unknown;
    }

  for (i = 'a'; i <= 'z'; i++)
    {
      plain[i] = arg;
    }

  for (i = '0'; i <= '9'; i++)
    {
      plain[i] = arg;
      alt[i] = arg;
    }

  plain[FORMFEED] = formfeed;
  plain[CTRL_('P')] = proceed;
  plain[ALTMODE] = altmode;
  alt[ALTMODE] = altmode;
  plain[RUBOUT] = rubout;
  alt[RUBOUT] = rubout;

  plain[':'] = colon;
  alt[':'] = colon;
  alt['g'] = start;
  alt['j'] = job;
  alt['p'] = cont;
  alt['u'] = login;
  alt['v'] = raid;
  alt['?'] = print_args;
}

static void dispatch (int ch)
{
  done = 0;
  character = ch;
  fn[ch] ();
}

void prompt_and_execute (void)
{
  int ch;

  write (1, prompt, 1);
  altmodes = 0;
  nprefix = 0;
  fn = plain;

  do
    {
      ch = term_read ();
      echo (ch);
      dispatch (ch);
    }
  while (!done);
}
