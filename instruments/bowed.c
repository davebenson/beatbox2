#include "instrument-includes.h"
#include "../core/bb-delay-line.h"
#include "../core/bb-filter1.h"
#include "../core/bb-bow-table.h"

typedef struct _BowedState BowedState;

typedef enum
{
  BOW_OFF,
  BOW_ATTACK,
  BOW_DECAY,
  BOW_SUSTAIN,
  BOW_RELEASE
} BowAdsrMode;


struct _BowedState
{
  BbDelayLine *neck_delay_line;
  gdouble      neck_delay;
  BbDelayLine *bridge_delay_line;
  gdouble      bridge_delay;
  gdouble      last_bridge_delay_out;
  BbBowTable   bow_table;
  BbFilter1   *string_filter;
  BbFilter1   *body_filter;
  double       vibrato_pos;
  double       vibrato_step;

  gdouble      max_velocity;
  gdouble      base_delay;
  gdouble      vibrato_gain;
  gdouble      beta_ratio;

  BowAdsrMode  adsr_mode;
  gdouble      adsr_value;

  gdouble      adsr_attack_rate, adsr_decay_rate, adsr_release_rate;
  gdouble      adsr_sustain_level;

  gdouble      sampling_rate;
};

enum
{
  PARAM_FREQUENCY,
  PARAM_BOW_PRESSURE,
  PARAM_BOW_POSITION,
  PARAM_VIBRATO_FREQUENCY,
  PARAM_VIBRATO_GAIN
};


static const BbInstrumentSimpleParam bowed_params[] =
{
  { PARAM_FREQUENCY,             "frequency",                  440 },
  { PARAM_BOW_PRESSURE,          "bow_pressure",               1.0 },
  { PARAM_BOW_POSITION,          "bow_position",               1.0 },
  { PARAM_VIBRATO_FREQUENCY,     "vibrato_frequency",          6.12723 },
  { PARAM_VIBRATO_GAIN,          "vibrato_gain",               0 }
};

enum
{
  ACTION_START_BOWING,
  ACTION_STOP_BOWING
};

static inline void
set_frequency (BowedState *state,
               gdouble     freq)
{
  double base_delay = state->sampling_rate / freq - 4.0;
  if (base_delay < 0.3)
    base_delay = 0.3;
  state->bridge_delay = base_delay * state->beta_ratio;
  state->neck_delay = base_delay * (1.0 - state->beta_ratio);
}

static inline void
set_vibrato_frequency (BowedState *state,
                       gdouble     freq)
{
  state->vibrato_step = fmod (freq * BB_2PI / state->sampling_rate, BB_2PI);
}

#define LOWEST_FREQ     5.0

void
bowed_state_init (BowedState *state,
                  double      sampling_rate)
{
  guint len = (guint) ceil (sampling_rate / LOWEST_FREQ + 1);
  state->sampling_rate = sampling_rate;
  state->neck_delay_line = bb_delay_line_new (len);
  state->neck_delay = 100.0;
  state->bridge_delay_line = bb_delay_line_new (len / 2);
  state->last_bridge_delay_out = 0;
  state->neck_delay = 29.0;
  bb_bow_table_init (&state->bow_table);
  bb_bow_table_set_slope (&state->bow_table, 3.0);
  state->vibrato_pos = 0;
  state->vibrato_step = BB_2PI * 6.12723 / sampling_rate;
  state->vibrato_gain = 0;
  state->string_filter = bb_filter1_one_pole (0.6 - (0.1 * 22050.0 / sampling_rate), 0.95);
  state->body_filter = bb_filter1_new_biquad (0.2, 500.0, sampling_rate, 0.85);
  state->beta_ratio = 0.127236;

  state->adsr_mode = BOW_OFF;
  state->adsr_value = 0;

  state->adsr_attack_rate = 1.0 / (0.02 * sampling_rate);
  state->adsr_decay_rate = (1.0-0.9) / (sampling_rate * 0.005);
  state->adsr_release_rate = 0.01 / sampling_rate;
  state->adsr_sustain_level = 0.9;

  set_frequency (state, 220.0);
}


static double *
bowed_complex_synth       (BbInstrument *instrument,
			   BbRenderInfo *render_info,
                           double        duration,
			   GValue       *init_params,
			   guint         n_events,
			   const BbInstrumentEvent *events,
			   guint        *n_samples_out)
{
  double rate = render_info->sampling_rate;
  guint n = render_info->note_duration_samples;
  double *rv = g_new (double, n);
  guint sample_at = 0;
  guint event_at = 0;
  gdouble last_bridge_delay = 0;
  BowedState state;
  bowed_state_init (&state, render_info->sampling_rate);
  set_frequency (&state, g_value_get_double (init_params + PARAM_FREQUENCY));
  state.vibrato_gain = g_value_get_double (init_params + PARAM_VIBRATO_GAIN);
  set_vibrato_frequency (&state, g_value_get_double (init_params + PARAM_VIBRATO_FREQUENCY));
  while (sample_at < n)
    {
      guint end_sample = n;
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
              switch (events[event_at].info.param.index)
                {
                case PARAM_FREQUENCY:
                  set_frequency (&state, g_value_get_double (v));
                  break;
                case PARAM_VIBRATO_FREQUENCY:
                  set_vibrato_frequency (&state, g_value_get_double (v));
                  break;
                case PARAM_VIBRATO_GAIN:
                  state.vibrato_gain = g_value_get_double (v);
                  break;
                default:
                  g_assert_not_reached ();
                }
              event_at++;
            }
          else if (events[event_at].type == BB_INSTRUMENT_EVENT_ACTION)
            {
              switch (events[event_at].info.action.index)
                {
                case ACTION_START_BOWING:
                  state.max_velocity = g_value_get_double (events[event_at].info.action.args + 0) * 0.2 + 0.03;
                  state.adsr_mode = BOW_ATTACK;
                  break;

                case ACTION_STOP_BOWING:
                  state.adsr_mode = BOW_RELEASE;
                  break;
                }
              event_at++;
            }
          else
            g_error ("event of unknown type %u", events[event_at].type);
        }
      
      while (sample_at < end_sample)
        {
          gdouble bow_velocity = state.adsr_value * state.max_velocity;
          gdouble bridge_reflection;
          gdouble nut_reflection;
          gdouble string_velocity;
          gdouble velocity_diff;
          gdouble new_velocity;


          /* Compute bow velocity */
          switch (state.adsr_mode)
            {
            case BOW_OFF:
              break;
            case BOW_ATTACK:
              state.adsr_value += state.adsr_attack_rate;
              if (state.adsr_value > 1.0)
                {
                  state.adsr_value = 1.0;
                  state.adsr_mode = BOW_DECAY;
                }
              break;
            case BOW_DECAY:
              state.adsr_value -= state.adsr_decay_rate;
              if (state.adsr_value < state.adsr_sustain_level)
                {
                  state.adsr_value = state.adsr_sustain_level;
                  state.adsr_mode = BOW_SUSTAIN;
                }
              break;
            case BOW_SUSTAIN:
              break;
            case BOW_RELEASE:
              state.adsr_value -= state.adsr_release_rate;
              if (state.adsr_value < 0)
                {
                  state.adsr_value = 0;
                  state.adsr_mode = BOW_OFF;
                }
              break;
            }

          bridge_reflection = -bb_filter1_run (state.string_filter, last_bridge_delay);
          nut_reflection = -bb_delay_line_get_lin (state.neck_delay_line, state.neck_delay);
          string_velocity = bridge_reflection + nut_reflection;
          velocity_diff = bow_velocity - string_velocity;
          new_velocity = velocity_diff * bb_bow_table_tick (&state.bow_table, velocity_diff);
          bb_delay_line_add (state.neck_delay_line, bridge_reflection + new_velocity);
          bb_delay_line_add (state.bridge_delay_line, nut_reflection + new_velocity);

          if (state.vibrato_gain != 0.0)
            {
              gdouble vib = cos (state.vibrato_pos);
              state.vibrato_pos += state.vibrato_step;
              if (state.vibrato_pos > BB_2PI)
                state.vibrato_pos -= BB_2PI;
              state.neck_delay = state.base_delay * (1.0 - state.beta_ratio)
                               + state.base_delay * state.vibrato_gain * vib;
            }

          /* update sample index */
          last_bridge_delay = bb_delay_line_get_lin (state.bridge_delay_line, state.bridge_delay);
          rv[sample_at++] = last_bridge_delay;
        }
    }
  *n_samples_out = n;
  return rv;
}

BB_INIT_DEFINE(_bb_bowed_init)
{
  BbInstrument *instrument = bb_instrument_new_from_simple_param (G_N_ELEMENTS (bowed_params),
                                                                  bowed_params);
  bb_instrument_add_action (instrument,
                            ACTION_START_BOWING, "start_bowing",
                            g_param_spec_double ("amplitude", "Amplitude", NULL, 0, 2, 1, 0),
                            NULL);
  bb_instrument_add_action (instrument,
                            ACTION_STOP_BOWING, "stop_bowing",
                            NULL);
  bb_instrument_set_name (instrument, "bowed");
  instrument->complex_synth_func = bowed_complex_synth;
}
