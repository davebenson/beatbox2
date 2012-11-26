#include "instrument-includes.h"
//#include <gsk/gskghelpers.h>
#include "../core/bb-vm.h"

static void
gain (guint n, double *inout, double scale)
{
  while (n-- > 0)
    *inout++ *= scale;
}

static double *
gain_synth    (BbInstrument *instrument,
               BbRenderInfo *render_info,
               GValue       *params,
               guint        *n_samples_out)
{
  BbInstrument *sub = instrument->data;
  double *rv = bb_instrument_synth (sub, render_info, params + 1, n_samples_out);
  gain (*n_samples_out, rv, g_value_get_double (params + 0));
  return rv;
}

static BbInstrument *
construct_gain    (guint            n_args,
                   char           **args,
                   GError         **error)
{
  BbInstrument *base;
  BbInstrument *rv;
  if (n_args != 1)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                   "gain requires 1 arg");
      return NULL;
    }
  base = bb_instrument_from_string (args[0]);
  {
    GParamSpec **pspecs = g_newa (GParamSpec *, base->n_param_specs + 1);
    guint i;
    pspecs[0] = g_param_spec_double ("gain", "Gain", NULL, 
                                     0, 100000, 1, G_PARAM_READWRITE);
    for (i = 0; i < base->n_param_specs; i++)
      pspecs[i+1] = base->param_specs[i];
    rv = bb_instrument_new_generic_v (NULL, base->n_param_specs + 1, pspecs);
    rv->synth_func = gain_synth;
  }
  rv->data = base;
  rv->free_data = g_object_unref;
  return rv;
}

BB_INIT_DEFINE(_bb_gain_init)
{
  bb_instrument_template_constructor_register ("gain", construct_gain);
}
