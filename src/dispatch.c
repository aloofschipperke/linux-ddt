/*
SPDX-License-Identifier: GPL-3.0-or-later

This file is part of Linux-ddt.

Linux-ddt is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation, either version 3 of the License, or (at
your option) any later version.

Linux-ddt is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Linux-ddt. If not, see <https://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "term.h"
#include "ccmd.h"
#include "jobs.h"
#include "user.h"
#include "debugger.h"
#include "aeval.h"
#include "typeout.h"

#define PREFIX_MAXBUF 255
#define SUFFIX_MAXBUF 255

const char *prompt = "*";
int monmode;

static char prefix[PREFIX_MAXBUF+1];
static char arg4str[PREFIX_MAXBUF+1];
static int nprefix;
static int narg4;
static char character;
static int done;
static int altmodes;

static void (**fn) (void);
static void (*plain[256]) (void);
static void (*alt[256]) (void);

#define BELL 07
#define BACKSPACE 010
#define FORMFEED 014
#define ALTMODE 033
#define RUBOUT 0177
#define CTRL_(c)	((c)-64)

static void echo (int ch)
{
  switch (ch)
    {
    case ALTMODE: fputc ('$', stderr); break;
    case '\r': fputc ('\r', stderr); fputc ('\n', stderr); break;
    default:
      if (iscntrl(ch))
	{
	  fputc('^', stderr);
	  fputc(ch + 64, stderr);
	}
      else
	fputc (ch, stderr);
      break;
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

static void altarg (void)
{
  if (altmodes == 1
      && nprefix < PREFIX_MAXBUF
      && narg4 == 0)
    {
      prefix[nprefix++] = character | 0x80;
      prefix[nprefix] = 0;
      altmodes = 0;
      fn = plain;
    }
  else
    fputc(BELL, stderr);
}

static void amper (void)
{
  if (!currjob)
    {
      fputs(" job? ", stderr);
      return;
    }

  if (nprefix)
    arg();
  else
    currjob->tamper(qreg);
}

static void nmsgn (void)
{
  if (nprefix)
    arg();
  else
    if (currjob)
      currjob->tnmsgn(qreg);
    else
      mnmsgn(qreg);
}

static void arg4 (void)
{
  if (narg4 < PREFIX_MAXBUF)
    {
      arg4str[narg4++] = character;
      arg4str[narg4] = 0;
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
  if (altmodes)
    {
      if (narg4)
	arg4str[--narg4] = 0;
      else if (!--altmodes)
	fn = plain;
    }
  else if (nprefix)
    {
      if (prefix[--nprefix] & 0x80)
	fputs ("\010 \010", stderr);
      if (iscntrl(prefix[nprefix]))
	fputs ("\010 \010", stderr);
      prefix[nprefix] = 0;
    }
  else {
    fputs("?? ", stderr);
    return;
  }
  fputs ("\010 \010", stderr);
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
	    case CTRL_('Q'):	/* quote next char */
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
	ccmd(cmdline, altmodes);
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
    {
      login_as(prefix);
      genjfl = clobrf = 0;
    }
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
  for (int i = 0; prefix[i]; i++)
    {
      if (prefix[i] & 0x80)
	fputc('$', stderr);
      if (iscntrl(prefix[i]))
	{
	  fputc('^', stderr);
	  fputc((prefix[i] & 0x7f) + 64, stderr);
	}
      else
	fputc(prefix[i] & 0x7f, stderr);
    }
  if (altmodes > 1)
    fputc('$', stderr);
  if (altmodes)
    fputc('$', stderr);
  if (narg4)
    fputs (arg4str, stderr);
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
  done = 1;
}

static void proceed (void)
{
  proced(NULL);
  done = 1;
}

static void stop (void)
{
  fputs("\r\n", stderr);
  if (altmodes > 1)
    {
      massacre(NULL);
    }
  else if (altmodes == 1)
    {
      if (nprefix)
	fprintf(stderr, "Would $^x with %s\r\n", prefix);
      else
	kill_currjob(NULL);
    }
  else
    {
      stop_currjob();
      return;
    }
  done = 1;
}

void asuser (void)
{
  if (altmodes > 1)
    {
      cwd(prefix);
    }
  else
    fprintf(stderr, "\r\nWould cause next command to run as user: %s\r\n", prefix);
  done = 1;
}

void backspace (void)
{
  if (altmodes)
    {
      fputs("\r\nWould $^h\r\n", stderr);
      done = 1;
      return;
    }
  if (nprefix)
    {
      fprintf(stderr, "\r\nWould ^h with %s\r\n", prefix);
      done = 1;
      return;
    }
  struct job *j = currjob;
  next_job();
  if (currjob != j)
    contin(NULL);
}

static void resetargs (void)
{
  altmodes = 0;
  prefix[0] = nprefix = 0;
  arg4str[0] = narg4 = 0;
  fn = plain;
}

void flushin (void)
{
  fputs(" xxx? ", stderr);
  resetargs();
}

void load (void)
{
  fputs(" ", stderr);
  char *cmdline = suffix();
  if (cmdline != NULL)
    {
      while (*cmdline == ' ')
	cmdline++;
      load_prog(cmdline);
    }
  else
    fputs("?? ", stderr);
  done = 1;
}

void kreat (void)
{
  if (nprefix)
    run_(prefix, NULL, 0, altmodes);
  else if (altmodes == 1)
    {
      load_symbols(currjob);
      fputs("\r\n", stderr);
    }
  else
    fputs("?? ", stderr);
  done = 1;
}

void print (void)
{
  print_file(prefix);
  done = 1;
}

void files (void)
{
  if (altmodes > 1)
    fprintf(stderr, "\r\n Would do hairy list of cwd\r\n");
  else if (altmodes)
    list_files(prefix, 0);
  else
    list_files(prefix, 1);
  done = 1;
}

void equal (void)
{
  if (nprefix)
    {
      uint64_t n = 0;
      char *r;
      if ((r = evalexpr(prefix, &n))
	  && !*r)
	qreg = n;
      else
	{
	  fputs("?? ", stderr);
	  goto leave;
	}
    }
  if (altmodes)
    tmf(qreg);
  else
    tmc(qreg);

 leave:
  resetargs();
}

static void radix8 (void)
{
  altmodes--;
  setradix(8, altmodes);
  if (altmodes)
    {
      fputs("   ", stderr);
      altmodes = 0;
    }
  fn = plain;
}

static void radix10 (void)
{
  altmodes--;
  setradix(10, altmodes);
  if (altmodes)
    {
      fputs("   ", stderr);
      altmodes = 0;
    }
  fn = plain;
}

static void radix16 (void)
{
  altmodes--;
  setradix(16, altmodes);
  if (altmodes)
    {
      fputs("   ", stderr);
      altmodes = 0;
    }
  fn = plain;
}

static void settmc (void)
{
  if (altmodes--)
      fputs("   ", stderr);

  settypeo(tmc, altmodes);

  altmodes = 0;
  fn = plain;
}

static void settmf (void)
{
  if (altmodes--)
      fputs("   ", stderr);

  settypeo(tmf, altmodes);

  altmodes = 0;
  fn = plain;
}

static void settmh (void)
{
  if (altmodes--)
      fputs("   ", stderr);

  settypeo(tmh, altmodes);

  altmodes = 0;
  fn = plain;
}

static void chquote (void)
{
  character = term_read();
  if (nprefix < PREFIX_MAXBUF)
    {
      prefix[nprefix++] = character;
      prefix[nprefix] = 0;
      echo (character);
    }
}

static void step (void)
{
  if (!currjob)
    {
      fputs(" job? ", stderr);
      return;
    }

  switch (currjob->state)
    {
    case 'r':
      fputs(" job running? ", stderr);
      break;
    case '~':
      fputs(" not started? ", stderr);
      break;
    case 'p':
      fputs("\r\n", stderr);
      step_job(currjob);
      typeout_pc(currjob);
      break;
    default:
      fputs(" not appropriate? ", stderr);
    }
}

void opennum (void)
{
  uint64_t n;
  if (nprefix)
    {
      char *r;
      if ((r = evalexpr(prefix, &n))
	  && *r)
	{
	  fputs("?? ", stderr);
	  goto leave;
	}
    }
  else
    n = qreg;

  if (openlocation(currjob ? currjob->proc.pid : 0, n))
    {
      fputs("   ", stderr);
      tmc(qreg);
    }

 leave:
  resetargs();
}

void carret (void)
{
  if (nprefix)
    fprintf(stderr, "\r\nWould deposit arg\r\n");

  closelocation();
  resettypeo();
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

  for (i = 'A'; i <= 'Z'; i++)
    {
      plain[i] = arg;
    }

  for (i = '0'; i <= '9'; i++)
    {
      plain[i] = arg;
      alt[i] = arg4;
    }


  plain[CTRL_('D')] = flushin;
  plain[CTRL_('F')] = files;
  plain[BACKSPACE] = backspace;
  plain[CTRL_('K')] = kreat;
  alt[CTRL_('K')] = kreat;
  plain[FORMFEED] = formfeed;
  alt[FORMFEED] = formfeed;
  plain[CTRL_('M')] = carret;
  plain[CTRL_('N')] = step;
  plain[CTRL_('P')] = proceed;
  plain[CTRL_('Q')] = chquote;
  plain[CTRL_('R')] = print;
  alt[CTRL_('S')] = asuser;
  plain[CTRL_('X')] = stop;
  alt[CTRL_('X')] = stop;
  plain[ALTMODE] = altmode;
  alt[ALTMODE] = altmode;
  plain[RUBOUT] = rubout;
  alt[RUBOUT] = rubout;

  alt[' '] = altarg;

  plain['*'] = arg;
  plain['+'] = arg;
  plain[','] = arg;
  plain['-'] = arg;
  plain['.'] = arg;
  plain['!'] = arg;
  alt['*'] = altarg;
  alt['+'] = altarg;
  alt[','] = altarg;
  alt['-'] = altarg;
  alt['.'] = altarg;
  alt['!'] = altarg;
  plain['#'] = nmsgn;
  plain['&'] = amper;

  plain['['] = opennum;

  plain[':'] = colon;
  alt[':'] = colon;
  plain['='] = equal;
  alt['='] = equal;

  alt['c'] = settmc;
  alt['d'] = radix10;
  alt['f'] = settmf;
  alt['g'] = start;
  alt['h'] = settmh;
  alt['j'] = job;
  alt['l'] = load;
  alt['o'] = radix8;
  alt['p'] = cont;
  alt['u'] = login;
  alt['v'] = raid;
  alt['X'] = radix16;
  alt['?'] = print_args;

  monmode = 0;
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

  fputs(prompt, stderr);
  if (monmode)
    {
      char *cmdline = suffix();
      if (cmdline != NULL)
	{
	  ccmd(cmdline, 0);
	  return;
	}
      else
	fputs("\010 \010", stderr);
    }
  resetargs();
  resetradix();

  do
    {
      ch = term_read ();
      echo (ch);
      dispatch (ch);
    }
  while (!done);
}
