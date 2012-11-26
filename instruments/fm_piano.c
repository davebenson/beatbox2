/* rhodey and wurley pianos.  from stk */
/* note: the original code hardcodes most of these parameters:
   users are advised to always set a preset of 'wurley' or 'rhodey'
   since the parameters decay_time[123], H[123], gain[123],
   scale_frequency[123], twozero_gain
   are somewhat interdependent. */
/* TODO: describe the params somewhere.
 */
/* basically there are 4 banks of FM: the first ("bank 0") is the harmonic
   and is the same for both pianos emulated.
   Those are left unparameterized:  that is why
   there is no delay0, H0, gain0, scale_frequency0.

   the remaining banks are parameterized by:
      decay_time            ...
      H                modulation_frequency / carrier_frequency
      gain             ...
      scale_frequency  whether to use H as an absolute freq (FALSE)
                       or a factor to multiple with frequency (TRUE)

   here are the parameters, according to Rhodey.cpp and Wurley.cpp 
   in the stk toolkit:
     ...
 */

#include "instrument-includes.h"
#include "../core/bb-filter1.h"
#include "../core/bb-waveform.h"

#define DUMP    0

#if DUMP
#define IF_DUMP(print_stmt)     print_stmt
#else
#define IF_DUMP(print_stmt)     
#endif

/* NOTE: we assume that PARAM_XXXn == PARAM_XXX0 + n.
   In other words, YOU MUST KEEP THE SIMILAR PARAMS GROUPED!!! */
enum
{
  PARAM_FREQUENCY,
  PARAM_DECAY_TIME0,
  PARAM_DECAY_TIME1,
  PARAM_DECAY_TIME2,
  PARAM_DECAY_TIME3,
  PARAM_RATIO0,          /* a ratio, if scale_frequency1, else an absolute frequency */
  PARAM_RATIO1,
  PARAM_RATIO2,
  PARAM_RATIO3,
  PARAM_GAIN0,
  PARAM_GAIN1,
  PARAM_GAIN2,
  PARAM_GAIN3,
  PARAM_VIBRATO_FREQUENCY,      /* default 6 */
  PARAM_VIBRATO_GAIN,           /* default 0 */
  PARAM_MODULATOR_INDEX,              /* default 0.5 (note this is 1/2 the value equivalent in stk) */
  PARAM_CROSSFADE,              /* default 0.5 (note this is 1/2 the value equivalent in stk) */
  PARAM_TWOZERO_GAIN,
  PARAM_SCALE_FREQUENCY0,
  PARAM_SCALE_FREQUENCY1,
  PARAM_SCALE_FREQUENCY2,
  PARAM_SCALE_FREQUENCY3,
};

enum
{
  ACTION_KEY_ON,
  ACTION_KEY_OFF
};

static BbInstrumentSimpleParam fm_piano_simple_params[] =
{
  { PARAM_FREQUENCY, "frequency", 440 },                /* defaults from 'wurley' */
  { PARAM_DECAY_TIME0, "decay_time0", 1.50 },
  { PARAM_DECAY_TIME1, "decay_time1", 1.50 },
  { PARAM_DECAY_TIME2, "decay_time2", 0.25 },
  { PARAM_DECAY_TIME3, "decay_time3", 0.15 },
  { PARAM_RATIO0, "ratio0", 1.0 },
  { PARAM_RATIO1, "ratio1", 4.0 },
  { PARAM_RATIO2, "ratio2", 510.0 },
  { PARAM_RATIO3, "ratio3", 510.0 },
  { PARAM_GAIN0, "gain0", 1.000 },                /* get_fm_gain(99) */
  { PARAM_GAIN1, "gain1", 0.308 },                /* get_fm_gain(82) */
  { PARAM_GAIN2, "gain2", 0.616 },                /* get_fm_gain(92) */
  { PARAM_GAIN3, "gain3", 0.117 },                /* get_fm_gain(68) */
  { PARAM_VIBRATO_FREQUENCY, "vibrato_frequency", 8.0 },
  { PARAM_VIBRATO_GAIN, "vibrato_gain", 0.394 },
  { PARAM_MODULATOR_INDEX, "modulator_index", 0.5 },
  { PARAM_CROSSFADE, "crossfade", 0.5 },
  { PARAM_TWOZERO_GAIN, "twozero_gain", 1.0 },
};

typedef struct _FmPianoOsc FmPianoOsc;
typedef struct _FmPianoState FmPianoState;

typedef enum
{
  FM_PIANO_OSC_OFF,
  FM_PIANO_OSC_ATTACK,
  FM_PIANO_OSC_DECAY,
} FmPianoOscMode;

struct _FmPianoOsc
{
  FmPianoOscMode adsr_mode;
  gdouble adsr_pos;
  gdouble adsr_decay_step;

  gdouble pos;
  gdouble step;

  BbWaveform *waveform;

  /* if scale_freq, a ratio of this osc's frequency to note frequency.
     if !scale_freq, a constant frequency. */
  gdouble ratio;
  gboolean scale_freq;

  gdouble decay_time;
  gdouble gain;
};

struct _FmPianoState
{
  double sampling_rate;
  FmPianoOsc subs[4];
  BbFilter1 *twozero;
  gdouble twozero_gain;
  gdouble frequency;
  gdouble adsr_attack_step;
  gdouble vibrato_pos;
  gdouble vibrato_step;
  gdouble vibrato_gain;
  gdouble modulator_index, crossfade; /* range from 0..1 */
};

static gdouble
get_fm_gain (gdouble log_value)
{
  return exp (-0.069314709 * (99.0 - log_value));
}

static inline void
recompute_steps (FmPianoState *state,
                 guint         which)
{
  gdouble rate = state->sampling_rate;
  FmPianoOsc *osc = state->subs + which;
  gdouble freq = (osc->scale_freq ? state->frequency : 1.0) * osc->ratio;
  osc->adsr_decay_step = 1.0 / (rate * osc->decay_time);
  osc->step = freq / rate;
}

static inline gdouble
adsr_tick (FmPianoState *state,
           guint         which)
{
  FmPianoOsc *osc = state->subs + which;
  switch (osc->adsr_mode)
    {
    case FM_PIANO_OSC_OFF:
      return 0;
    case FM_PIANO_OSC_ATTACK:
      osc->adsr_pos += state->adsr_attack_step;
      if (osc->adsr_pos >= 1.0)
        {
          osc->adsr_pos = 1.0;
          osc->adsr_mode = FM_PIANO_OSC_DECAY;
        }
      return osc->adsr_pos;
    case FM_PIANO_OSC_DECAY:
      osc->adsr_pos -= osc->adsr_decay_step;
      if (osc->adsr_pos <= 0.0)
        {
          osc->adsr_pos = 0.0;
          osc->adsr_mode = FM_PIANO_OSC_OFF;
        }
      return osc->adsr_pos;
    default:
      g_return_val_if_reached (0);
    }
}
static inline gdouble
wave_tick (FmPianoState *state,
           guint         which,
           gdouble       phase_offset)
{
  FmPianoOsc *osc = state->subs + which;
  gdouble rv;
  gdouble in = osc->pos + phase_offset;
  if (in < 0)
    {
      in += 1;
      while (in < 0)
        in += 1;
    }
  else if (in >= 1.0)
    {
      in -= 1;
      while (in >= 1.0)
        in -= 1;
    }
  rv = bb_waveform_eval (osc->waveform, in);
  osc->pos += osc->step;
  if (osc->pos >= 1.0)
    osc->pos -= 1.0;
  return rv;
}

#if DUMP
static inline gdouble
_adsr_tick (FmPianoState *state,
            guint         which)
{
  gdouble rv = _adsr_tick (state,which);
  g_print ("adsr_tick[%u]: %.6f\n", which, rv);
  return rv;
}

static inline gdouble
_wave_tick (FmPianoState *state,
            guint         which,
            gdouble       phase_offset)
{
  gdouble old_pos = state->subs[which].pos;
  gdouble rv = wave_tick (state, which, phase_offset);
  g_print ("wave_tick[%u]: pos=%.6f, phase_offset=%.6f; output=%.6f\n",
            which, old_pos, phase_offset, rv);
  return rv;
}
#define wave_tick _wave_tick
#define adsr_tick _adsr_tick
#endif

static double *
fm_piano_complex_synth  (BbInstrument *instrument,
                         BbRenderInfo *render_info,
                         double        duration,
                         GValue       *init_params,
                         guint         n_events,
                         const BbInstrumentEvent *events,
                         guint        *n_samples_out)
{
  double rate = render_info->sampling_rate;
  guint n = render_info->note_duration_samples;
  FmPianoState state;
  guint which;
  guint sample_at = 0, event_at = 0;
  gdouble twozero_last_out = 0;
  gdouble *rv = g_new (gdouble, n);
  state.sampling_rate = rate;
  state.adsr_attack_step = 1.0 / (rate * 0.001);
  state.twozero = bb_filter1_new_two_zero (1.0, 0.0, -1.0);
  state.twozero_gain = g_value_get_double (init_params + PARAM_TWOZERO_GAIN);
  state.vibrato_pos = 0;
  state.vibrato_step = g_value_get_double (init_params + PARAM_VIBRATO_FREQUENCY) * BB_2PI / rate;
  state.vibrato_gain = g_value_get_double (init_params + PARAM_VIBRATO_GAIN);
  state.frequency = g_value_get_double (init_params + PARAM_FREQUENCY);
  state.modulator_index = g_value_get_double (init_params + PARAM_MODULATOR_INDEX);
  state.crossfade = g_value_get_double (init_params + PARAM_CROSSFADE);
  for (which = 0; which < 4; which++)
    {
      state.subs[which].adsr_mode = FM_PIANO_OSC_OFF;
      state.subs[which].adsr_pos = 0;
      state.subs[which].ratio = g_value_get_double (init_params + PARAM_RATIO0 + which);
      state.subs[which].decay_time = g_value_get_double (init_params + PARAM_DECAY_TIME0 + which);
      state.subs[which].gain = g_value_get_double (init_params + PARAM_GAIN0 + which);
      state.subs[which].scale_freq = g_value_get_boolean (init_params + PARAM_SCALE_FREQUENCY0 + which);
      state.subs[which].pos = 0;
      if (which == 3)
        state.subs[which].waveform = bb_waveform_new_abssinblank ();
      else
        state.subs[which].waveform = bb_waveform_new_sin ();
      recompute_steps (&state, which);
    }
  while (sample_at < n)
    {
      guint end_sample = n;
      gboolean must_recompute_steps[4] = { FALSE, FALSE, FALSE, FALSE };
      while (event_at < n_events)
        {
          guint event_sample = (int)(events[event_at].time * rate);
          if (event_sample > sample_at)
            {
              end_sample = event_sample;
              break;
            }
          else if (events[event_at].type == BB_INSTRUMENT_EVENT_PARAM)
            {
              const GValue *v = &events[event_at].info.param.value;
              guint param = events[event_at].info.param.index;
              switch (param)
                {
                case PARAM_FREQUENCY:
                  state.frequency = g_value_get_double (v);
                  for (which = 0; which < 4; which++)
                    must_recompute_steps[which] = TRUE;
                  break;
                case PARAM_DECAY_TIME0:
                case PARAM_DECAY_TIME1:
                case PARAM_DECAY_TIME2:
                case PARAM_DECAY_TIME3:
                  which = param - PARAM_DECAY_TIME0;
                  state.subs[which].decay_time = g_value_get_double (v);
                  must_recompute_steps[which] = TRUE;
                  break;
                case PARAM_RATIO0:
                case PARAM_RATIO1:
                case PARAM_RATIO2:
                case PARAM_RATIO3:
                  which = param - PARAM_RATIO0;
                  state.subs[which].ratio = g_value_get_double (v);
                  must_recompute_steps[which] = TRUE;
                  break;
                case PARAM_GAIN0:
                case PARAM_GAIN1:
                case PARAM_GAIN2:
                case PARAM_GAIN3:
                  which = param - PARAM_GAIN0;
                  state.subs[which].gain = g_value_get_double (v);
                  break;
                case PARAM_SCALE_FREQUENCY0:
                case PARAM_SCALE_FREQUENCY1:
                case PARAM_SCALE_FREQUENCY2:
                case PARAM_SCALE_FREQUENCY3:
                  which = param - PARAM_SCALE_FREQUENCY0;
                  state.subs[which].scale_freq = g_value_get_boolean (v);
                  must_recompute_steps[which] = TRUE;
                  break;
                case PARAM_VIBRATO_FREQUENCY:
                  state.vibrato_step = BB_2PI * g_value_get_double (v) / rate;
                  break;
                case PARAM_VIBRATO_GAIN:
                  state.vibrato_gain = g_value_get_double (v);
                  break;
                case PARAM_MODULATOR_INDEX:
                  state.modulator_index = g_value_get_double (v);
                  break;
                case PARAM_CROSSFADE:
                  state.crossfade = g_value_get_double (v);
                  break;
                case PARAM_TWOZERO_GAIN:
                  state.twozero_gain = g_value_get_double (v);
                  break;
                default:
                  g_error ("%s, %u: no handling for event for param %s",
                           __FILE__, __LINE__,
                           instrument->param_specs[events[event_at].info.param.index]->name);
                }
            }
          else if (events[event_at].type == BB_INSTRUMENT_EVENT_ACTION)
            {
              //GValue *args = events[event_at].info.action.args;
              switch (events[event_at].info.action.index)
                {
                case ACTION_KEY_ON:
                  {
                    /* enter attack mode */
                    for (which = 0; which < 4; which++)
                      state.subs[which].adsr_mode = FM_PIANO_OSC_ATTACK;
                    break;
                  }
                case ACTION_KEY_OFF:
                  {
                    /* enter release mode, which is instantaneous (release_time==0 always);
                       so go straight to OFF */
                    for (which = 0; which < 4; which++)
                      {
                        state.subs[which].adsr_pos = 0;
                        state.subs[which].adsr_mode = FM_PIANO_OSC_OFF;
                      }
                    break;
                  }
                default:
                  g_error ("unknown action %u (%s,%d)",
                           events[event_at].info.action.index, __FILE__, __LINE__);
                }
            }
          else
            g_error ("event of unknown type %u", events[event_at].type);
          event_at++;
        }
      
      while (sample_at < end_sample)
        {
          double temp;
          double wav0_phase_offset;
          double wav2_phase_offset;
          double vibrato;

          wav0_phase_offset = state.subs[1].gain
                            * adsr_tick (&state, 1)
                            * wave_tick (&state, 1, 0.0)
                            * state.modulator_index * 2.0;
          IF_DUMP (g_print("wav0_phase_offset=%.6f\n", wav0_phase_offset));

          wav2_phase_offset = state.subs[3].gain
                            * adsr_tick (&state, 3)
                            * wave_tick (&state, 3, twozero_last_out);
          IF_DUMP (g_print("wav2_phase_offset=%.6f\n", wav2_phase_offset));
          twozero_last_out = bb_filter1_run (state.twozero, wav2_phase_offset * state.twozero_gain);

          temp = (1.0 - state.crossfade)
               * state.subs[0].gain
               * adsr_tick (&state, 0)
               * wave_tick (&state, 0, wav0_phase_offset)
               + (state.crossfade)
               * state.subs[2].gain
               * adsr_tick (&state, 2)
               * wave_tick (&state, 2, wav2_phase_offset);

          vibrato = sin (state.vibrato_pos) * state.vibrato_gain;
          state.vibrato_pos += state.vibrato_step;
          if (state.vibrato_pos >= BB_2PI)
            state.vibrato_pos -= BB_2PI;

          rv[sample_at] = temp * (1.0 + vibrato) * 0.5;
          IF_DUMP (g_print("tick %u: %.6f", sample_at,rv[sample_at]));

          /* update sample index */
          sample_at++;
        }
    }
  *n_samples_out = n;
  return rv;
}


BB_INIT_DEFINE(_bb_fm_piano_init)
{
  BbInstrument *instrument;
  instrument = bb_instrument_new_from_simple_param (G_N_ELEMENTS (fm_piano_simple_params),
                                                    fm_piano_simple_params);
  bb_instrument_add_param (instrument,
                           PARAM_SCALE_FREQUENCY0,
                           g_param_spec_boolean ("scale_frequency0", "Scale Frequency 0", NULL, TRUE, 0));
  bb_instrument_add_param (instrument,
                           PARAM_SCALE_FREQUENCY1,
                           g_param_spec_boolean ("scale_frequency1", "Scale Frequency 1", NULL, TRUE, 0));
  bb_instrument_add_param (instrument,
                           PARAM_SCALE_FREQUENCY2,
                           g_param_spec_boolean ("scale_frequency2", "Scale Frequency 2", NULL, FALSE, 0));
  bb_instrument_add_param (instrument,
                           PARAM_SCALE_FREQUENCY3,
                           g_param_spec_boolean ("scale_frequency3", "Scale Frequency 3", NULL, FALSE, 0));
  bb_instrument_add_action (instrument,
                            ACTION_KEY_ON,
                            "key_on",
                            NULL);
  bb_instrument_add_action (instrument,
                            ACTION_KEY_OFF,
                            "key_off",
                            NULL);

  bb_instrument_add_preset (instrument, "wurley",
			    "ratio0",                    1.0,
			    "gain0",                     get_fm_gain (99),
			    "decay_time0",               1.50,
			    "ratio1",                    4.0,
			    "gain1",                     get_fm_gain (82),
			    "decay_time1",               1.50,
			    "ratio2",                    510.0,
			    "gain2",                     get_fm_gain (92),
			    "decay_time2",               0.25,
			    "ratio3",                    510.0,
			    "gain3",                     get_fm_gain (68),
			    "decay_time3",               0.15,
			    "scale_frequency0",          TRUE,
			    "scale_frequency1",          TRUE,
			    "scale_frequency2",          FALSE,
			    "scale_frequency3",          FALSE,
			    "twozero_gain",              1.0,
                            "vibrato_frequency",         8.0,
                            "vibrato_gain",              0.394,
			    NULL);
  bb_instrument_add_preset (instrument, "rhodey",
                            "ratio0",                    2.0,
			    "gain0",                     get_fm_gain (99),
			    "decay_time0",               1.50,
                            "ratio1",                    1.0,
			    "gain1",                     get_fm_gain (90),
			    "decay_time1",               1.50,
                            "ratio2",                    2.0,
	                    "gain2",                     get_fm_gain (99),
	                    "decay_time2",               1.00,
	                    "ratio3",                    30.0,
	                    "gain3",                     get_fm_gain (67),
	                    "decay_time3",               0.25,
			    "scale_frequency0",          TRUE,
			    "scale_frequency1",          TRUE,
			    "scale_frequency2",          TRUE,
			    "scale_frequency3",          TRUE,
	                    "twozero_gain",              1.0,
                            "vibrato_frequency",         6.0,
	                    NULL);

  instrument->complex_synth_func = fm_piano_complex_synth;

  bb_instrument_set_name (instrument, "fm_piano");
}
