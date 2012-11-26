#include "instrument-includes.h"

/* --- adsr --- */
static double *
adsr_synth (BbInstrument *instrument,
            BbRenderInfo *render_info,
            GValue       *params,
            guint        *n_samples_out)
{
  double attack_time     = bb_value_get_duration_as_samples (params + 1, render_info);
  double decay_time      = bb_value_get_duration_as_samples (params + 2, render_info);
  double sustain_level   = g_value_get_double (params + 3);
  double release_time    = bb_value_get_duration_as_samples (params + 4, render_info);
  guint n = render_info->note_duration_samples;
  double *rv;
  guint attack_end, decay_end, sustain_end;
  *n_samples_out = n;
  rv = g_new (double, n);

  attack_end = (guint)attack_time;
  decay_end = (guint) (attack_time + decay_time);
  if (release_time >= (gdouble)n)
    sustain_end = 0;
  else
    sustain_end = (gdouble) n - release_time;

  if (attack_end > n)
    attack_end = n;
  if (decay_end > n)
    decay_end = n;
  if (sustain_end > n)
    sustain_end = n;
  bb_linear (rv, 0, 0.0, attack_end, 1.0);
  bb_linear (rv, attack_end, 1.0, decay_end, sustain_level);
  bb_linear (rv, decay_end, sustain_level, sustain_end, sustain_level);
  bb_linear (rv, sustain_end, sustain_level, n, 0.0);
  return rv;
}


BB_INIT_DEFINE (_bb_adsr_init)
{
  BbInstrument *instrument =
  bb_instrument_new_generic ("adsr",
                             param_spec_duration (),
                             bb_param_spec_duration ("attack-time", "Attack Time", NULL,
                                                     BB_DURATION_UNITS_NOTE_LENGTHS, 0.1, G_PARAM_READWRITE),
                             bb_param_spec_duration ("decay-time", "Decay Time", NULL,
                                                     BB_DURATION_UNITS_NOTE_LENGTHS, 0.3, G_PARAM_READWRITE),
                             g_param_spec_double ("sustain-level", "Sustain Level", NULL,
                                                  0.0, 1.0, 0.5, G_PARAM_READWRITE),
                             bb_param_spec_duration ("release-time", "Release Time", NULL,
                                                     BB_DURATION_UNITS_NOTE_LENGTHS, 0.4, G_PARAM_READWRITE),
                             NULL);
  instrument->synth_func = adsr_synth;
}
