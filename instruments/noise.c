#include "instrument-includes.h"

static double *
noise_synth  (BbInstrument *instrument,
              BbRenderInfo *render_info,
              GValue       *params,
              guint        *n_samples_out)
{
  guint n = render_info->note_duration_samples;
  double *rv;
  guint i;
  *n_samples_out = n;
  rv = g_new (double, n);
  for (i = 0; i < n; i++)
    rv[i] = g_random_double_range (-1, 1); /* um, is this ok? */
  return rv;
}

static double *
noise_01_synth  (BbInstrument *instrument,
                 BbRenderInfo *render_info,
                 GValue       *params,
                 guint        *n_samples_out)
{
  double duty_cycle      = g_value_get_double (params + 1);
  guint n = render_info->note_duration_samples;
  double *rv;
  guint i;
  *n_samples_out = n;
  rv = g_new (double, n);
  for (i = 0; i < n; i++)
    rv[i] = g_random_double () < duty_cycle ? 1.0 : 0.0;
  return rv;
}
BB_INIT_DEFINE(_bb_noise_init)
{
  BbInstrument *instrument =
  bb_instrument_new_generic ("noise", param_spec_duration (), NULL);
  instrument->synth_func = noise_synth;

  instrument =
  bb_instrument_new_generic ("noise_01",
                             param_spec_duration (),
                             g_param_spec_double ("duty-cycle", "Duty Cycle", NULL,
                                                  0.0, 1.0, 0.5, G_PARAM_READWRITE),
                             NULL);
  instrument->synth_func = noise_01_synth;
}
