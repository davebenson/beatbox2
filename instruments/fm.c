/* parameters:
  - waveform (controls envelope and modulation)
 */

#include "instrument-includes.h"
#include "../core/bb-waveform.h"

enum
{
  PARAM_FREQUENCY,
  PARAM_H,		/* modulation / carrier frequency ratio */
  PARAM_MIN_DEPTH,	/* minimum affect of modulation */
  PARAM_MAX_DEPTH,	/* maximum affect of modulation */
  PARAM_DURATION,
  PARAM_ENVELOPE,       /* overall carrier envelop */
  PARAM_MODULATION_ENVELOPE /* modulation strength (defaults to envelope) */
};

static const BbInstrumentSimpleParam fm_params[] =
{
  { PARAM_FREQUENCY,             "frequency",                  440 },
  { PARAM_H,                     "H",                          1.0 },
  { PARAM_MIN_DEPTH,             "min_depth",                  0.0 },
  { PARAM_MAX_DEPTH,             "max_depth",                  2.0 },
};
  

static double *
fm_synth (BbInstrument *instrument,
          BbRenderInfo *render_info,
          GValue       *params,
          guint        *n_samples_out)
{
  double frequency       = g_value_get_double (params + PARAM_FREQUENCY);
  double H               = g_value_get_double (params + PARAM_H);
  double min_depth       = g_value_get_double (params + PARAM_MIN_DEPTH);
  double max_depth       = g_value_get_double (params + PARAM_MAX_DEPTH);
  double delta_depth     = max_depth - min_depth;
  BbWaveform *env        = g_value_get_boxed (params + PARAM_ENVELOPE);
  BbWaveform *mod_env    = g_value_get_boxed (params + PARAM_MODULATION_ENVELOPE);

  guint n                = render_info->note_duration_samples;
  double waveform_step   = 1.0 / n;
  double pos             = 0;
  double base_step       = BB_2PI * frequency / render_info->sampling_rate;
  double mod_pos         = 0;
  double mod_step        = base_step * H;
  double *rv = g_new (double, n);
  double *mod_env_data = NULL;
  guint i;
  
  for (i = 0; i < n; i++)
    rv[i] = waveform_step * i;
  if (mod_env)
    mod_env_data = g_memdup (rv, sizeof (gdouble) * n);
  bb_waveform_eval_array (env, rv, n);
  if (mod_env)
    bb_waveform_eval_array (mod_env, mod_env_data, n);
  else
    mod_env_data = rv;
  for (i = 0; i < n; i++)
    {
      double mod_value = sin (mod_pos);
      mod_pos += mod_step;
      if (mod_pos >= BB_2PI)
        mod_pos -= BB_2PI;
      pos += base_step + mod_step * mod_value * (min_depth + delta_depth * mod_env_data[i]);
      rv[i] *= sin (pos);
      if (pos < -BB_2PI || pos > BB_2PI * 2.0)
        pos = fmod (pos, BB_2PI);
    }
  if (mod_env)
    g_free (mod_env_data);
  *n_samples_out = n;
  return rv;
}


BB_INIT_DEFINE(_bb_fm_init)
{
  BbInstrument *instrument;
  instrument = bb_instrument_new_from_simple_param (G_N_ELEMENTS (fm_params), fm_params);
  bb_instrument_add_param (instrument, PARAM_DURATION, param_spec_duration ());
  bb_instrument_add_param (instrument, PARAM_ENVELOPE,
                           g_param_spec_boxed ("envelope", "Envelope", NULL, BB_TYPE_WAVEFORM, 0));
  bb_instrument_add_param (instrument, PARAM_MODULATION_ENVELOPE,
                           g_param_spec_boxed ("modulation_envelope", "Modulation Envelope", NULL, BB_TYPE_WAVEFORM, 0));
  instrument->synth_func = fm_synth;
  bb_instrument_set_name (instrument, "fm");
}
