#include <stdio.h>
#include "../core/bb-vm.h"
#include "../core/bb-init.h"

int main(int argc, char **argv)
{
  BbProgram *prog;
  GPtrArray *pspecs;
  GError *error = NULL;
  guint i;
  bb_init ();
  if (argc != 2)
    g_error ("usage: test-compile-expr EXPR");
  pspecs = g_ptr_array_new ();
  prog = bb_program_parse (argv[1], pspecs, &error);
  if (prog == NULL)
    g_error ("error parsing program: %s", error->message);
  for (i = 0; i < pspecs->len; i++)
    g_print ("param %2u: %s: %s\n", i, ((GParamSpec*)pspecs->pdata[i])->name, g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspecs->pdata[i])));
  for (i = 0; i < prog->n_instructions; i++)
    bb_instruction_dump (prog->instructions + i, (GParamSpec **) pspecs->pdata);
  bb_program_unref (prog);
  return 0;
}
