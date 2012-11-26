#include "instrument-includes.h"

enum
{
  PARAM_FREQUENCY,
  PARAM_NOISE_LEVEL,
  PARAM_MIN_SOUND_LEVEL,
  PARAM_GAIN,
  PARAM_ATTACK_TIME,
  PARAM_HALFLIFE_TIME,
};

#define MAX_GRAIN_LEN	88200

static const BbInstrumentSimpleParam grain2_params[] =
{
  { PARAM_FREQUENCY,       "frequency",         440.00 },
  { PARAM_NOISE_LEVEL,     "noise_level",         0.00 },
  { PARAM_MIN_SOUND_LEVEL, "min_sound_level",     0.01 },
  { PARAM_GAIN,            "gain",                1.00 }
};

static double *
grain2_synth (BbInstrument *instrument,
              BbRenderInfo *render_info,
              GValue       *params,
              guint        *n_samples_out)
{
  gdouble noise_level = g_value_get_double (params + PARAM_NOISE_LEVEL);
  gdouble min_sound_level = g_value_get_double (params + PARAM_MIN_SOUND_LEVEL);
  gdouble attack_time_samps = bb_value_get_duration_as_samples (params + PARAM_ATTACK_TIME, render_info);
  gdouble halflife_time_samps = bb_value_get_duration_as_samples (params + PARAM_HALFLIFE_TIME, render_info);
  gdouble frequency = g_value_get_double (params + PARAM_FREQUENCY);
  gdouble gain = g_value_get_double (params + PARAM_GAIN);
  guint i = 0;
  gdouble T;
  guint n;
  double *rv;
  BbComplex reson, factor;
  gdouble rad_per_sample = frequency * BB_2PI / render_info->sampling_rate;

  /* decay per sample */
  gdouble sound_level_decay = pow (0.5, 1.0 / halflife_time_samps);

  /* number of samples to decay to min_sound_level;
   *    sound_level_decay^T == min_sound_level
   * => T = log(min_sound_level)/log(sound_level_decay). */
  T = log (min_sound_level) / log (sound_level_decay)
    + attack_time_samps;
  guint ats = (guint) floor (attack_time_samps);
  
  if (T > MAX_GRAIN_LEN)
    n = MAX_GRAIN_LEN;
  else
    n = (guint) T;

  rv = g_new (gdouble, n);
  if (noise_level >= 1.0)
    {
      /* produce a pure noise grain */
      if (ats > 0)
	{
	  gdouble attack_step = 1.0 / (gdouble) ats;
	  gdouble cur_level = 0.0;
	  for (i = 0; i < ats; i++)
	    {
	      rv[i] = g_random_double_range (-noise_level, noise_level) * cur_level;
	      cur_level += attack_step;
	    }
	}
      for (     ; i < n; i++)
	{
	  rv[i] = g_random_double_range (-noise_level, noise_level);
	  noise_level *= sound_level_decay;
	}
    }
  else
    {
      gdouble tuned_level = (1.0 - noise_level) * gain;
      reson.re = 0;
      reson.im = tuned_level;
      factor.re = cos (rad_per_sample);
      factor.im = sin (rad_per_sample);
      if (noise_level == 0)
	{
	  if (ats > 0)
	    {
	      gdouble attack_step = 1.0 / (gdouble) ats;
	      gdouble cur_level = 0;
	      for (i = 0; i < ats; i++)
		{
		  rv[i] = reson.re * cur_level;
		  cur_level += attack_step;
		  BB_COMPLEX_MUL (reson, factor, reson);
		}
	    }
	  factor.re *= sound_level_decay;
	  factor.im *= sound_level_decay;
	  for (     ; i < n; i++)
	    {
	      rv[i] = reson.re;
	      BB_COMPLEX_MUL (reson, factor, reson);
	    }
	}
      else
	{
          gdouble real_noise_level = noise_level * gain;
	  if (ats > 0)
	    {
	      gdouble attack_step = 1.0 / (gdouble) ats;
	      gdouble cur_level = 0;
	      for (i = 0; i < ats; i++)
		{
		  rv[i] = (reson.re + g_random_double_range (-real_noise_level, real_noise_level)) * cur_level;
		  cur_level += attack_step;
		  BB_COMPLEX_MUL (reson, factor, reson);
		}
	    }
	  factor.re *= sound_level_decay;
	  factor.im *= sound_level_decay;
	  for (     ; i < n; i++)
	    {
	      rv[i] = reson.re + g_random_double_range (-real_noise_level, real_noise_level);
	      real_noise_level *= sound_level_decay;
	      BB_COMPLEX_MUL (reson, factor, reson);
	    }
	}
    }
  *n_samples_out = n;
  return rv;
}

BB_INIT_DEFINE(_bb_grain2_init)
{
  BbInstrument *instrument =
  bb_instrument_new_from_simple_param (G_N_ELEMENTS (grain2_params),
                                       grain2_params);
  bb_instrument_add_param (instrument, PARAM_ATTACK_TIME,
                           bb_param_spec_duration ("attack_time",
                                                   "Attack Time", NULL,
                                                   BB_DURATION_UNITS_SECONDS, 0.01, 0));
  bb_instrument_add_param (instrument, PARAM_HALFLIFE_TIME,
                           bb_param_spec_duration ("halflife_time",
                                                   "Halflife Time", NULL,
                                                   BB_DURATION_UNITS_SECONDS, 0.02, 0));
  instrument->synth_func = grain2_synth;
  bb_instrument_set_name (instrument, "grain2");
}
