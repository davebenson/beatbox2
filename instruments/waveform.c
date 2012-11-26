#include "instrument-includes.h"
#include "../core/bb-waveform.h"

static double *
waveform_synth    (BbInstrument *instrument,
                   BbRenderInfo *render_info,
                   GValue       *params,
                   guint        *n_samples_out)
{
  BbWaveform *waveform = g_value_get_boxed (params + 1);
  guint n = render_info->note_duration_samples;
  gdouble *rv = g_new (double, n);
  bb_waveform_eval_linear (waveform, 0.0, 1.0 / n, rv, n);
  *n_samples_out = n;
  return rv;
}

BB_INIT_DEFINE (_bb_waveform_init)
{
  BbInstrument *instrument =
  bb_instrument_new_generic ("waveform",
                             param_spec_duration (),
                             g_param_spec_boxed ("waveform", "Waveform", NULL, BB_TYPE_WAVEFORM, G_PARAM_READWRITE),
                             NULL);
  instrument->synth_func = waveform_synth;
}
