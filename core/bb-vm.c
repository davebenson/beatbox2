#include <string.h>
#include "bb-vm.h"
#include "bb-utils.h"
#include "bb-types.h"
#include "bb-error.h"

BbProgram *bb_program_new   (void)
{
  BbProgram *p = g_new (BbProgram, 1);
  p->ref_count = 1;
  p->n_instructions = 0;
  p->instructions_alloced = 4;
  p->instructions = g_new (BbInstruction, p->instructions_alloced);
  return p;
}

BbProgram *bb_program_ref   (BbProgram *program)
{
  g_return_val_if_fail (program->ref_count > 0, NULL);
  ++(program->ref_count);
  return program;
}
void       bb_program_unref (BbProgram *program)
{
  g_return_if_fail (program->ref_count > 0);
  --(program->ref_count);
  if (program->ref_count == 0)
    {
      guint i;
      for (i = 0; i < program->n_instructions; i++)
        {
          BbInstruction *in = program->instructions + i;
          switch (in->type)
            {
            case BB_INSTRUCTION_PUSH:
              g_value_unset (&in->info.push);
              break;
            case BB_INSTRUCTION_FUNCTION:
              bb_function_unref (in->info.function);
              break;
            case BB_INSTRUCTION_LAZY_FUNCTION:
              g_free (in->info.lazy_function);
              break;
            case BB_INSTRUCTION_PUSH_PARAM:
            case BB_INSTRUCTION_PUSH_SPECIAL:
            case BB_INSTRUCTION_POP:
            case BB_INSTRUCTION_BUILTIN:
            case BB_INSTRUCTION_FUNCTION_ARG_START:
              break;
            }
        }
      g_free (program->instructions);
      g_free (program);
    }
}
static void
ensure_space (BbProgram *p, guint space)
{
  guint min_req = space + p->n_instructions;
  guint new_alloced = p->instructions_alloced;
  if (new_alloced < min_req)
    {
      while (new_alloced < min_req)
        new_alloced += new_alloced;
      p->instructions = g_renew (BbInstruction, p->instructions, new_alloced);
      p->instructions_alloced = new_alloced;
    }
}
static BbInstruction *add_instructions_raw (BbProgram *p, guint n)
{
  ensure_space (p, n);
  p->n_instructions += n;
  return p->instructions + (p->n_instructions - n);
}

void       bb_program_add_push_param    (BbProgram    *program,
                                         guint         param_index)
{
  BbInstruction *in = add_instructions_raw (program, 1);
  in->type = BB_INSTRUCTION_PUSH_PARAM;
  in->info.push_param = param_index;
}

void       bb_program_add_push_special  (BbProgram    *program,
                                         BbVmSpecial   special)
{
  BbInstruction *in = add_instructions_raw (program, 1);
  in->type = BB_INSTRUCTION_PUSH_SPECIAL;
  in->info.push_special = special;
}

void       bb_program_add_pop           (BbProgram    *program)
{
  BbInstruction *in = add_instructions_raw (program, 1);
  in->type = BB_INSTRUCTION_POP;
}

void       bb_program_add_push          (BbProgram    *program,
                                         const GValue *value)
{
  BbInstruction *in = add_instructions_raw (program, 1);
  in->type = BB_INSTRUCTION_PUSH;
  memset (&in->info.push, 0, sizeof (GValue));
  g_value_init (&in->info.push, G_VALUE_TYPE (value));
  g_value_copy (value, &in->info.push);
}


void       bb_program_add_builtin       (BbProgram    *program,
                                         const char   *name,
                                         BbBuiltinFunc func)
{
  BbInstruction *in = add_instructions_raw (program, 1);
  in->type = BB_INSTRUCTION_BUILTIN;
  in->info.builtin.func = func;
  in->info.builtin.name = name;
}
void       bb_program_add_function_begin(BbProgram    *program)
{
  BbInstruction *in = add_instructions_raw (program, 1);
  in->type = BB_INSTRUCTION_FUNCTION_ARG_START;
}

void       bb_program_add_function      (BbProgram    *program,
                                         BbFunction   *function)
{
  BbInstruction *in = add_instructions_raw (program, 1);
  in->type = BB_INSTRUCTION_FUNCTION;
  in->info.function = bb_function_ref (function);
}

void       bb_program_add_lazy_function (BbProgram    *program,
                                         const char   *function_name)
{
  BbInstruction *in = add_instructions_raw (program, 1);
  in->type = BB_INSTRUCTION_LAZY_FUNCTION;
  in->info.lazy_function = g_strdup (function_name);
}

void       bb_program_append_program    (BbProgram    *program,
                                         BbProgram    *subprogram)
{
  guint n = subprogram->n_instructions;
  BbInstruction *in = add_instructions_raw (program, n);
  guint i;
  for (i = 0; i < n; i++)
    {
      in[i].type = subprogram->instructions[i].type;
      switch (in[i].type)
        {
        case BB_INSTRUCTION_PUSH_PARAM:
          in[i].info.push_param = subprogram->instructions[i].info.push_param;
          break;
        case BB_INSTRUCTION_PUSH:
          memset (&in[i].info.push, 0, sizeof (GValue));
          g_value_init (&in[i].info.push, G_VALUE_TYPE (&subprogram->instructions[i].info.push));
          g_value_copy (&subprogram->instructions[i].info.push, &in[i].info.push);
          break;
        case BB_INSTRUCTION_PUSH_SPECIAL:
          in[i].info.push_special = subprogram->instructions[i].info.push_special;
          break;
        case BB_INSTRUCTION_POP:
          break;
        case BB_INSTRUCTION_BUILTIN:
          in[i].info.builtin = subprogram->instructions[i].info.builtin;
          break;
        case BB_INSTRUCTION_FUNCTION_ARG_START:
          break;
        case BB_INSTRUCTION_FUNCTION:
          in[i].info.function = bb_function_ref (subprogram->instructions[i].info.function);
          break;
        case BB_INSTRUCTION_LAZY_FUNCTION:
          in[i].info.lazy_function = g_strdup (subprogram->instructions[i].info.lazy_function);
          break;
        default:
          g_error ("bb_program_append_program: unknown instruction %u",
                   subprogram->instructions[i].type);
        }
    }
}

BbVm *bb_vm_new  (guint         n_params,
                  const GValue *params,
                  gboolean      copy_params)
{
  BbVm *vm = g_new (BbVm, 1);
  vm->n_values = 0;
  vm->values_alloced = 8;
  vm->values = g_new (GValue, vm->values_alloced);
  vm->n_params = n_params;
  vm->recursion_depth = 0;
  vm->trace = FALSE;
  if (copy_params)
    {
      GValue *copy = g_new0 (GValue, n_params);
      guint i;
      for (i = 0; i < n_params; i++)
        {
          g_value_init (copy + i, G_VALUE_TYPE (params + i));
          g_value_copy (params + i, copy + i);
        }
      vm->params = params;
      vm->free_params = TRUE;
    }
  else
    {
      vm->params = params;
      vm->free_params = FALSE;
    }
  return vm;
}

static inline GValue *
push_zeroed (BbVm *vm)
{
  GValue *rv;
  if (G_UNLIKELY (vm->n_values == vm->values_alloced))
    {
      guint N = vm->values_alloced * 2;
      vm->values = g_renew (GValue, vm->values, N);
      vm->values_alloced = N;
    }
  rv = vm->values + vm->n_values++;
  memset (rv, 0, sizeof (GValue));
  return rv;
}
  
void  bb_vm_push (BbVm *vm, const GValue *value)
{
  GValue *new = push_zeroed (vm);
  g_value_init (new, G_VALUE_TYPE (value));
  g_value_copy (value, new);
}

void  bb_vm_push_special (BbVm *vm, BbVmSpecial special)
{
  GValue *new = push_zeroed (vm);
  new->data[0].v_int = special;
}

void  bb_vm_pop  (BbVm *vm, GValue *value)
{
  GValue *r;
  g_return_if_fail (vm->n_values > 0);
  r = vm->values + vm->n_values - 1;
  g_return_if_fail (r->g_type != 0);
  if (value)
    {
      if (value->g_type == 0)
        g_value_init (value, G_VALUE_TYPE (r));
      if (G_VALUE_TYPE (r) != G_VALUE_TYPE (value))
        g_error ("bb_vm_pop: popping type %s, but expected type %s",
                 G_VALUE_TYPE_NAME (r),
                 G_VALUE_TYPE_NAME (value));
      g_value_copy (r, value);
    }
  g_value_unset (vm->values + vm->n_values - 1);
  vm->n_values--;
}
void  bb_vm_pop_special  (BbVm *vm, BbVmSpecial *out)
{
  GValue *r;
  g_return_if_fail (vm->n_values > 0);
  r = vm->values + vm->n_values - 1;
  g_return_if_fail (r->g_type == 0);
  if (out)
    *out = r->data[0].v_int;
  vm->n_values--;
}
void     bb_vm_push_stack    (BbVm         *vm)
{
  BbVmStack *new = g_new (BbVmStack, 1);
  new->n_values = vm->n_values;
  new->values = vm->values;
  new->values_alloced = vm->values_alloced;
  new->up = vm->stack;
  vm->stack = new;
  vm->n_values = 0;
  vm->values_alloced = 8;
  vm->values = g_new (GValue, vm->values_alloced);
}
void     bb_vm_pop_stack     (BbVm         *vm)
{
  g_assert (vm->stack != NULL);

  {
    BbVmStack *up = vm->stack;
    guint n_values = vm->n_values;
    GValue *values = vm->values;
    guint i;
    vm->n_values = up->n_values;
    vm->values = up->values;
    vm->values_alloced = up->values_alloced;
    vm->stack = up->up;

    for (i = 0; i < n_values; i++)
      *push_zeroed (vm) = values[i];
    g_free (values);
    g_free (up);
  }
}
void
bb_instruction_dump (BbInstruction *insn,
                     GParamSpec   **pspecs)
{
  switch (insn->type)
    {
    case BB_INSTRUCTION_PUSH:
      {
        char *str = bb_value_to_string (&insn->info.push);
        g_print ("PUSH %s\n", str);
        g_free (str);
        break;
      }
    case BB_INSTRUCTION_PUSH_PARAM:
      {
        if (pspecs == NULL)
          g_print ("PUSH PARAM %u\n", insn->info.push_param);
        else
          g_print ("PUSH PARAM %s\n", pspecs[insn->info.push_param]->name);
        break;
      }
    case BB_INSTRUCTION_PUSH_SPECIAL:
      {
        g_print ("PUSH SPECIAL %u\n", insn->info.push_special);
        break;
      }
    case BB_INSTRUCTION_POP:
      {
        g_print ("POP\n");
        break;
      }
    case BB_INSTRUCTION_BUILTIN:
      {
        g_print ("CALL %s\n", insn->info.builtin.name);
        break;
      }
    case BB_INSTRUCTION_FUNCTION_ARG_START:
      {
        g_print ("FUNCTION_ARG_START\n");
        break;
      }
    case BB_INSTRUCTION_FUNCTION:
      {
        g_print ("FUNCTION(%s)\n", insn->info.function->name);
        break;
      }
    case BB_INSTRUCTION_LAZY_FUNCTION:
      {
        g_print ("LAZY_FUNCTION(%s)\n", insn->info.lazy_function);
        break;
      }

    default:
      g_assert_not_reached ();
    }
}

gboolean bb_vm_is_special (BbVm      *vm,
                           guint      depth,
                           BbVmSpecial *out)
{
  GValue *v;
  g_return_val_if_fail (depth < vm->n_values, FALSE);
  v = &vm->values[vm->n_values - depth - 1];
  if (v->g_type == 0)
    {
      if (out)
        *out = v->data[0].v_int;
      return TRUE;
    }
  return FALSE;
}

static gboolean
run_function (BbVm *vm, BbFunction *function, const BbRenderInfo *render_info, GError **error)
{
  GValue *out;
  guint n_out;
  gboolean must_free_out = FALSE;
  guint i;
  switch (function->type)
    {
    case BB_FUNCTION_TYPE_GENERAL:
      if (!function->func.general (vm->n_values, vm->values, 
                                   &n_out, &out, render_info, error))
        return FALSE;
      must_free_out = TRUE;
      break;

    case BB_FUNCTION_TYPE_SINGLE_OUTPUT:
      out = g_newa (GValue, 1);
      memset (out, 0, sizeof (GValue));
      if (!function->func.single_output (vm->n_values, vm->values,
                                         out, render_info, error))
        return FALSE;
      if (out->g_type == 0)
        n_out = 0;
      else
        n_out = 1;
      break;
    }
  for (i = 0; i < vm->n_values; i++)
    g_value_unset (vm->values + i);
  if (G_UNLIKELY (vm->values_alloced < n_out))
    {
      guint new_alloced = vm->values_alloced * 2;
      while (new_alloced < n_out)
        new_alloced += new_alloced;
      vm->values_alloced = new_alloced;
      vm->values = g_renew (GValue, vm->values, new_alloced);
    }
  memcpy (vm->values, out, n_out * sizeof (GValue));
  vm->n_values = n_out;
  if (must_free_out)
    g_free (out);
  bb_vm_pop_stack (vm);
  return TRUE;
}
gboolean bb_vm_run   (BbVm         *vm,
                      BbProgram    *program,
                      const BbRenderInfo *render_info,
                      GError      **error)
{
  guint i;
  guint depth = vm->recursion_depth;
  ++(vm->recursion_depth);
  for (i = 0; i < program->n_instructions; i++)
    {
      BbInstruction *insn = program->instructions + i;
      if (vm->trace)
        {
          g_print ("%*s", depth, "");
          bb_instruction_dump (insn, NULL);
        }
      switch (insn->type)
        {
        case BB_INSTRUCTION_PUSH:
          bb_vm_push (vm, &insn->info.push);
          break;
        case BB_INSTRUCTION_PUSH_PARAM:
          if (insn->info.push_param >= vm->n_params)
            {
              g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_INVALID_ARG,
                           "bad program requests param %u, n_params=%u",
                           insn->info.push_param, vm->n_params);
              goto fail;
           }
          bb_vm_push (vm, vm->params + insn->info.push_param);
          break;
        case BB_INSTRUCTION_PUSH_SPECIAL:
          bb_vm_push_special (vm, insn->info.push_special);
          break;
        case BB_INSTRUCTION_POP:
          if (vm->n_values == 0)
            {
              g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_INVALID_STATE,
                           "'pop' while stack empty");
              goto fail;
            }
          bb_vm_pop (vm, NULL);
          break;
        case BB_INSTRUCTION_BUILTIN:
          /* note: may re-enter bb_vm_run() */
          if (!insn->info.builtin.func (vm, render_info, error))
            goto fail;
          break;

        case BB_INSTRUCTION_FUNCTION_ARG_START:
          bb_vm_push_stack (vm);
          break;

        case BB_INSTRUCTION_FUNCTION:
          {
            if (!run_function (vm, insn->info.function, render_info, error))
              goto fail;
            break;
          }
        case BB_INSTRUCTION_LAZY_FUNCTION:
          {
            BbFunction *f;

            /* find the function to invoke */
            {
              GType *types = g_new (GType, vm->n_values);
              guint i;
              for (i = 0; i < vm->n_values; i++)
                types[i] = G_VALUE_TYPE (vm->values + i);
              f = bb_function_choose (insn->info.lazy_function, vm->n_values, types);
              g_free (types);
            }

            /* error if no function found */
            if (f == NULL)
              {
                g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_INVALID_ARG,
                             "no function named %s with appropriate signature",
                             insn->info.lazy_function);
                goto fail;
              }

            /* invoke the function */
            if (!run_function (vm, f, render_info, error))
              goto fail;
            break;
          }

        default:
          g_error ("no code written for instruction type %u", insn->type);
        }
    }
  --(vm->recursion_depth);
  return TRUE;

fail:
  --(vm->recursion_depth);
  return FALSE;
}

void  bb_vm_free (BbVm *vm)
{
  guint i;
  for (i = 0; i < vm->n_values; i++)
    g_value_unset (vm->values + i);
  g_free (vm->values);
  if (vm->free_params)
    {
      for (i = 0; i < vm->n_params; i++)
        g_value_unset ((GValue*)(vm->params + i));
      g_free ((gpointer)(vm->params));
    }
  g_free (vm);
}

void   bb_vm_run_get_value_or_die  (BbProgram *program,
                                    const BbRenderInfo *render_info,
                                    guint n_params,
                                    const GValue *params,
                                    GValue *value)
{
  BbVm *vm = bb_vm_new (n_params, params, FALSE);
  GError *error = NULL;
  if (!bb_vm_run (vm, program, render_info, &error))
    g_error ("error running program: %s", error->message);
  if (value == NULL)
    {
      if (vm->n_values != 0)
        g_error ("got unexpected value");
    }
  else
    {
      if (vm->n_values == 0)
        g_error ("got no return value");
      if (vm->n_values > 1)
        g_error ("got multiple return values");
      bb_vm_pop (vm, value);
    }
  bb_vm_free (vm);
}

double bb_vm_run_get_double_or_die (BbProgram *program,
                                    const BbRenderInfo *render_info,
                                    guint n_params,
                                    const GValue *params)
{
  GValue v = BB_VALUE_INIT;
  g_value_init (&v, G_TYPE_DOUBLE);
  bb_vm_run_get_value_or_die (program, render_info, n_params, params, &v);
  return g_value_get_double (&v);
}

gboolean bb_vm_parse_run_string_get_value (const char *str,
                                           const BbRenderInfo *render_info,
                                           GValue     *out,
                                           GError    **error)
{
  BbVm *vm;
  BbProgram *program = bb_program_parse (str, error);
  if (program == NULL)
    return FALSE;
  vm = bb_vm_new (0, NULL, FALSE);
  if (!bb_vm_run (vm, program, render_info, error))
    {
      bb_vm_free (vm);
      bb_program_unref (program);
      return FALSE;
    }
  bb_program_unref (program);
  if (vm->n_values == 0)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_INVALID_ARG,
                   "evaluating program led to no values (in run_string_get_value)");
      bb_vm_free (vm);
      return FALSE;
    }
  if (vm->n_values > 1)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_INVALID_ARG,
                   "evaluating program led to multiple values");
      bb_vm_free (vm);
      return FALSE;
    }
  bb_vm_pop (vm, out);		/* XXX: fatal error handling */
  return TRUE;
}

#if 0
static gboolean pop_double (BbVm *vm, gdouble *out, GError **error)
{
  GValue value;
  if (vm->n_values == 0)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_INVALID_STATE,
                   "pop_double: stack-empty");
      return FALSE;
    }
  memset (&value, 0, sizeof (value));
  bb_vm_pop (vm, &value);
  switch (G_VALUE_TYPE (&value))
    {
    case G_TYPE_DOUBLE:
      *out = g_value_get_double (&value);
      break;
    default:
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_INVALID_TYPE,
                   "expected double, got %s",
                   G_VALUE_TYPE_NAME (&value));
      return FALSE;
    }
  g_value_unset (&value);
  return TRUE;
}

static void
push_double (BbVm *vm, double v)
{
  GValue value;
  memset (&value, 0, sizeof (value));
  g_value_init (&value, G_TYPE_DOUBLE);
  g_value_set_double (&value, v);
  bb_vm_push (vm, &value);
  g_value_unset (&value);
}

gboolean bb_builtin_add (BbVm *vm, const BbRenderInfo *ri, GError **error)
{
  double a,b;
  if (!pop_double (vm, &a, error)
   || !pop_double (vm, &b, error))
    return FALSE;
  push_double (vm, a + b);
  return TRUE;
}
gboolean bb_builtin_sub (BbVm *vm, const BbRenderInfo *ri, GError **error)
{
  double a,b;
  if (!pop_double (vm, &a, error)
   || !pop_double (vm, &b, error))
    return FALSE;
  push_double (vm, b - a);
  return TRUE;
}
gboolean bb_builtin_mul (BbVm *vm, const BbRenderInfo *ri, GError **error)
{
  double a,b;
  if (!pop_double (vm, &a, error)
   || !pop_double (vm, &b, error))
    return FALSE;
  push_double (vm, a * b);
  return TRUE;
}
gboolean bb_builtin_div (BbVm *vm, const BbRenderInfo *ri, GError **error)
{
  double a,b;
  if (!pop_double (vm, &a, error)
   || !pop_double (vm, &b, error))
    return FALSE;
  push_double (vm, b / a);
  return TRUE;
}
gboolean bb_builtin_neg (BbVm *vm, const BbRenderInfo *ri, GError **error)
{
  double a;
  if (!pop_double (vm, &a, error))
    return FALSE;
  push_double (vm, -a);
  return TRUE;
}
#endif


gboolean bb_builtin_create_array (BbVm *vm, const BbRenderInfo *ri, GError **error)
{
  guint i;
  BbVmSpecial spec;
  for (i = 0; i < vm->n_values; i++)
    if (bb_vm_is_special (vm, i, &spec)
     && spec == BB_VM_SPECIAL_ARRAY_BEGIN)
      {
        GValue *array = g_new0 (GValue, i);
	guint j;
	GValue pushv = BB_VALUE_INIT;
	for (j = 0; j < i; j++)
	  bb_vm_pop (vm, &array[i-j-1]);
	bb_vm_pop_special (vm, &spec);

        /* setup pushv: it takes ownership of 'array' */
	g_value_init (&pushv, BB_TYPE_VALUE_ARRAY);
        g_value_take_boxed (&pushv, bb_value_array_new_take (i, array));

        /* add value to stack */
	bb_vm_push (vm, &pushv);

        /* clear value */
	g_value_unset (&pushv);
	
	return TRUE;
      }
  g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_INVALID_STATE,
	       "no array-begin mark");
  return FALSE;
}

/* XXX: just remove this facility.  it is unused */
static GHashTable *builtin_functions;
void          bb_builtin_register (const char   *function_name,
                                   BbBuiltinFunc func)
{
  if (builtin_functions == NULL)
    builtin_functions = g_hash_table_new (g_str_hash, g_str_equal);
  g_return_if_fail (g_hash_table_lookup (builtin_functions, function_name) == NULL);
  g_hash_table_insert (builtin_functions, (gpointer) function_name, (gpointer) func);
}
BbBuiltinFunc bb_builtin_lookup   (const char   *function_name)
{
  if (builtin_functions == NULL)
    return NULL;
  return g_hash_table_lookup (builtin_functions, function_name);
}
