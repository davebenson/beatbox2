#include <string.h>
#include "bb-function.h"

static GHashTable *sig_table = NULL;

static guint signature_hash (gconstpointer data)
{
  const BbFunctionSignature *sig = data;
  guint rv = 10003;
  guint i;
  for (i = 0; i < sig->n_inputs; i++)
    {
      rv *= 33;
      rv += sig->inputs[i];
    }
  for (i = 0; i < sig->n_outputs; i++)
    {
      rv *= 37;
      rv += sig->outputs[i];
    }
  return rv;
}
static gboolean signature_equal (gconstpointer a, gconstpointer b)
{
  const BbFunctionSignature *sa = a;
  const BbFunctionSignature *sb = b;
  return sa->n_inputs == sb->n_inputs
      && sa->n_outputs == sb->n_outputs
      && memcmp (sa->inputs, sb->inputs, sb->n_inputs * sizeof (GType)) == 0
      && memcmp (sa->outputs, sb->outputs, sb->n_outputs * sizeof (GType)) == 0;
}
BbFunctionSignature *bb_function_signature_new_v (guint        n_inputs,
                                                  const GType *inputs,
                                                  guint        n_outputs,
                                                  const GType *outputs)
{
  BbFunctionSignature *rv;
  if (sig_table == NULL)
    sig_table = g_hash_table_new (signature_hash, signature_equal);
  else
    {
      BbFunctionSignature dummy;
      dummy.n_inputs = n_inputs;
      dummy.n_outputs = n_outputs;
      dummy.inputs = (GType *) inputs;
      dummy.outputs = (GType *) outputs;
      dummy.ref_count = 0;
      rv = g_hash_table_lookup (sig_table, &dummy);
      if (rv)
        {
          rv->ref_count++;
          return rv;
        }
    }
  rv = g_new (BbFunctionSignature, 1);
  rv->n_inputs = n_inputs;
  rv->n_outputs = n_outputs;
  rv->inputs = g_memdup (inputs, n_inputs * sizeof (GType));
  rv->outputs = g_memdup (outputs, n_outputs * sizeof (GType));
  rv->ref_count = 2;    /* 1 to return, 1 for hash-table */
  g_hash_table_insert (sig_table, rv, rv);
  return rv;
}

BbFunctionSignature *bb_function_signature_new   (GType        first_input_type,
                                                  ...)
{
  guint n_inputs = 0, n_outputs = 0;
  GType t;
  GType *in, *out;
  va_list args;
  guint i;

  va_start (args, first_input_type);
  for (t = first_input_type; t != 0; t = va_arg (args, GType))
    n_inputs++;
  for (t = va_arg (args, GType); t != 0; t = va_arg (args, GType))
    n_outputs++;
  va_end (args);

  in = g_newa (GType, n_inputs);
  out = g_newa (GType, n_outputs);

  i = 0;
  va_start (args, first_input_type);
  for (t = first_input_type; t != 0; t = va_arg (args, GType))
    in[i++] = t;
  i = 0;
  for (t = va_arg (args, GType); t != 0; t = va_arg (args, GType))
    out[i++] = t;
  va_end (args);
  
  return bb_function_signature_new_v (n_inputs, in, n_outputs, out);
}

BbFunctionSignature *bb_function_signature_ref   (BbFunctionSignature *sig)
{
  g_assert (sig->ref_count > 1);
  sig->ref_count++;
  return sig;
}

void                 bb_function_signature_unref (BbFunctionSignature *sig)
{
  g_assert (sig->ref_count > 1);
  --(sig->ref_count);                   /* the hash-table always holds a ref anyways */
}

/* --- function registry --- */
static GHashTable *function_queue_by_name;
BbFunction *bb_function_choose            (const char          *name,
                                           guint                n_inputs,
                                           const GType         *inputs)
{
  GQueue *fq;
  GList *at;
  BbFunction *sigless = NULL;
  if (function_queue_by_name == NULL)
    return NULL;

  fq = g_hash_table_lookup (function_queue_by_name, name);
  if (fq == NULL)
    return NULL;

  for (at = fq->head; at != NULL; at = at->next)
    {
      BbFunction *f = at->data;
      if (f->sig != NULL
       && n_inputs == f->sig->n_inputs
       && memcmp (inputs, f->sig->inputs, f->sig->n_inputs * sizeof (GType)) == 0)
        return f;
      else if (f->sig == NULL)
        sigless = f;
    }
  return sigless;
}

static void
function_register (BbFunction *function)
{
  GQueue *queue;
  g_assert (function->name);
  if (function_queue_by_name == NULL)
    function_queue_by_name = g_hash_table_new (g_str_hash, g_str_equal);
  else
    {
      queue = g_hash_table_lookup (function_queue_by_name, function->name);
      if (queue)
        goto got_queue;
    }
  queue = g_queue_new ();
  g_hash_table_insert (function_queue_by_name, g_strdup (function->name), queue);

got_queue:
  g_queue_push_tail (queue, bb_function_ref (function));
}

/* --- constructors --- */
BbFunction *bb_function_new_single_output (const char          *name,
                                           BbFunctionSignature *sig,
                                           BbFunctionSingleFunc func)
{
  BbFunction *rv = g_new (BbFunction, 1);
  rv->type = BB_FUNCTION_TYPE_SINGLE_OUTPUT;
  rv->ref_count = 1;
  rv->name = g_strdup (name);
  rv->func.single_output = func;
  rv->sig = bb_function_signature_ref (sig);
  if (name)
    function_register (rv);
  return rv;
}
BbFunction *bb_function_new_general       (const char          *name,
                                           BbFunctionSignature *sig, /* optional */
                                           BbFunctionGeneralFunc func)
{
  BbFunction *rv = g_new (BbFunction, 1);
  rv->type = BB_FUNCTION_TYPE_GENERAL;
  rv->ref_count = 1;
  rv->name = g_strdup (name);
  rv->func.general = func;
  rv->sig = sig ? bb_function_signature_ref (sig) : NULL;
  if (name)
    function_register (rv);
  return rv;
}

BbFunction *
bb_function_ref               (BbFunction          *function)
{
  ++(function->ref_count);
  return function;
}
void
bb_function_unref             (BbFunction          *function)
{
  --(function->ref_count);
  if (function->ref_count == 0)
    {
      g_assert (function->name == NULL);
      if (function->sig)
        bb_function_signature_unref (function->sig);
      g_free (function);
    }
}
