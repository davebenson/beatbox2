#include "instrument-includes.h"
#include <gsk/gskghelpers.h>
#include "../core/bb-vm.h"

typedef struct _ParamMapData ParamMapData;
struct _ParamMapData
{
  BbProgram **vars;
  BbInstrument *base;
};

#define DUMP_PARAMS	0

#if 0
static GParamSpec *copy_pspep_rename (GParamSpec *pspec,
                                      const char *name)
{
  if (G_IS_PARAM_SPEC_DOUBLE (pspec))
    {
      GParamSpecDouble *d = G_PARAM_SPEC_DOUBLE (pspec);
      return g_param_spec_double (name, name, NULL, d->minimum, d->maximum, d->default_value, pspec->flags);
    }
  else
    {
      g_return_val_if_reached (NULL);
    }
}
#endif

static void
param_map_data_free (gpointer data)
{
  ParamMapData *p = data;
  guint i;
  for (i = 0; i < p->base->n_param_specs; i++)
    if (p->vars[i] != NULL)
      bb_program_unref (p->vars[i]);
  g_free (p->vars);
  g_object_unref (p->base);
  g_free (p);
}

static double *
param_map_synth (BbInstrument *instrument,
                 BbRenderInfo *render_info,
                 GValue       *params,
                 guint        *n_samples_out)
{
  ParamMapData *p = instrument->data;
  GValue *values = g_newa (GValue, p->base->n_param_specs);
  guint i;
  BbRenderInfo my_render_info = *render_info;
  memset (values, 0, sizeof (GValue) * p->base->n_param_specs);
#if DUMP_PARAMS
  for (i = 0; i < instrument->n_param_specs; i++)
    g_message ("param_map: in: %u: %s => %.6f", i, instrument->param_specs[i]->name, g_value_get_double(params+i));
#endif
  for (i = 0; i < p->base->n_param_specs; i++)
    {
      GError *error = NULL;
      BbVm *vm = bb_vm_new (instrument->n_param_specs, params, FALSE);
      if (!bb_vm_run (vm, p->vars[i], render_info, &error))
        g_error ("error running vm: %s", error->message);
      g_assert (vm->n_values == 1);
      bb_vm_pop (vm, values + i);
#if DUMP_PARAMS
      g_message ("param_map: out: %u: %s => %.6f", i, p->base->param_specs[i]->name, g_value_get_double(values+i));
#endif
      bb_vm_free (vm);
    }
  if (p->base->duration_param >= 0)
    {
      BbDuration dur, subdur;
      gboolean has_dur;
      if (instrument->duration_param >= 0)
        {
          has_dur = TRUE;
          bb_value_get_duration (params + instrument->duration_param, &dur);
        }
      else
        has_dur = FALSE;
      bb_value_get_duration (values + p->base->duration_param, &subdur);

      if (!has_dur || subdur.units != dur.units || subdur.value != dur.value)
        {
          /* update my_render_info */
          my_render_info.note_duration_samples = bb_duration_to_samples (&subdur, render_info);
          my_render_info.note_duration_secs = bb_duration_to_seconds (&subdur, render_info);
          my_render_info.note_duration_beats = bb_duration_to_beats (&subdur, render_info);
        }
    }

  return bb_instrument_synth (p->base, &my_render_info, values, n_samples_out);
}

static BbInstrument *
construct_param_map (guint            n_args,
                     char           **args,
                     GError         **error)
{
  BbInstrument *base;
  GPtrArray *param_specs;
  ParamMapData *param_map_data;
  BbProgram **vars;
  guint i;
  BbInstrument *rv;
  if (n_args < 2)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                   "param_map requires 2 args");
      return NULL;
    }
  base = bb_instrument_from_string (args[0]);
  vars = g_new0 (BbProgram *, base->n_param_specs);
  param_specs = g_ptr_array_new ();
  for (i = 1; i < n_args; i++)
    {
      const char *at;
      GParamSpec *pspec;
      if (memcmp (args[i], "param", 5) != 0
       || !isspace (args[i][5]))
        break;

      /* parse pspec */
      at = args[i] + 5;
      GSK_SKIP_WHITESPACE (at);

      pspec = bb_parse_g_param_spec (at, error);
      if (pspec == NULL)
        {
          gsk_g_error_add_prefix (error, "parsing param-spec for param_map");
          return FALSE;
        }
      g_ptr_array_add (param_specs, pspec);
    }
  for (     ; i < n_args; i++)
    {
      /* assign parameters */
      const char *at = args[i];
      const char *name_start;
      guint name_len;
      guint p;
      GSK_SKIP_WHITESPACE (at);
      name_start = at;
      GSK_SKIP_CHAR_TYPE (at, IS_IDENTIFIER_CHAR);
      name_len = at - name_start;
      GSK_SKIP_WHITESPACE (at);
      if (*at != '=')
        {
          g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                       "expected '=' in param-map argument");
          return NULL;
        }
      at++;
      GSK_SKIP_WHITESPACE (at);
      for (p = 0; p < base->n_param_specs; p++)
        if (bb_param_spec_names_equal_len (base->param_specs[p]->name, -1,
                                           name_start, name_len))
          break;
      if (p == base->n_param_specs)
        {
          g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                       "no param %.*s found", name_len, name_start);
          return NULL;
        }
      if (vars[p] != NULL)
        {
          g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                       "param %.*s assigned twice", name_len, name_start);
          return NULL;
        }
      vars[p] = bb_program_parse_create_params (at, param_specs, base->n_param_specs, base->param_specs, error);
      if (vars[p] == NULL)
        {
          gsk_g_error_add_prefix (error, "parsing param %.*s", name_len, name_start);
          return NULL;
        }
    }

  for (i = 0; i < base->n_param_specs; i++)
    if (vars[i] == NULL)
      {
        char *txt, *tmp;
        vars[i] = bb_program_new ();
        txt = g_strdup (base->param_specs[i]->name);
        for (tmp = txt; *tmp; tmp++)
          if (*tmp == '-')
            *tmp = '_';
        vars[i] = bb_program_parse_create_params (txt, param_specs, base->n_param_specs, base->param_specs, error);
        g_assert (vars[i] != NULL);
        g_free (txt);
      }

  param_map_data = g_new (ParamMapData, 1);
  param_map_data->base = base;
  param_map_data->vars = vars;

  rv = bb_instrument_new_generic_v (NULL,
                                    param_specs->len,
                                    (GParamSpec **) param_specs->pdata);
  rv->synth_func = param_map_synth;
  rv->data = param_map_data;
  rv->free_data = param_map_data_free;
  g_ptr_array_free (param_specs, TRUE);
  return rv;
}

BB_INIT_DEFINE(_bb_param_map_init)
{
  bb_instrument_template_constructor_register ("param_map", construct_param_map);
}
