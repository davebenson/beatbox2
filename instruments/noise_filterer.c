#include "instrument-includes.h"
#include "../core/bb-filter1.h"
#include "../core/bb-delay-line.h"

/* --- adsr --- */
static double *
noise_filterer_synth (BbInstrument *instrument,
                      BbRenderInfo *render_info,
                      GValue       *params,
                      guint        *n_samples_out)
{
  double repeat_count    = g_value_get_double (params + 1);
  BbFilter1 *filter      = g_value_dup_boxed (params + 2);
  double  prepasses    = g_value_get_double (params + 3);
  double  falloff    = g_value_get_double (params + 4);
  guint n = render_info->note_duration_samples;
  guint delay_line_len_unclamped = n / MAX (1, repeat_count);
  guint delay_line_len = MAX (delay_line_len_unclamped, 10);
  BbDelayLine *delay_line = bb_delay_line_new (delay_line_len);
  double *rv = g_new (double, n);
  double last = 0;
  guint i, j;
if (repeat_count < 1.0) g_message ("repeat-count is too low (%.6f)", repeat_count);
//g_message ("overall falloff: %.6f", falloff);
  falloff = pow (falloff, 1.0 / (1.0 + repeat_count));
//g_message ("per-instance falloff: %.6f", falloff);
  for (i = 0; i < delay_line_len; i++)
    delay_line->buffer[i] = g_random_double_range (-1, 1);
  for (j = 0; j < prepasses; j++)
    for (i = 0; i < delay_line_len; i++)
      last = bb_delay_line_add (delay_line, bb_filter1_run (filter, last));
  for (i = 0; i < n; i++)
    {
      rv[i] = last = bb_delay_line_add (delay_line, bb_filter1_run (filter, last));
      last *= falloff;
    }
  *n_samples_out = n;
  bb_filter1_free (filter);
  return rv;
}

BB_INIT_DEFINE (_bb_noise_filterer_init)
{
  BbInstrument *instrument =
  bb_instrument_new_generic ("noise_filterer",
                             param_spec_duration (),
                             g_param_spec_double ("repeat-count", "repeat-count",
			                          "Number of times for noise-buffer to repeat in duration",
                                                  0.0, 10000.0, 20.0, G_PARAM_READWRITE),
                             g_param_spec_boxed ("filter", "Filter", NULL,
			                         BB_TYPE_FILTER1, G_PARAM_READWRITE),
                             g_param_spec_double ("prepasses", "Prepasses", NULL,
			                        0, 10000, 1, G_PARAM_READWRITE),
                             g_param_spec_double ("falloff", "Falloff", NULL,
			                        0.0, 1.0, 0.99, G_PARAM_READWRITE),
                             NULL);
  instrument->synth_func = noise_filterer_synth;
}
