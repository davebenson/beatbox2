/* wavetrain vocal model */
#include "instrument-includes.h"

#define EVAL_FOR_ALL_SIMPLE_PARAMS(macro) \
  macro(PARAM_FREQUENCY,             frequency,                  440) \
  macro(PARAM_VOLUME,                volume,                       1) \
  macro(PARAM_FM_FREQUENCY,          fm_frequency,                 6) \
  macro(PARAM_FM_DEPTH,              fm_depth,                   0.1) \
  macro(PARAM_AM_FREQUENCY,          am_frequency,                 6) \
  macro(PARAM_AM_DEPTH,              am_depth,                  0.01) \
  macro(PARAM_GRAIN_FREQ_0,          grain_freq_0,               730) \
  macro(PARAM_GRAIN_AMP_0,           grain_amp_0,                0.6) \
  macro(PARAM_GRAIN_FREQ_1,          grain_freq_1,              1090) \
  macro(PARAM_GRAIN_AMP_1,           grain_amp_1,                0.3) \
  macro(PARAM_GRAIN_FREQ_2,          grain_freq_2,              2440) \
  macro(PARAM_GRAIN_AMP_2,           grain_amp_2,                0.1) \
  macro(PARAM_GRAIN_NOISE,           grain_noise,                0.0)

enum
{
#define DEF_ENUM(p,n,d) p,
  EVAL_FOR_ALL_SIMPLE_PARAMS(DEF_ENUM)
#undef DEF_ENUM
  PARAM_GRAIN_DURATION
};

static const BbInstrumentSimpleParam voice0_params[] =
{
#define ONE_ENTRY(p,n,d) { p,#n,d },
  EVAL_FOR_ALL_SIMPLE_PARAMS(ONE_ENTRY)
};

typedef struct _Oscil Oscil;
struct _Oscil
{
  BbComplex cur, fac;
};
static inline void
oscil_init (Oscil *oscil,
            gdouble amp,
            gdouble freq)
{
  oscil->cur.re = amp;
  oscil->cur.im = 0;
  oscil->fac.re = cos (freq);
  oscil->fac.im = sin (freq);
}
static inline gdouble
oscil_tick (Oscil *oscil)
{
  gdouble rv = oscil->cur.im;
  BB_COMPLEX_MUL (oscil->cur, oscil->fac, oscil->cur);
  return rv;
}

static void
add_grain (gdouble *inout,
           guint    grain_dur,
           guint    n,                  /* <= grain_dur */
           gdouble  amp0,
           gdouble  freq0,              /* frequencies are in radians/sample */
           gdouble  amp1,
           gdouble  freq1,
           gdouble  amp2,
           gdouble  freq2,
           gdouble  noise_level)
{
  Oscil oscils[3];
  Oscil env_oscil;
  guint i;
  oscil_init (oscils + 0, amp0, freq0);
  oscil_init (oscils + 1, amp1, freq1);
  oscil_init (oscils + 2, amp2, freq2);
  oscil_init (&env_oscil, 0.5, BB_2PI / grain_dur);
  if (noise_level > 0.0)
    for (i = 0; i < n; i++)
      {
        gdouble env = 0.5 + env_oscil.cur.re;
        oscil_tick (&env_oscil);
        inout[i] += env * (oscil_tick (oscils + 0) 
                         + oscil_tick (oscils + 1) 
                         + oscil_tick (oscils + 2) 
                         + g_random_double_range (-noise_level, noise_level));
      }
  else
    for (i = 0; i < n; i++)
      {
        gdouble env = 0.5 + env_oscil.cur.re;
        oscil_tick (&env_oscil);
        inout[i] += env * (oscil_tick (oscils + 0) 
                         + oscil_tick (oscils + 1) 
                         + oscil_tick (oscils + 2));
      }
}

static double *
voice0_complex_synth  (BbInstrument *instrument,
			   BbRenderInfo *render_info,
                           double        duration,
			   GValue       *init_params,
			   guint         n_events,
			   const BbInstrumentEvent *events,
			   guint        *n_samples_out)
{
  gdouble rate = render_info->sampling_rate;
  gdouble one_over_sampling_rate = 1.0 / rate;
  gdouble two_pi_over_sampling_rate = BB_2PI * one_over_sampling_rate;
  guint n = render_info->note_duration_samples;
  guint i;
#define DECLARE_AND_ALLOCATE(p,name,defval) gdouble *name = g_new (gdouble, n);
  EVAL_FOR_ALL_SIMPLE_PARAMS (DECLARE_AND_ALLOCATE)
#undef DECLARE_AND_ALLOCATE

  guint *grain_durations = g_new (guint, n);
  gdouble *rv = g_new0 (gdouble, n);


  gdouble am_at = 0;
  gdouble fm_at = 0;
  gdouble at = 0;       /* when this completes an oscillation,
                           then we will emit a grain */

  /* Scan through events, searching for grain_duration parameter changes. */
  {
    guint n_grain_durations_output = 0;
    gdouble last_grain_dur_d = bb_value_get_duration_as_samples (init_params + PARAM_GRAIN_DURATION, render_info);
    guint last_grain_dur = last_grain_dur_d;
    for (i = 0; i < n_events; i++)
      if (events[i].type == BB_INSTRUMENT_EVENT_PARAM
       && events[i].info.param.index == PARAM_GRAIN_DURATION)
        {
          gdouble d = bb_value_get_duration_as_samples (&events[i].info.param.value, render_info);
          gdouble samp_d = rate * events[i].time;
          guint samp = samp_d;
          if (samp > n)
            samp = n;
          while (n_grain_durations_output < samp)
            grain_durations[n_grain_durations_output++] = last_grain_dur;
          last_grain_dur = d;
        }
    while (n_grain_durations_output < n)
      grain_durations[n_grain_durations_output++] = last_grain_dur;
  }

#define INTERPOLATE_ONE(p,name,defval)                                  \
  bb_instrument_param_events_interpolate (instrument,                   \
                                          p,                            \
                                          init_params + p,              \
                                          render_info,                  \
                                          n_events, events,             \
                                          BB_INTERPOLATION_LINEAR,      \
                                          name);
  EVAL_FOR_ALL_SIMPLE_PARAMS (INTERPOLATE_ONE)
#undef INTERPOLATE_ONE

  for (i = 0; i < n; i++)
    {
      gdouble cur_freq = frequency[i];
      cur_freq *= (1.0 + fm_depth[i] * sin (fm_at));
      at += cur_freq * one_over_sampling_rate;
      if (at >= 1.0)
        {
          guint grain_dur, clipped_grain_dur;
          do
            {
              at -= 1.0;
            }
          while (at >= 1.0);

          /* emit grain */
          grain_dur = grain_durations[i];
          if (i + grain_dur > n)
            clipped_grain_dur = n - i;
          else
            clipped_grain_dur = grain_dur;
          
          add_grain (rv + i,
                     grain_dur, clipped_grain_dur,
                     grain_amp_0[i], grain_freq_0[i] * two_pi_over_sampling_rate,
                     grain_amp_1[i], grain_freq_1[i] * two_pi_over_sampling_rate,
                     grain_amp_2[i], grain_freq_2[i] * two_pi_over_sampling_rate,
                     grain_noise[i]);
        }

      /* update fm pointer */
      fm_at += fm_frequency[i] * two_pi_over_sampling_rate;
      if (fm_at >= BB_2PI)
        fm_at -= BB_2PI;

      /* safe to do am modulate now */
      rv[i] *= volume[i] * (1.0 + sin (am_at) * am_depth[i]);
      am_at += am_frequency[i] * two_pi_over_sampling_rate;
      if (am_at >= BB_2PI)
        am_at -= BB_2PI;
    }

#define FREE_BUF(p, n, def) g_free(n);
  EVAL_FOR_ALL_SIMPLE_PARAMS (FREE_BUF)
#undef FREE_BUF
  g_free (grain_durations);

  *n_samples_out = n;
  return rv;
}
BB_INIT_DEFINE(_bb_voice0_init)
{
  BbInstrument *instrument = bb_instrument_new_from_simple_param (G_N_ELEMENTS (voice0_params),
                                                                  voice0_params);
  bb_instrument_add_param (instrument, PARAM_GRAIN_DURATION,
                           bb_param_spec_duration ("grain-duration", "Grain Duration", NULL,
                                                   BB_DURATION_UNITS_SECONDS, 0.005, 0));
  instrument->complex_synth_func = voice0_complex_synth;
  bb_instrument_set_name (instrument, "voice0");
}
