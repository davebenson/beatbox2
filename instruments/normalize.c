#include "instrument-includes.h"
//#include <gsk/gskghelpers.h>
#include "../core/bb-vm.h"

static double *
normalize_synth    (BbInstrument *instrument,
                    BbRenderInfo *render_info,
                    GValue       *params,
                    guint        *n_samples_out)
{
  BbInstrument *sub = instrument->data;
  double *rv = bb_instrument_synth (sub, render_info, params, n_samples_out);
  gdouble max_abs = 0;
  guint n = *n_samples_out;
  guint i;
  for (i = 0; i < n; i++)
    {
      gdouble a = ABS (rv[i]);
      if (a > max_abs)
        max_abs = a;
    }
  if (max_abs == 0)
    g_warning ("normalize encountered waveform with no sound");
  else
    {
      gdouble gain = 1.0 / max_abs;
      for (i = 0; i < n; i++)
        rv[i] *= gain;
    }
  return rv;
}

static double *
normalize_dc_block_synth    (BbInstrument *instrument,
                             BbRenderInfo *render_info,
                             GValue       *params,
                             guint        *n_samples_out)
{
  BbInstrument *sub = instrument->data;
  double *rv = bb_instrument_synth (sub, render_info, params, n_samples_out);
  gdouble min, max;
  guint n = *n_samples_out;
  guint i;
  if (n == 0)
    {
      g_warning ("normalize (with dcblock) called on zero-length waveform");
      return rv;
    }

  min = max = rv[0];
  for (i = 1; i < n; i++)
    {
      if (rv[i] < min)
        min = rv[i];
      else if (rv[i] > max)
        max = rv[i];
    }
  if (min == max)
    {
      /* um, i think this is reasonable... */
      memset (rv, 0, sizeof (gdouble) * n);
      return rv;
    }
  else
    {
      gdouble factor = 2.0 / (max - min);
      gdouble median = (min + max) * 0.5;	/* the median value should map to 0 */
      for (i = 0; i < n; i++)
        rv[i] = (rv[i] - median) / factor;
    }
  return rv;
}

static BbInstrument *
construct_normalize    (guint            n_args,
                        char           **args,
                        GError         **error)
{
  BbInstrument *base;
  BbInstrument *rv;
  gboolean dc_block = FALSE;
  if (n_args != 1 && n_args != 2)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                   "normalize requires 1 or 2 args");
      return NULL;
    }
  if (n_args == 2)
    {
      if (strcmp (args[1], "dcblock") != 0)
        {
	  g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
		       "normalizes second arguments only allowed value is 'dcblock' (got %s)", args[1]);
	  return NULL;
	}
      dc_block = TRUE;
    }
  base = bb_instrument_from_string (args[0]);
  rv = bb_instrument_new_generic_v (NULL, base->n_param_specs, base->param_specs);
  rv->synth_func = dc_block ? normalize_dc_block_synth : normalize_synth;
  rv->data = base;
  rv->free_data = g_object_unref;
  return rv;
}

BB_INIT_DEFINE(_bb_normalize_init)
{
  bb_instrument_template_constructor_register ("normalize", construct_normalize);
}
