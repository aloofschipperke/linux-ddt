#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include "aeval.h"

// TODO: jobs.h needs to include termios.h
#include <termios.h>
// TODO: errout needs to be moved out of jobs.c
#include "jobs.h"

int base = 10;

char *evalfactor(char *expr, uint64_t *value)
{
  if (*expr == '-')
    {
      expr++;
      expr = evalexpr(expr, value);
      *value = -*value;
    }
  else if (isdigit(*expr))
    {
      uint64_t v;
      char *end = expr;
      errno = 0;
      v = strtoull(expr, &expr, base);
      if (errno)
	errout("bad int");
      *value = v;
    }
  else
    expr = NULL;

  return expr;
}

static char *logictail(char *expr, uint64_t *value)
{
  uint64_t result = 0;
  switch (*expr)
    {
    case '#':
      if ((expr = evalfactor(++expr, &result)) == NULL)
	return expr;
      *value = *value ^ result;
      break;
    case '&':
      if ((expr = evalfactor(++expr, &result)) == NULL)
	return expr;
      *value = *value & result;
      break;
    case '|':
      if ((expr = evalfactor(++expr, &result)) == NULL)
	return expr;
      *value = *value | result;
      break;
    default:
      return expr;
    }
  return logictail(expr, value);
}

char *evallogic(char *expr, uint64_t *value)
{
  uint64_t result = 0;
  
  if ((expr = evalfactor(expr, &result)) == NULL)
    return expr;
  if ((expr = logictail(expr, &result)) == NULL)
    return expr;

  *value = result;

  return expr;
}

static char *termtail(char *expr, uint64_t *value)
{
  uint64_t result = 0;
  switch ((unsigned char)*expr)
    {
    case '*':
      if ((expr = evallogic(++expr, &result)) == NULL)
	return expr;
      *value = *value * result;
      break;
    case '!':
      if ((expr = evallogic(++expr, &result)) == NULL)
	return expr;
      *value = *value / result;
      break;
    case '*' + 0x80:
      if ((expr = evallogic(++expr, &result)) == NULL)
    	return expr;
      *value = (uint64_t) ((double)(*value) * (double)result);
      break;
    default:
      return expr;
    }
  return termtail(expr, value);
}

char *evalterm(char *expr, uint64_t *value)
{
  uint64_t result = 0;
  
  if ((expr = evallogic(expr, &result)) == NULL)
    return expr;
  if ((expr = termtail(expr, &result)) == NULL)
    return expr;

  *value = result;

  return expr;
}

static char *exprtail(char *expr, uint64_t *value)
{
  uint64_t result = 0;
  switch (*expr)
    {
    case '+':
      if ((expr = evalterm(++expr, &result)) == NULL)
	return expr;
      *value = *value + result;
      break;
    case '-':
      if ((expr = evalterm(++expr, &result)) == NULL)
	return expr;
      *value = *value - result;
      break;
    default:
      return expr;
    }
  return exprtail(expr, value);
}

char *evalexpr(char *expr, uint64_t *value)
{
  uint64_t result = 0;

  if ((expr = evalterm(expr, &result)) == NULL)
    return expr;
  if ((expr = exprtail(expr, &result)) == NULL)
    return expr;

  *value = result;

  return expr;
}
