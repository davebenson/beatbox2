#define _GNU_SOURCE             /* for log2() */
#include <math.h>
#include "instrument-includes.h"
#include "../core/bb-delay-line.h"

/* from piano.ins in clm-3 */

#define N_STIFFNESS_ALLPASS_FILTERS             8

#define LONGITUDINAL_MODE_CUTOFF_KEYNUM         29
#define LONGITUDINAL_MODE_STIFFNESS_COEFF       -0.5
#define GOLDEN_MEAN                             0.618
#define LOOP_GAIN_ENV_T60                       0.05
#define LOOP_GAIN_DEFAULT                       0.9999

/* TODO: not tunable */
#define N_STRINGS                               3

#define SGN(x)   (((x) < 0.0) ? -1.0 : ((x) > 0.0) ? 1.0 : 0.0)

typedef struct _KeyValuePair KeyValuePair;
struct _KeyValuePair
{
  gdouble key;		/* midi key number, i believe */
  gdouble value;
};

static KeyValuePair loud_pole_table[] =
{
  { 36, .8 },
  { 60, .85 },
  { 84, .7 },
  { 96, .6 },
  { 108, .5 },
};

static KeyValuePair soft_pole_table[] =
{
  { 36, .93 },
  { 60, .9 },
  { 84, .9 },
  { 96, .8 },
  { 108, .8 },
};


static KeyValuePair loud_gain_table[] =
{
  { 21.000, 0.700 },
  { 36.000, 0.700 },
  { 48.000, 0.700 },
  { 60.000, 0.650 },
  { 72.000, 0.650 },
  { 84.000, 0.650 },
  { 87.006, 0.681 },
  { 88.070, 0.444 },
  { 90.653, 0.606 },
  { 95.515, 0.731 },
  { 99.770, 0.775 },
  { 101.897, 0.794 },
  { 104.024, 0.800 },
  { 105.695, 0.806 },
};

static KeyValuePair soft_gain_table[] =
{
  { 21, .25 },
  { 108, .25 },
};

static KeyValuePair strike_position_table[] =
{
  { 21.000, 0.140 },
  { 23.884, 0.139 },
  { 36.000, 0.128 },
  { 56.756, 0.129 },
  { 57.765, 0.130 },
  { 59.000, 0.130 },
  { 60.000, 0.128 },
  { 61.000, 0.128 },
  { 62.000, 0.129 },
  { 66.128, 0.129 },
  { 69.000, 0.128 },
  { 72.000, 0.128 },
  { 73.000, 0.128 },
  { 79.000, 0.128 },
  { 80.000, 0.128 },
  { 96.000, 0.128 },
  { 99.000, 0.128 },
};

static KeyValuePair detuning2_table[] =
{
  { 22.017, -0.090 },
  { 23.744, -0.090 },
  { 36.000, -0.080 },
  { 48.055, -0.113 },
  { 60.000, -0.135 },
  { 67.264, -0.160 },
  { 72.000, -0.200 },
  { 84.054, -0.301 },
  { 96.148, -0.383 },
  { 108, -0.383 },
};

static KeyValuePair detuning3_table[] =
{
  { 21.435, 0.027 },
  { 23.317, 0.043 },
  { 36.000, 0.030 },
  { 48.000, 0.030 },
  { 60.000, 0.030 },
  { 72.000, 0.020 },
  { 83.984, 0.034 },
  { 96.000, 0.034 },
  { 99.766, 0.034 },
};

static KeyValuePair stiffness_coeff_table[] =
{
  { 21.000, -0.920 },
  { 24.000, -0.900 },
  { 36.000, -0.700 },
  { 48.000, -0.250 },
  { 60.000, -0.100 },
  { 75.179, -0.040 },
  { 82.986, -0.040 },
  { 92.240, -0.040 },
  { 96.000, -0.040 },
  { 99.000, .2 },
  { 108.000, .5 },
};

static KeyValuePair single_string_decay_rate_table[] =
{
  { 21.678, -2.895 },
  { 24.000, -3.000 },
  { 36.000, -4.641 },
  { 41.953, -5.867 },
  { 48.173, -7.113 },
  { 53.818, -8.016 },
  { 59.693, -8.875 },
  { 66.605, -9.434 },
  { 73.056, -10.035 },
  { 78.931, -10.293 },
  { 84.000, -12.185 },
};

static KeyValuePair single_string_zero_table[] =
{
  { 21.000, -0.300 },
  { 24.466, -0.117 },
  { 28.763, -0.047 },
  { 36.000, -0.030 },
  { 48.000, -0.020 },
  { 60.000, -0.010 },
  { 72.000, -0.010 },
  { 84.000, -0.010 },
  { 96.000, -0.010 },
};

static KeyValuePair single_string_pole_table[] =
{
  { 21.000, 0 },
  { 24.466, 0 },
  { 28.763, 0 },
  { 36.000, 0 },
  { 108, 0 },
};

static KeyValuePair release_loop_gain_table[] =
{
  { 21.643, 0.739 },
  { 24.000, 0.800 },
  { 36.000, 0.880 },
  { 48.000, 0.910 },
  { 60.000, 0.940 },
  { 72.000, 0.965 },
  { 84.000, 0.987 },
  { 88.99, .987 },
  { 89.0, 1.0 },
  { 108, 1.0 },
};

static KeyValuePair dry_tap_filter_coeff_T60_table[] =
{
  { 36, .35 },
  { 60, .25 },
  { 108, .15 },
};

static KeyValuePair dry_tap_filter_coeff_target_table[] =
{
  { 36, -.8 },
  { 60, -.5 },
  { 84, -.4 },
  { 108, -.1 },
};

static KeyValuePair dry_tap_filter_coeff_current_table[] =
{
  { 0, 0 },
  { 200, 0 },
};

static KeyValuePair dry_tap_amp_T60_table[] =
{
  { 36, .55 },
  { 60, .5 },
  { 108, .45 },
};

static KeyValuePair sustain_pedal_level_table[] =
{
  { 21.000, 0.250 },
  { 24.000, 0.250 },
  { 36.000, 0.200 },
  { 48.000, 0.125 },
  { 60.000, 0.075 },
  { 72.000, 0.050 },
  { 84.000, 0.030 },
  { 96.000, 0.010 },
  { 99.000, 0.010 },
};

static KeyValuePair pedal_resonance_pole_table[] =
{
  { 20.841, 0.534 },
  { 21.794, 0.518 },
  { 33.222, 0.386 },
  { 45.127, 0.148 },
  { 55.445, -0.065 },
  { 69.255, -0.409 },
  { 82.905, -0.729 },
  { 95.763, -0.869 },
  { 106.398, -0.861 },
};

static KeyValuePair pedal_envelope_T60_table[] =
{
  { 21.0, 7.5 },
  { 108.0, 7.5 },
};

static KeyValuePair soundboard_cutoff_T60_table[] =
{
  { 21.0, .25 },
  { 108.0, .25 },
};

static KeyValuePair dry_pedal_resonance_factor_table[] =
{
  { 21.0, .5 },
  { 108.0, .5 },
};


static KeyValuePair una_corda_gain_table[] =
{
  { 21, 1.0 },
  { 24, .4 },
  { 29, .1 },
  { 29.1, .95 },
  { 108, .95 },
};

/* convert to a key-number, such that 60==middle c; so middle a==57. */
static inline gdouble
frequency_to_key (gdouble freq)
{
  return log2 (freq / 440.0) * 12.0 + 57.0;
}

static inline gdouble
T60_to_decay_rate (gdouble sampling_rate,
                   gdouble T60)
{
  /* hmm, this doesn't really seem right...
     T60 is supposed to be the number of seconds to decay 60dB... */
  return 1.0 - pow (0.001, 1.0 / (T60 * sampling_rate));
}

static inline gdouble
interpolate_from_key  (gdouble key,
                       guint   n_kv,
                       const KeyValuePair *kv)
{
  guint start = 0, count = n_kv - 1;
  const KeyValuePair *interp;
  if (key <= kv[0].key)
    return kv[0].value;
  if (key >= kv[n_kv-1].key)
    return kv[n_kv-1].value;
  while (count > 1)
    {
      guint mid = start + count / 2;
      if (kv[mid].key <= key)
        {
          count = start + count - mid;
          start = mid;
        }
      else
        {
          count /= 2;
        }
    }
  g_assert (count == 1);
  interp = kv + start;
  return (key - interp[0].key)
       * (interp[1].value - interp[0].value)
       / (interp[1].key - interp[0].key) + interp[0].value;
}
#define INTERPOLATE_FROM_KEY(key, static_table) \
  interpolate_from_key(key, G_N_ELEMENTS (static_table), (static_table))

/* --- expseg: move a certain fraction of the way to a target
               value on each tick --- */
typedef struct _ExpRateLimiter ExpRateLimiter;
struct _ExpRateLimiter
{
  gdouble current, target, rate;
};
#define EXP_RATE_LIMITER_INIT(current,target,rate) {current,target,rate}
static inline gdouble
exp_rate_limiter_tick (ExpRateLimiter *limiter)
{
  limiter->current += (limiter->target - limiter->current) * limiter->rate;
  return limiter->current;
}

/* --- One-Pole Filter --- */
typedef struct _OnePole OnePole;
struct _OnePole
{
  gdouble y1;
  gdouble a0, b1;
};
static inline void
one_pole_init (OnePole *one_pole,
               gdouble  a0,
               gdouble  b1)
{
  one_pole->a0 = a0;
  one_pole->b1 = b1;
  one_pole->y1 = 0;
}

static inline gdouble
one_pole_tick (OnePole *filter,
               gdouble  input)
{
  gdouble output = filter->a0 * input
                 - filter->b1 * filter->y1;
  filter->y1 = output;
  return output;
}

/* --- Signal-Controlled One-Pole Lowpass Filter --- */
typedef struct _OnePoleSwept OnePoleSwept;
struct _OnePoleSwept
{
  gdouble y1;
};
#define ONE_POLE_SWEPT_INIT() {0}
static inline gdouble
one_pole_swept_tick (OnePoleSwept *filter,
                     gdouble       input,
                     gdouble       coeff)
{
  filter->y1 = ((1.0 + coeff) * input) - (coeff * filter->y1);
  return filter->y1;
}

/* --- One-Pole Allpass Filter --- */
typedef struct _OnePoleAllpass OnePoleAllpass;
struct _OnePoleAllpass
{
  gdouble a0;
  gdouble x1, y1;
};
static inline void
one_pole_allpass_init (OnePoleAllpass *filter,
                       gdouble         a0)
{
  filter->a0 = a0;
  filter->x1 = 0;
  filter->y1 = 0;
}
static inline gdouble
one_pole_allpass_tick (OnePoleAllpass *filter,
                       gdouble         input)
{
  gdouble output = filter->a0 * (input - filter->y1) + filter->x1;
  filter->x1 = input;
  filter->y1 = output;
  return output;
}

/* --- One-Pole One-Zero Filter --- */
typedef struct _OnePoleOneZero OnePoleOneZero;
struct _OnePoleOneZero
{
  gdouble a0, a1, b1;
  gdouble x1, y1;
};
#define ONE_POLE_ONE_ZERO_INIT(a0,a1,b1) {a0,a1,b1,0,0}
static inline void
one_pole_one_zero_init (OnePoleOneZero *filter,
                        gdouble         a0,
                        gdouble         a1,
                        gdouble         b1)
{
  filter->a0 = a0;
  filter->a1 = a1;
  filter->b1 = b1;
  filter->x1 = 0;
  filter->y1 = 0;
}
static inline gdouble
one_pole_one_zero_tick (OnePoleOneZero *filter,
                        gdouble         input)
{
  gdouble output = filter->a0 * input
                 + filter->a1 * filter->x1
                 - filter->b1 * filter->y1;
  filter->x1 = input;
  filter->y1 = output;
  return output;
}

/* --- "Very Special Noise Generator" --- */
typedef struct _NoiseGenerator NoiseGenerator;
struct _NoiseGenerator
{
  guint noise_seed;
};
#define NOISE_GENERATOR_INIT() {16383}
static inline void
noise_generator_init (NoiseGenerator *noise_gen)
{
  noise_gen->noise_seed = 16383;
}
static inline gdouble
noise_generator_tick_1 (NoiseGenerator *noise_gen)
{
  guint seed = noise_gen->noise_seed * 1103515245 + 12345;
  gdouble rv = ((seed>>16) & 0xffff) * 0.0000305185 - 1.0;
  noise_gen->noise_seed = seed;
  return rv;
}
#define noise_generator_tick(noise_gen, amp) ((amp) * noise_generator_tick_1(noise_gen))

static void
init_tuned_delay (gdouble          delay,
                  gdouble          rad_per_sample,
                  BbDelayLine    **int_delay_out,
                  OnePoleAllpass  *tuning_filter_out)
{
  guint int_delay;
  gdouble frac;
  gdouble coeff;

  /* from apfloor */
  {
    gdouble floor_value = floor (delay);
    gdouble floor_frac = delay - floor_value;
    if (floor_value >= 1.0 && floor_frac < GOLDEN_MEAN)
      {
        int_delay = floor_value - 1;
        frac = floor_frac + 1;
      }
    else
      {
        int_delay = floor_value;
        frac = floor_frac;
      }
  }

  /* from get-allpass-coeff */
  gdouble ta = tan (-frac * rad_per_sample);
  gdouble c = cos (rad_per_sample);
  gdouble s = cos (rad_per_sample);
  coeff = (-ta + SGN (ta) * sqrt ((1.0 + ta*ta) * s*s)) / (c*ta - s);

  if (int_delay == 0)
    *int_delay_out = NULL;
  else
    *int_delay_out = bb_delay_line_new (int_delay);
  one_pole_allpass_init (tuning_filter_out, coeff);
}

static inline gdouble
my_delay_line_add (BbDelayLine *line, gdouble in)
{
  return (line == NULL) ? in : bb_delay_line_add (line, in);
}

typedef struct _StringModel StringModel;
struct _StringModel
{
  BbDelayLine *delay_line;
  OnePoleAllpass tuning_allpass;
  OnePoleAllpass stiffness_filters[N_STIFFNESS_ALLPASS_FILTERS];
  gdouble junction_input;
};


/* TODO: test against lisp version */
static inline double
allpass_phase_change (gdouble      a1,  /* coeff */
                      gdouble      wT)  /* rad/sample */
{
  return atan2 ((a1*a1 - 1.0) * sin (wT),
                2.0*a1 + (a1*a1 + 1.0) * cos (wT));
}

/* TODO: test against lisp version */
static inline double
opoz_phase_change (gdouble b0, gdouble b1, gdouble a1, gdouble wT)
{
  gdouble s = sin (wT);
  gdouble c = cos (wT);
  return atan2 (a1*s*(b0+b1*c) - b1*s*(1.0+a1*c),
                (b0+b1*c) * (1+a1*c) + b1*s*a1*s);
}

static inline void
string_model_init (StringModel *model,
                   gdouble      sampling_rate,
                   gdouble      freq,
                   gdouble      stiffness,
                   gdouble      b0,
                   gdouble      b1,
                   gdouble      a1)
{
  gdouble wT = freq * BB_2PI / sampling_rate;
  gdouble apPhase = allpass_phase_change (stiffness, wT);
  gdouble oposPhase = opoz_phase_change (1.0+3.0*b0, a1+3.0*b1, a1, wT);
  double delay;
  guint i;

  delay = (BB_2PI + N_STIFFNESS_ALLPASS_FILTERS * apPhase + oposPhase) / wT;
  init_tuned_delay (delay, wT, &model->delay_line, &model->tuning_allpass);

  for (i = 0; i < N_STIFFNESS_ALLPASS_FILTERS; i++)
    one_pole_allpass_init (model->stiffness_filters + i, stiffness);

  model->junction_input = 0;
}

static inline gdouble
string_model_tick (StringModel *model,
                   gdouble      excitation,
                   gdouble      coupling_filter_output,
                   gdouble      loop_gain)
{
  gdouble tmp = model->junction_input + coupling_filter_output;
  guint i;
  for (i = 0; i < N_STIFFNESS_ALLPASS_FILTERS; i++)
    tmp = one_pole_allpass_tick (&model->stiffness_filters[i], tmp);
  tmp = one_pole_allpass_tick (&model->tuning_allpass, tmp);
  tmp = my_delay_line_add (model->delay_line, tmp);
  model->junction_input = tmp * loop_gain + excitation;
  return model->junction_input;
}

/* --- main function --- */
enum
{
  PARAM_FREQUENCY,
  PARAM_STRIKE_VELOCITY,
  PARAM_NOISE_GAIN,
  PARAM_RELEASE_TIME,
  PARAM_DURATION,
  PARAM_PEDAL_DOWN,
};

static BbInstrumentSimpleParam piano1_params[] =
{
  { PARAM_FREQUENCY, "frequency", 440.0 },
  { PARAM_STRIKE_VELOCITY, "strike_velocity", 0.5 },
  { PARAM_NOISE_GAIN, "noise_gain", 0.5 },
};

static double *
piano1_synth  (BbInstrument *instrument,
               BbRenderInfo *render_info,
               GValue       *params,
               guint        *n_samples_out)
{
  gdouble frequency = g_value_get_double (params + PARAM_FREQUENCY);
  gboolean pedal_down = g_value_get_boolean (params + PARAM_PEDAL_DOWN);
  gboolean strike_velocity = g_value_get_double (params + PARAM_STRIKE_VELOCITY);
  gdouble noise_gain = g_value_get_double (params + PARAM_NOISE_GAIN);
  gdouble key = frequency_to_key (frequency);
  gdouble rate = render_info->sampling_rate;
  gdouble rad_per_sample = frequency * BB_2PI / rate;
  guint n = render_info->note_duration_samples;

  guint i;

  /* a lot of what follows were params in the original piano,
     but i'm too lazy to bother. */
  gdouble detuning_factor = 1.0;
  gdouble stiffness_factor = 1.0;
  gdouble pedal_presence_factor = 0.3;
  gdouble longitudinal_mode = 10.5;
  gdouble strike_position_inv_factor = -0.9;
  gdouble single_string_decay_rate_factor = 1.0;
  gdouble loud_pole = INTERPOLATE_FROM_KEY (key, loud_pole_table);
  gdouble soft_pole = INTERPOLATE_FROM_KEY (key, soft_pole_table);
  gdouble loud_gain = INTERPOLATE_FROM_KEY (key, loud_gain_table);
  gdouble soft_gain = INTERPOLATE_FROM_KEY (key, soft_gain_table);
  gdouble strike_position = INTERPOLATE_FROM_KEY (key, strike_position_table);
  gdouble detuning2 = INTERPOLATE_FROM_KEY (key, detuning2_table);
  gdouble detuning3 = INTERPOLATE_FROM_KEY (key, detuning3_table);
  gdouble stiffness_coeff = INTERPOLATE_FROM_KEY (key, stiffness_coeff_table);
  gdouble single_string_decay_rate = INTERPOLATE_FROM_KEY (key, single_string_decay_rate_table)
                                   * single_string_decay_rate_factor;
  gdouble single_string_zero = INTERPOLATE_FROM_KEY (key, single_string_zero_table);
  gdouble single_string_pole = INTERPOLATE_FROM_KEY (key, single_string_pole_table);
  gdouble release_loop_gain = INTERPOLATE_FROM_KEY (key, release_loop_gain_table);
  gdouble dry_tap_filter_coeff_T60 = INTERPOLATE_FROM_KEY (key, dry_tap_filter_coeff_T60_table);
  gdouble dry_tap_filter_coeff_target = INTERPOLATE_FROM_KEY (key, dry_tap_filter_coeff_target_table);
  gdouble dry_tap_filter_coeff_current = INTERPOLATE_FROM_KEY (key, dry_tap_filter_coeff_current_table);
  gdouble dry_tap_amp_T60 = INTERPOLATE_FROM_KEY (key, dry_tap_amp_T60_table);
  gdouble sustain_pedal_level = INTERPOLATE_FROM_KEY (key, sustain_pedal_level_table);
  gdouble pedal_resonance_pole = INTERPOLATE_FROM_KEY (key, pedal_resonance_pole_table);
  gdouble pedal_envelope_T60 = INTERPOLATE_FROM_KEY (key, pedal_envelope_T60_table);
  gdouble soundboard_cutoff_T60 = INTERPOLATE_FROM_KEY (key, soundboard_cutoff_T60_table);
  gdouble dry_pedal_resonance_factor = INTERPOLATE_FROM_KEY (key, dry_pedal_resonance_factor_table);
  gdouble una_corda_gain = INTERPOLATE_FROM_KEY (key, una_corda_gain_table);

  /* initialize soundboard impulse response elements */
  OnePoleOneZero dry_tap_filter = ONE_POLE_ONE_ZERO_INIT (1.0, 0.0, 0.0);
  gdouble dry_tap_filter_coeff_change_rate = T60_to_decay_rate (rate, dry_tap_filter_coeff_T60);
  ExpRateLimiter dry_tap_coeff_limiter = EXP_RATE_LIMITER_INIT (dry_tap_filter_coeff_current,
                                                                dry_tap_filter_coeff_target,
                                                                dry_tap_filter_coeff_change_rate);
  OnePoleSwept dry_tap_one_pole_swept = ONE_POLE_SWEPT_INIT ();
  gdouble dry_tap_amp_change_rate = T60_to_decay_rate (rate, dry_tap_amp_T60);
  ExpRateLimiter dry_tap_amp_limiter = EXP_RATE_LIMITER_INIT (1.0, 0.0, dry_tap_amp_change_rate);

  /* initialize open-string resonance elements */
  OnePoleOneZero wet_tap_filter = ONE_POLE_ONE_ZERO_INIT (-ABS (pedal_resonance_pole), 0.0,
                                                          -pedal_resonance_pole);
  gdouble wet_tap_coeff_change_rate = T60_to_decay_rate (rate, pedal_envelope_T60);
  ExpRateLimiter wet_tap_coeff_limiter = EXP_RATE_LIMITER_INIT (0.0, -0.5, wet_tap_coeff_change_rate);
  OnePoleSwept wet_tap_one_pole_swept = ONE_POLE_SWEPT_INIT ();
  ExpRateLimiter wet_tap_amp_limiter
    = EXP_RATE_LIMITER_INIT (sustain_pedal_level * pedal_presence_factor
                             * (pedal_down ? 1.0 : dry_pedal_resonance_factor),
                             0.0, T60_to_decay_rate (rate, pedal_envelope_T60));
  gdouble soundboard_cutoff_rate = T60_to_decay_rate (rate, soundboard_cutoff_T60);

  /* initialize velocity-dependent piano hammer filter elements */
  gdouble hammer_pole = soft_pole + (loud_pole - soft_pole) * strike_velocity;
  gdouble hammer_gain = soft_gain + (loud_gain - soft_gain) * strike_velocity;
  OnePole hammer_one_poles[4];
  for (i = 0; i < 4; i++)
    one_pole_init (hammer_one_poles + i, 1.0 - hammer_pole, -hammer_pole);

  gdouble agraffe_len = rate * strike_position / frequency;
  BbDelayLine *agraffe_delay_line;
  OnePoleAllpass agraffe_tuning_filter;
  init_tuned_delay (agraffe_len, rad_per_sample, &agraffe_delay_line, &agraffe_tuning_filter);
  gdouble attenuation_per_period = pow (10.0, single_string_decay_rate / (frequency * 20));
  gdouble cfb0, cfb1, cfa1;
  {
    gdouble g = attenuation_per_period;           /* dc gain */
    gdouble b = single_string_zero;
    gdouble a = single_string_pole;
    gdouble ctemp = 1.0 -b + g - a*g + N_STRINGS * (1.0 - b - g + a*g);
    cfb0 = 2.0 * (-1 + b + g - a*g) / ctemp;
    cfb1 = 2.0 * (a - a*b - b*g - a*b*g) / ctemp;
    cfa1 = (-a + a*b - b*g + a*b*g + N_STRINGS * (-a + a*b + b*g - a*b*g)) / ctemp;
  }
  OnePoleOneZero coupling_filter = ONE_POLE_ONE_ZERO_INIT (cfb0, cfb1, cfa1);

  /* determine string tunings (and longitudinal modes, if present) */
  StringModel strings[3];

  if (stiffness_factor > 1.0)
    stiffness_coeff -= (1.0 + stiffness_coeff) / (stiffness_factor - 1.0);
  else
    stiffness_coeff *= stiffness_factor;

  if (key < LONGITUDINAL_MODE_CUTOFF_KEYNUM)
    string_model_init (&strings[0],
                       rate,
                       frequency * longitudinal_mode,
                       LONGITUDINAL_MODE_STIFFNESS_COEFF,
                       cfb0, cfb1, cfa1);
  else
    string_model_init (&strings[0], rate, frequency, stiffness_coeff,
                       cfb0, cfb1, cfa1);
  string_model_init (&strings[1], rate, frequency + detuning2 * detuning_factor, stiffness_coeff,
                     cfb0, cfb1, cfa1);
  string_model_init (&strings[2], rate, frequency + detuning3 * detuning_factor, stiffness_coeff,
                     cfb0, cfb1, cfa1);

  gdouble loop_gain_change_rate = T60_to_decay_rate (rate, LOOP_GAIN_ENV_T60);
  ExpRateLimiter loop_gain_limiter = EXP_RATE_LIMITER_INIT (LOOP_GAIN_DEFAULT,
                                                            release_loop_gain,
                                                            loop_gain_change_rate);

  gdouble dry_tap = 0.0;
  gdouble open_strings = 0.0;
  gdouble combed_excitation_signal = 0.0;
  gdouble adel_out = 0.0;
  gdouble adel_in = 0.0;
  gdouble total_tap = 0.0;
  gdouble loop_gain = LOOP_GAIN_DEFAULT;
  gboolean is_release_time = FALSE;
  NoiseGenerator noise_gen = NOISE_GENERATOR_INIT();
  gdouble rel_samples_d = bb_value_get_duration_as_samples (params + PARAM_RELEASE_TIME, render_info);
  guint release_sample = ((int)rel_samples_d > (int)n) ? 0 : (n - (int) rel_samples_d);
  gdouble *rv = g_new (gdouble, n);

  for (i = 0; i < n; i++)
    {
      guint j;
      gdouble tmp;
      if (is_release_time)
        loop_gain = exp_rate_limiter_tick (&loop_gain_limiter);
      else if (i == release_sample)
        {
          is_release_time = TRUE;
          dry_tap_amp_limiter.rate = soundboard_cutoff_rate;
          wet_tap_amp_limiter.rate = soundboard_cutoff_rate;
        }

      dry_tap = exp_rate_limiter_tick (&dry_tap_amp_limiter)
              * one_pole_swept_tick (&dry_tap_one_pole_swept,
                                     one_pole_one_zero_tick (&dry_tap_filter,
                                                             noise_generator_tick (&noise_gen, noise_gain)),
                                     exp_rate_limiter_tick (&dry_tap_coeff_limiter));

      open_strings
              = exp_rate_limiter_tick (&wet_tap_amp_limiter)
              * one_pole_swept_tick (&wet_tap_one_pole_swept,
                                     one_pole_one_zero_tick (&wet_tap_filter,
                                                             noise_generator_tick (&noise_gen, noise_gain)),
                                     exp_rate_limiter_tick (&wet_tap_coeff_limiter));
      total_tap = dry_tap + open_strings;
      tmp = total_tap;
      for (j = 0; j < 4; j++)
        tmp = one_pole_tick (hammer_one_poles + j, tmp);
      adel_in = tmp;
      combed_excitation_signal = hammer_gain * (adel_out + adel_in * strike_position_inv_factor);
      adel_out = one_pole_allpass_tick (&agraffe_tuning_filter,
                                        my_delay_line_add (agraffe_delay_line, adel_in));
      tmp = string_model_tick (strings + 0, combed_excitation_signal * una_corda_gain,
                               coupling_filter.y1, loop_gain);
      for (j = 1; j < N_STRINGS; j++)
        tmp += string_model_tick (strings + j, combed_excitation_signal,
                                  coupling_filter.y1, loop_gain);
      one_pole_one_zero_tick (&coupling_filter, tmp);
      rv[i] = tmp;
    }
  for (i = 0; i < 3; i++)
    if (strings[i].delay_line != NULL)
      bb_delay_line_free (strings[i].delay_line);
  if (agraffe_delay_line != NULL)
    bb_delay_line_free (agraffe_delay_line);
  *n_samples_out = n;
  return rv;
}

BB_INIT_DEFINE(_bb_piano1_init)
{
  BbInstrument *instrument =
  bb_instrument_new_from_simple_param (G_N_ELEMENTS (piano1_params),
                                       piano1_params);
  bb_instrument_add_param (instrument, PARAM_RELEASE_TIME,
                           bb_param_spec_duration ("release-time", "Release Time", NULL,
                                                   BB_DURATION_UNITS_SECONDS, 0.5, 0));
  bb_instrument_add_param (instrument, PARAM_DURATION, param_spec_duration ());
  bb_instrument_add_param (instrument, PARAM_PEDAL_DOWN,
                           g_param_spec_boolean ("pedal-down", "Pedal Down", NULL, FALSE, 0));
  bb_instrument_set_name (instrument, "piano1");
  instrument->synth_func = piano1_synth;
}
