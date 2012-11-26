#include "instrument-includes.h"

/* --- sin --- */
static double *
sin_synth  (BbInstrument *instrument,
            BbRenderInfo *render_info,
            GValue       *params,
            guint        *n_samples_out)
{
  double freq            = g_value_get_double (params + 1);
  double sample_to_radian = M_PI * 2.0 * freq / render_info->sampling_rate;
  double r = 0;
  guint n = render_info->note_duration_samples;
  guint i;
  gdouble *rv = g_new (double, n);
  *n_samples_out = n;

  /* TODO: optimize? */
  for (i = 0; i < n; i++)
    {
      rv[i] = sin (r);
      r += sample_to_radian;
      if (r > G_PI)
        r -= 2.0 * G_PI;
    }
  return rv;
}

BB_INIT_DEFINE(_bb_sin_init)
{
  BbInstrument *instrument =
  bb_instrument_new_generic ("sin",
                             param_spec_duration (),
                             param_spec_frequency (),
                             NULL);
  instrument->synth_func = sin_synth;
}
