#include "instrument-includes.h"

/* --- exp_decay --- */
static double *
exp_decay_synth (BbInstrument *instrument,
                 BbRenderInfo *render_info,
                 GValue       *params,
                 guint        *n_samples_out)
{
  double attack_samples = bb_value_get_duration_as_samples (params + 1, render_info);
  guint attack_end_sample;
  double halflife_samples   = bb_value_get_duration_as_samples (params + 2, render_info);
  double release_time    = bb_value_get_duration_as_note_lengths (params + 3, render_info);
  //double duration_secs   = duration_beats * render_info->beat_period;
  guint n = render_info->note_duration_samples;
  double *rv;
  guint exp_end_sample;
  gdouble exp_per_sample;
  guint i, j;
  *n_samples_out = n;
  rv = g_new (double, n);

  /* must be an exact integer, since it's used as a loop and divide-by-0 guard */
  exp_end_sample = (guint)((1.0 - release_time) * (double)n);
  if (attack_samples < 0)
    attack_end_sample = 0;
  else if (attack_samples > n)
    attack_end_sample = n;
  else
    attack_end_sample = (gint) attack_samples;

  /* exp(halflife_samples * exp_per_sample) = 1/2.
   hence:
     halflife_samples * exp_per_sample = log(1/2) */
  exp_per_sample = log(0.5) / halflife_samples;

  if (attack_end_sample > 0)
    bb_linear (rv, 0, 0, attack_end_sample, 1.0);

  /* exponential part */
  for (i = attack_end_sample, j = 0; i < exp_end_sample; i++, j++)
    rv[i] = exp (exp_per_sample * j);

  /* linear part */
  if (i < n)
    bb_linear (rv, i, exp (exp_per_sample * j), n, 0.0);

  return rv;
}

BB_INIT_DEFINE(_bb_exp_decay_init)
{
  BbInstrument *instrument =
  bb_instrument_new_generic ("exp-decay",
                             param_spec_duration (),
                             /* attack-time is 0 for back-compatibility */
                             bb_param_spec_duration ("attack-time", "Attack Time (%)", NULL,
                                                      BB_DURATION_UNITS_NOTE_LENGTHS, 0.0, G_PARAM_READWRITE),
                             bb_param_spec_duration ("halflife-time", "Halflife Time (%)", NULL,
                                                      BB_DURATION_UNITS_NOTE_LENGTHS, 0.08, G_PARAM_READWRITE),
                             bb_param_spec_duration ("release-time", "Release Time (%)", NULL,
                                                      BB_DURATION_UNITS_NOTE_LENGTHS, 0.08, G_PARAM_READWRITE),
                             NULL);
  instrument->synth_func = exp_decay_synth;
}
