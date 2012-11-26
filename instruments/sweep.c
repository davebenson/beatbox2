#include "instrument-includes.h"
#include <math.h>

/* --- sweep --- */
static double *
sweep_synth  (BbInstrument *instrument,
              BbRenderInfo *render_info,
              GValue       *params,
              guint        *n_samples_out)
{
  double freq            = g_value_get_double (params + 1);
  double sample_to_radian = M_PI * 2.0 * freq / render_info->sampling_rate;
  double octave_time = bb_value_get_duration_as_samples (params + 2, render_info);
  double factor = log(2.0) / octave_time;

  double r = 0;
  guint n = render_info->note_duration_samples;
  guint i;
  gdouble *rv = g_new (double, n);
  *n_samples_out = n;

  for (i = 0; i < n; i++)
    {
      rv[i] = sin (r);
      r += sample_to_radian * exp (factor * i);
      if (r > G_PI)
        r -= 2.0 * G_PI;
    }
  return rv;
}

BB_INIT_DEFINE(_bb_synth_init)
{
  BbInstrument *instrument =
  bb_instrument_new_generic ("sweep",
                             param_spec_duration (),
                             param_spec_frequency (),
                             bb_param_spec_duration ("octave_time",
                                                     "Octave Time",
                                "time to fall one octave",
                                BB_DURATION_UNITS_BEATS, 1.0,
                                G_PARAM_READWRITE),
                             NULL);
  instrument->synth_func = sweep_synth;
}
