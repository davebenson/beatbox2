#include <string.h>
#include <stdlib.h>
#include <glib-object.h>
#include "../core/bb-input-file.h"
#include "../core/macros.h"
#include "../core/bb-init.h"
#include "../core/bb-types.h"
#include "../core/bb-duration.h"

#include "../core/bb-filter1.h"
#include "../core/bb-waveform.h"

static void
usage (void)
{
  g_printerr ("usage: make-bindings OPTIONS BINDING_DATA\n");
  exit (1);
}

typedef struct _TypeInfo TypeInfo;
struct _TypeInfo
{
  GType type;
  char *type_macro;
  char *c_type;
  const char *get_value;
  const char *set_value;
  const char *init_value;
  const char *cleanup_func;
};


static GQuark type_info_quark;
#define get_type_info(type)   ((TypeInfo*)(g_type_get_qdata(type, type_info_quark)))

static FILE *output_functions, *output_registration;

void
register_type_full (GType type,
                    const char *macro_name,
                    const char *ctype_name,
                    const char *value_get_func_name,
                    const char *value_set_func_name,
                    const char *init_value,
                    const char *cleanup_func_name)
{
  TypeInfo *type_info;
  g_assert (get_type_info (type) == NULL);
  type_info = g_new (TypeInfo, 1);
  type_info->type = type;
  type_info->type_macro = g_strdup (macro_name);
  type_info->c_type = g_strdup (ctype_name);
  type_info->get_value = value_get_func_name;
  type_info->set_value = value_set_func_name;
  type_info->init_value = init_value;
  type_info->cleanup_func = cleanup_func_name;
  g_type_set_qdata (type, type_info_quark, type_info);
}

static char *
name_to_get_type_macro (const char *name)
{
  const char *at;
  GString *get_type = g_string_new ("");
  g_assert ('A' <= name[0] && name[0] <= 'Z');
  g_string_append_c (get_type, name[0]);
  for (at = name + 1; *at != 0 && !('A' <= *at && *at <= 'Z'); at++)
    g_string_append_c (get_type, g_ascii_toupper (*at));
  g_string_append (get_type, "_TYPE");
  while (*at)
    {
      if ('A' <= *at && *at <= 'Z')
        g_string_append_c (get_type, '_');
      g_string_append_c (get_type, g_ascii_toupper (*at));
      at++;
    }
  return g_string_free (get_type, FALSE);
}

void register_type_boxed (GType       type,
                          const char *cleanup_func)
{
  const char *name = g_type_name (type);
  char *mac = name_to_get_type_macro (name);
  char *ct = g_strdup_printf ("%s *", name);
  register_type_full (type, mac, ct, "g_value_get_boxed", "g_value_set_boxed", "NULL", cleanup_func);
  g_free (mac);
  g_free (ct);
}
void
register_type_enum (GType type)
{
  const char *name = g_type_name (type);
  char *mac = name_to_get_type_macro (name);
  register_type_full (type, mac, name, "g_value_get_enum", "g_value_set_enum", "0", NULL);
  g_free (mac);
}

void register_type_object (GType type)
{
  const char *name = g_type_name (type);
  char *mac = name_to_get_type_macro (name);
  char *ct = g_strdup_printf ("%s *", name);
  register_type_full (type, mac, ct, "g_value_get_object", "g_value_set_object", "NULL", "g_object_unref");
  g_free (mac);
  g_free (ct);
}

/* returns the mangled name */
typedef struct _Param Param;
struct _Param
{
  GType type;
  char *name;
};
static char *
get_mangled_name (const char *function_name,
                  guint       n_inputs,
                  Param      *inputs,
                  guint       n_outputs,
                  Param      *outputs)
{
  GString *fname = g_string_new (function_name);
  guint i;
  g_string_append_c (fname, '_');
  for (i = 0; i < n_inputs; i++)
    {
      g_string_append_c (fname, '_');
      g_string_append (fname, g_type_name (inputs[i].type));
    }
  g_string_append_c (fname, '_');
  for (i = 0; i < n_outputs; i++)
    {
      g_string_append_c (fname, '_');
      g_string_append (fname, g_type_name (outputs[i].type));
    }
  return g_string_free (fname, FALSE);
}

static void
emit_function (const char *function_name,
               guint       n_inputs,
               Param      *inputs,
               guint       n_outputs,
               Param      *outputs,
	       const char *code)
{
  char *mangled_name = get_mangled_name (function_name, n_inputs, inputs, n_outputs, outputs);
  guint i;
  if (n_outputs <= 1)
    {
      fprintf (output_functions,
               "static gboolean %s\n"
               "\t(guint n_inputs, const GValue *inputs, GValue *output, const BbRenderInfo *render_info, GError **error)\n",
               mangled_name);
    }
  else
    {
      fprintf (output_functions,
               "static gboolean %s\n"
               "\t(guint n_inputs, const GValue *inputs, guint *n_outputs_out, GValue **outputs_out, const BbRenderInfo *render_info, GError **error)\n",
               mangled_name);
    }
  fprintf (output_functions, "{\n");
  fprintf (output_functions, "#define CLEANUP()  G_STMT_START{ \\\n");
  for (i = 0; i < n_outputs; i++)
    {
      TypeInfo *ti = get_type_info (outputs[i].type);
      if (ti == NULL)
        g_error ("no type-info found for %s", g_type_name (outputs[i].type));
      if (ti->cleanup_func)
        fprintf (output_functions, "    if (%s != NULL) %s (%s); \\\n",
                outputs[i].name, ti->cleanup_func, outputs[i].name);
    }
  fprintf (output_functions, "  }G_STMT_END\n");

  for (i = 0; i < n_inputs; i++)
    {
      TypeInfo *ti = get_type_info (inputs[i].type);
      if (ti == NULL)
        g_error ("no type-info found for %s", g_type_name (inputs[i].type));
      fprintf (output_functions, "  %s %s = %s (inputs + %u);\n",
              ti->c_type, inputs[i].name, ti->get_value, i);
    }
  for (i = 0; i < n_outputs; i++)
    {
      TypeInfo *ti = get_type_info (outputs[i].type);
      if (outputs[i].type == BB_TYPE_VALUE)
        {
          if (n_outputs == 1)
            fprintf (output_functions, "  GValue *%s = output;\n",
                     outputs[i].name);
          else
            {
              /* XXX: may not work, since i don't see where outputs is alloced */
              fprintf (output_functions, "  GValue *%s = outputs + %u;\n",
                       outputs[i].name, i);
            }
        }
      else if (ti->init_value)
        fprintf (output_functions, "  %s %s = %s;\n",
                ti->c_type, outputs[i].name, ti->init_value);
      else
        fprintf (output_functions, "  %s %s;\n",
                ti->c_type, outputs[i].name);
    }
  fprintf (output_functions, "  {\n%s\n  }\n", code);
  if (n_outputs == 0)
    {
    }
  else if (n_outputs == 1)
    {
      if (outputs[0].type != 0)
        {
          TypeInfo *ti = get_type_info (outputs[0].type);
          if (ti->set_value == NULL)
            {
              /* this is the "GValue" case (ie variable type output);
                 nothing needs to be done. */
            }
          else
            {
              fprintf (output_functions, "  g_value_init (output, %s);\n", ti->type_macro);
              fprintf (output_functions, "  %s (output, %s);\n", ti->set_value, outputs[0].name);
            }
        }
    }
  else
    for (i = 0; i < n_outputs; i++)
      if (outputs[0].type != 0)
        {
          TypeInfo *ti = get_type_info (outputs[i].type);
          fprintf (output_functions, "  g_value_init (outputs + %u, %s);\n", i, ti->type_macro);
          fprintf (output_functions, "  %s (outputs + %u, %s);\n", ti->set_value, i, outputs[0].name);
        }
  fprintf (output_functions, "  CLEANUP();\n");
  fprintf (output_functions, "#undef CLEANUP\n");
  fprintf (output_functions, "  return TRUE;\n}\n");
  g_free (mangled_name);
}

static void
emit_registration (const char *function_name,
                   guint       n_inputs,
                   Param      *inputs,
                   guint       n_outputs,
                   Param      *outputs)
{
  char *mangled_name = get_mangled_name (function_name, n_inputs, inputs, n_outputs, outputs);
  guint i;
  fprintf (output_registration,
           "sig = bb_function_signature_new (");
  for (i = 0; i < n_inputs; i++)
    {
      TypeInfo *ti = get_type_info (inputs[i].type);
      fprintf (output_registration, "%s, ", ti->type_macro);
    }
  fprintf (output_registration, "(GType) 0, ");
  for (i = 0; i < n_outputs; i++)
    {
      TypeInfo *ti = get_type_info (outputs[i].type);
      fprintf (output_registration, "%s, ", ti->type_macro);
    }
  fprintf (output_registration, "(GType) 0);\n");
  if (n_outputs < 2)
    fprintf (output_registration, "bb_function_new_single_output");
  else
    fprintf (output_registration, "bb_function_new_general");
  fprintf (output_registration, " (\"%s\", sig, %s);\n", function_name, mangled_name);
  fprintf (output_registration, "bb_function_signature_unref (sig);\n");
}

static const char *
parse_params_array (const char *start, const char *dir, guint *n_out, Param **out, guint line_no)
{
  const char *at = start;
  GArray *params = g_array_new (FALSE, FALSE, sizeof (Param));
  GSK_SKIP_WHITESPACE (at);
  if (*at != '(')
    g_error ("missing '(' on line %u for %s-params", line_no, dir);
  at++;
  for (;;)
    {
      Param p;
      char *tmp_typename;
      GSK_SKIP_WHITESPACE (at);
      if (*at == ')')
        break;
      start = at;
      GSK_SKIP_CHAR_TYPE (at, IS_IDENTIFIER_CHAR);
      if (at == start)
        g_error ("error parsing type for %s-arg %u [line %u]", dir, params->len, line_no);
      tmp_typename = g_strndup (start, at-start);
      GSK_SKIP_WHITESPACE (at);
      start = at;
      GSK_SKIP_CHAR_TYPE (at, IS_IDENTIFIER_CHAR);
      if (at == start)
        g_error ("error parsing name for %s-arg %u [line %u]", dir, params->len, line_no);
      p.name = g_strndup (start, at-start);
      p.type = g_type_from_name (tmp_typename);
      if (p.type == 0)
        g_error ("error finding type %s [%s-arg %u, line %u]", tmp_typename, dir, params->len, line_no);
      g_free (tmp_typename);
      tmp_typename = NULL;
      GSK_SKIP_WHITESPACE (at);
      g_array_append_val (params, p);
      if (*at == ',')
        {
          at++;
          GSK_SKIP_WHITESPACE (at);
        }
      else if (*at == ')')
        break;
      else if (*at == '\0')
        g_error ("unexpected eof after %s-arg %u [line %u]", dir, params->len, line_no);
      else
        g_error ("unexpected character '%c' after %s-arg %u [line %u]", *at, dir, params->len, line_no);
    }
  at++;
  GSK_SKIP_WHITESPACE (at);
  *n_out = params->len;
  *out = (Param *) g_array_free (params, FALSE);
  return at;
}

int main(int argc, char **argv)
{
  BbInputFile *file;
  GError *error = NULL;

  type_info_quark = g_quark_from_static_string ("Type-Info");
  g_type_init ();
  bb_init ();

  output_functions = fopen (".generated/binding-impls.inc.tmp", "w");
  output_registration = fopen (".generated/binding-regs.inc.tmp", "w");

  register_type_boxed (BB_TYPE_WAVEFORM, "bb_waveform_unref");
  register_type_boxed (BB_TYPE_FILTER1, "bb_filter1_free");
  register_type_boxed (BB_TYPE_VALUE_ARRAY, "bb_value_array_unref");
  register_type_boxed (BB_TYPE_DOUBLE_ARRAY, "bb_double_array_free");

  register_type_full (G_TYPE_DOUBLE, "G_TYPE_DOUBLE", "gdouble",
                      "g_value_get_double", "g_value_set_double", "0.0", NULL);
  register_type_full (G_TYPE_UINT, "G_TYPE_UINT", "guint",
                      "g_value_get_uint", "g_value_set_uint", "0", NULL);
  register_type_full (G_TYPE_INT, "G_TYPE_INT", "gint",
                      "g_value_get_int", "g_value_set_int", "0", NULL);
  register_type_full (BB_TYPE_DURATION, "BB_TYPE_DURATION", "BbDuration",
                      "value_get_duration", "value_set_duration", "{BB_DURATION_UNITS_SECONDS, 0}", NULL);
  register_type_full (BB_TYPE_VALUE, "BB_TYPE_VALUE", "GValue *",
                      "", NULL, "NULL", NULL);

  register_type_enum (BB_TYPE_BUTTERWORTH_MODE);

  if (argc != 1)
    usage ();
#if 0
  for (i = 1; i < (guint)argc; i++)
    {
      if (argv[i][0] == '-')
	{
	  usage ();
	}
      else
        {
	  if (filename)
	    g_error ("cannot specify multiple files to make-bindings");
	  filename = argv[i];
	}
    }
  if (filename == NULL)
    g_error ("missing filename");
#endif

  file = bb_input_file_open ("core/binding-data", &error);
  if (file == NULL)
    g_error ("error opening %s: %s", "core/binding-data", error->message);
  
  while (!file->eof)
    {
      /* parse:  function_name(intype inname,...) = (outtype outname, ...). */
      const char *at = file->line;
      const char *start;
      guint n_in;
      Param *in;
      guint n_out;
      Param *out;
      char *function_name;
      GSK_SKIP_WHITESPACE (at);
      GString *code;
      guint i;
      guint brace_count;
      if (*at == '\0' || *at == '#')
        {
          bb_input_file_advance (file);
          continue;
        }

      start = at;
      GSK_SKIP_CHAR_TYPE (at, IS_IDENTIFIER_CHAR);
      if (at == start)
        g_error ("error parsing function name on line %u", file->line_no);
      function_name = g_strndup (start, at - start);
      GSK_SKIP_WHITESPACE (at);
      at = parse_params_array (at, "in", &n_in, &in, file->line_no);
      GSK_SKIP_WHITESPACE (at);
      if (*at == '=')
        {
          at++;
          GSK_SKIP_WHITESPACE (at);
          at = parse_params_array (at, "out", &n_out, &out, file->line_no);
          GSK_SKIP_WHITESPACE (at);
        }
      else
        {
          /* 0 outputs */
          n_out = 0;
          out = NULL;
        }
      if (*at == '.')
        ;
      else if (*at == 0)
        g_error ("unexpected eof at line %u (perhaps you want a '.')", file->line_no);
      else
        g_error ("unexpected char %c at line %u", *at, file->line_no);

      /* parse code */
      code = g_string_new ("");
      brace_count = 0;
      while (bb_input_file_advance (file))
        {
          g_string_append (code, file->line);
          g_string_append_c (code, '\n');
          for (at = file->line; *at; at++)
            if (*at == '{')
              brace_count++;
            else if (*at == '}')
              {
                g_assert (brace_count != 0);
                brace_count--;
              }
          if (brace_count == 0)
            {
              bb_input_file_advance (file);
              break;
            }
        }

      /* emit code */
      emit_function (function_name, n_in, in, n_out, out, code->str);
      emit_registration (function_name, n_in, in, n_out, out);

      for (i = 0; i < n_in; i++)
        g_free (in[i].name);
      for (i = 0; i < n_out; i++)
        g_free (out[i].name);
      g_string_free (code, TRUE);
      g_free (in);
      g_free (out);
      g_free (function_name);
    }

  rename (".generated/binding-impls.inc.tmp", ".generated/binding-impls.inc");
  rename (".generated/binding-regs.inc.tmp", ".generated/binding-regs.inc");

  return 0;
}
