#include "instrument-includes.h"
//#include <gsk/gskghelpers.h>
#include "../core/bb-vm.h"
#include "../core/bb-filter1.h"

typedef struct _FilterInfo FilterInfo;
struct _FilterInfo
{
  BbInstrument *base;
  guint n_filters;
  BbProgram **filter_programs;
};

static double *
filter_synth    (BbInstrument *instrument,
                 BbRenderInfo *render_info,
                 GValue       *params,
                 guint        *n_samples_out)
{
  FilterInfo *info = instrument->data;
  double *rv = bb_instrument_synth (info->base, render_info, params, n_samples_out);
  guint n = *n_samples_out;
  double *rv_alt = g_new (double, n);
  guint i;
  for (i = 0; i < info->n_filters; i++)
    {
      /* construct filter */
      GValue v = BB_VALUE_INIT;
      BbFilter1 *f;
      g_value_init (&v, BB_TYPE_FILTER1);
      bb_vm_run_get_value_or_die (info->filter_programs[i], render_info, instrument->n_param_specs, params, &v);
      f = g_value_get_boxed (&v);

      /* filter buffer */
      bb_filter1_run_buffer (f, rv, rv_alt, n);

      /* swap buffers */
      {
        double *tmp;
        tmp = rv;
        rv = rv_alt;
        rv_alt = tmp;
      }

      g_value_unset (&v);
    }
  return rv;
}

static void
free_filter_info (gpointer data)
{
  FilterInfo *info = data;
  guint i;
  for (i = 0; i < info->n_filters; i++)
    bb_program_unref (info->filter_programs[i]);
  g_free (info->filter_programs);
  g_object_unref (info->base);
  g_free (info);
}

static BbInstrument *
construct_filter  (guint            n_args,
                   char           **args,
                   GError         **error)
{
  BbInstrument *base;
  BbInstrument *rv;
  FilterInfo *info;
  GPtrArray *pspecs = g_ptr_array_new ();
  guint i;
  BbProgram **programs;
  if (n_args < 1)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                   "filter requires at least 1 arg");
      return NULL;
    }
  base = bb_instrument_from_string (args[0]);
  for (i = 0; i < base->n_param_specs; i++)
    g_ptr_array_add (pspecs, base->param_specs[i]);
  programs = g_new (BbProgram *, n_args - 1);
  for (i = 1; i < n_args; i++)
    {
      programs[i-1] = bb_program_parse_create_params (args[i], pspecs, 0, NULL, error);
      if (programs[i-1] == NULL)
        {
          g_ptr_array_foreach (pspecs, (GFunc) g_param_spec_sink, NULL);
          g_ptr_array_free (pspecs, TRUE);
          return NULL;
        }
   }
  info = g_new (FilterInfo, 1);
  info->n_filters = n_args - 1;
  info->filter_programs = programs;
  info->base = base;
  rv = bb_instrument_new_generic_v (NULL, pspecs->len, (GParamSpec**)(pspecs->pdata));
  rv->synth_func = filter_synth;
  rv->data = info;
  rv->free_data = free_filter_info;
  return rv;
}

BB_INIT_DEFINE(_bb_filter_init)
{
  bb_instrument_template_constructor_register ("filter", construct_filter);
}
