#include "instrument-includes.h"

enum
{
  PARAM_FREQUENCY,
  PARAM_MIN_SOUND_LEVEL,
  PARAM_GAIN,
  PARAM_NOISE_HALFLIFE_TIME,
  PARAM_RESONANCE_HALFLIFE_TIME,
};

#define MAX_GRAIN_LEN   20000

static const BbInstrumentSimpleParam grain0_params[] =
{
  { PARAM_FREQUENCY,       "frequency",         440.00 },
  { PARAM_MIN_SOUND_LEVEL, "min_sound_level",     0.01 },
  { PARAM_GAIN,            "gain",                1.00 }
};

static double *
grain0_synth (BbInstrument *instrument,
              BbRenderInfo *render_info,
              GValue       *params,
              guint        *n_samples_out)
{
  gdouble output_0 = 0;
  gdouble output_1 = 0;
  gdouble coeff_0, coeff_1;
  gdouble sound_level = g_value_get_double (params + PARAM_GAIN);
  gdouble min_sound_level = g_value_get_double (params + PARAM_MIN_SOUND_LEVEL);
  gdouble noise_halflife_time_samps = bb_value_get_duration_as_samples (params + PARAM_NOISE_HALFLIFE_TIME, render_info);
  gdouble reson_halflife_time_samps = bb_value_get_duration_as_samples (params + PARAM_RESONANCE_HALFLIFE_TIME, render_info);
  gdouble frequency = g_value_get_double (params + PARAM_FREQUENCY);
  guint i;
  gdouble T;
  guint n;
  double *rv;

  /* decay per sample */
  gdouble sound_level_decay = pow (0.5, 1.0 / noise_halflife_time_samps);
  gdouble reson = pow (0.5, 1.0 / reson_halflife_time_samps);

  /* initialize coeff0 and coeff1 */
  coeff_1 = reson * reson;
  coeff_0 = -reson * 2.0 * cos(frequency * BB_2PI / render_info->sampling_rate);
  
  /* The contribution of the noise input sound at sample t
     to the output at sample T, is:
         reson^t * sld^(T-t)
       = sld^T * reson^t / sld^t
       = sld^T * (reson/sld)^t
     Hence the sound output at time T is bounded by:
        T                                     T
       SUM   sld^T * (reson/sld)^t  =  sld^T SUM (reson/sld)^t 
       t=0                                   t=0
         
     If reson < sld, we can apply the geometric series formala:
         sld^T * (1-(reson/sld)^T) / (1-(reson/sld))  (when reson<sld)

     If reson > sld, 
           T                           T                               T
    sld^T SUM (reson/sld)^t = reson^T SUM (reson/sld)^(t-T) = reson^T SUM (sld/reson)^t
          t=0                         t=0                             t=0

       = reson^T * (1-(sld/reson)^T) / (1-(sld/reson))  (when reson>sld)

     If reson == sid, SUM = T * sld^T.

     So some upper bounds are:
         sld^T / (1 - reson / sld)               if reson < sld,
         reson^T / (1 - sld / reson)             if reson > sld,
         T * sld^T                               if reson == sld.

     We want to solve for T such that this upper bound is equal to min_sound_level.

     XXX: this is all bs.  the resonator doesn't last nearly as long as
     this pretends it will -- only a few cycles after the noise ends.
   */
  if (reson < sound_level_decay)
    {
      /* sld^T / (1 - reson / sld) = min_sound_level
         sld^T = min_sound_level * (1 - reson / sld)
         T = log (min_sound_level * (1 - reson / sld)) / log(sld). */
      T = log (min_sound_level * (1 - reson / sound_level_decay))
        / log (sound_level_decay);
    }
  else if (reson > sound_level_decay)
    {
      /* reson^T / (1 - sld/reson) == min_sound_level
         reson^T = min_sound_level * (1 - sld/reson)
         T = log(min_sound_level * (1 - sld/reson)) / log(reson). */
      T = log(min_sound_level * (1.0 - sound_level_decay/reson)) / log(reson);
    }
  else
    {
      /* T * sld^T == min_sound_level. */
      T = 1;
      gdouble delta_t;
      gdouble one_over_log_sld = 1.0 / log (sound_level_decay);
      do
        {
          /* T_i * sld^T_{i+1} == msl  => T_{i+1} = log(msl/T_i) / log(sld). */
          gdouble new_T = log (min_sound_level / T) * one_over_log_sld;
          delta_t = ABS (new_T - T);
          T = new_T;
        }
      while (delta_t > 1.5);
    }

  if (T > MAX_GRAIN_LEN)
    n = MAX_GRAIN_LEN;
  else
    n = (guint) T;
  rv = g_new (gdouble, n);

  for (i = 0; i < n; i++)
    {
      gdouble new_output = g_random_double_range (-sound_level, sound_level)
                         - output_0 * coeff_0
                         - output_1 * coeff_1;
      sound_level *= sound_level_decay;
      rv[i] = new_output;
      output_1 = output_0;
      output_0 = new_output;
    }
  *n_samples_out = n;
  return rv;
}

BB_INIT_DEFINE(_bb_grain0_init)
{
  BbInstrument *instrument =
  bb_instrument_new_from_simple_param (G_N_ELEMENTS (grain0_params),
                                       grain0_params);
  bb_instrument_add_param (instrument, PARAM_NOISE_HALFLIFE_TIME,
                           bb_param_spec_duration ("noise_halflife_time",
                                                   "Noise Halflife Time", NULL,
                                                   BB_DURATION_UNITS_SECONDS, 0.02, 0));
  bb_instrument_add_param (instrument, PARAM_RESONANCE_HALFLIFE_TIME,
                           bb_param_spec_duration ("resonance_halflife_time",
                                                   "Resonance Halflife Time", NULL,
                                                   BB_DURATION_UNITS_SECONDS, 0.02, 0));
  instrument->synth_func = grain0_synth;
  bb_instrument_set_name (instrument, "grain0");
}
