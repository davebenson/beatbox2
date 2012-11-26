#include "instrument-includes.h"

/* there is no sound sweeter than the sound of the constant instrument */
static double *
constant_synth  (BbInstrument *instrument,
                 BbRenderInfo *render_info,
                 GValue       *params,
                 guint        *n_samples_out)
{
  double value           = g_value_get_double (params + 1);
  guint n = render_info->note_duration_samples;
  double *rv;
  guint i;
  *n_samples_out = n;
  rv = g_new (double, n);
  for (i = 0; i < n; i++)
    rv[i] = value;
  return rv;
}

BB_INIT_DEFINE(_bb_constant_init)
{
  BbInstrument *instrument =
  bb_instrument_new_generic ("constant",
                             param_spec_duration (),
                             g_param_spec_double ("value", "Value", NULL,
                                                  -100000,100000,1.0,G_PARAM_READWRITE),
                             NULL);
  instrument->synth_func = constant_synth;
}
