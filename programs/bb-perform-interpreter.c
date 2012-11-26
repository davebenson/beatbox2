#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <gsk/gskutils.h>
#include <gsk/gskerrno.h>
#include <sys/stat.h>
#include "bb-perform-interpreter.h"

static char *bb_interpreter_bb_args[] =
{
  "./bb-run",
  "--raw",
  "-o",
  "$OUTPUT",
  "-r",
  "$SAMPLING_RATE",
  "$INPUT",
  NULL
};
static char *bb_interpreter_mub_args[] =
{
  "./programs/bb-mub-wrapper",
  "$INPUT",
  "$OUTPUT",
  NULL
};


static BbPerformInterpreter _bb_interpreters[2] =
{
  {
    "BB",
    bb_interpreter_bb_args
  },
  {
    "MUB",
    bb_interpreter_mub_args
  },
};


BbPerformInterpreter *bb_perform_interpreters = _bb_interpreters;
guint                 bb_perform_interpreters_count = G_N_ELEMENTS (_bb_interpreters);

BbPerformInterpreter *
bb_perform_interpreter_find_by_name (const char *name)
{
  guint i;
  for (i = 0; i < bb_perform_interpreters_count; i++)
    if (strcmp (bb_perform_interpreters[i].name, name) == 0)
      return bb_perform_interpreters + i;
  return NULL;
}

GPid   bb_perform_interpreter_launch   (BbPerformInterpreter *interpreter,
                                        const char           *input_name,
                                        const char           *output_name,
                                        gdouble               sampling_rate)
{
  guint n_args;
  char **args;
  char srate_buf[255];
  int rv;
  guint i;
  for (n_args = 0; interpreter->args[n_args] != NULL; n_args++)
    ;
  args = g_newa (char *, n_args + 1);
  g_snprintf (srate_buf, sizeof (srate_buf), "%g", sampling_rate);
  for (i = 0; i < n_args; i++)
    if (strcmp (interpreter->args[i], "$INPUT") == 0)
      args[i] = (char*) input_name;
    else if (strcmp (interpreter->args[i], "$OUTPUT") == 0)
      args[i] = (char*) output_name;
    else if (strcmp (interpreter->args[i], "$SAMPLING_RATE") == 0)
      args[i] = srate_buf;
    else
      args[i] = interpreter->args[i];
  args[i] = NULL;
  while ((rv = fork ()) < 0)
    {
      if (gsk_errno_is_ignorable (rv))
        continue;
      g_error ("error forking: %s", g_strerror (errno));
    }
  if (rv == 0)
    {
      /* child process */
      execvp (args[0], args);
      g_error ("error execing %s: %s", args[0], g_strerror (errno));
      _exit(127); /* just in case? */
    }
  return rv;
}
