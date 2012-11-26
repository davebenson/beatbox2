#include "instrument-includes.h"
#include "../core/bb-waveform.h"

/* --- wavetable --- */
static double *
wavetable_synth  (BbInstrument *instrument,
                  BbRenderInfo *render_info,
                  GValue       *params,
                  guint        *n_samples_out)
{
  /* TODO: optimize? */
  double freq            = g_value_get_double (params + 1);
  BbWaveform *waveform   = g_value_get_boxed (params + 2);
  double step            = freq / (double)(render_info->sampling_rate);
  double r = 0;
  guint n = render_info->note_duration_samples;
  guint i;
  gdouble *rv = g_new (double, n);
  *n_samples_out = n;

  /* ensure 0 <= step < 1. */
  if (step >= 1.0)
    do step -= 1.0; while (step >= 1.0);
  else
    while (step < 0.0) step += 1.0;

  for (i = 0; i < n; i++)
    {
      rv[i] = r;
      r += step;
      if (r >= 1.0)
        r -= 1.0;
    }
  bb_waveform_eval_array (waveform, rv, n);
  return rv;
}

BB_INIT_DEFINE(_bb_wavetable_init)
{
  BbInstrument *instrument = bb_instrument_new_generic ("wavetable",
                                                        param_spec_duration (),
                                                        param_spec_frequency (),
                                                        g_param_spec_boxed ("waveform", "Waveform", NULL, BB_TYPE_WAVEFORM, G_PARAM_READWRITE),
                                                        NULL);
  instrument->synth_func = wavetable_synth;
}
