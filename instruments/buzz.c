#include "instrument-includes.h"

/* --- buzz --- */
static double *
buzz_synth  (BbInstrument *instrument,
             BbRenderInfo *render_info,
             GValue       *params,
             guint        *n_samples_out)
{
  /* TODO: optimize? */
  double freq            = g_value_get_double (params + 1);
  double sample_to_frac  = freq / render_info->sampling_rate;
  double r = 0;
  guint n = render_info->note_duration_samples;
  guint i;
  gdouble *rv = g_new (double, n);
  int v = 1;
  *n_samples_out = n;
  sample_to_frac = fmod (sample_to_frac, 1.0);
  for (i = 0; i < n; i++)
    {
      r += sample_to_frac;
      if (r >= 1.0)
        {
          rv[i] = v;
          v = -v;
          r -= 1.0;
        }
      else
        rv[i] = 0;
    }
  return rv;
}

BB_INIT_DEFINE(_bb_buzz_init)
{
  BbInstrument *instrument =
  bb_instrument_new_generic ("buzz",
                             param_spec_duration (),
                             param_spec_frequency (),
                             NULL);
  instrument->synth_func = buzz_synth;
}
