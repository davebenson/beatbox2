#include "instrument-includes.h"
#include "../core/bb-filter1.h"
#include "../core/bb-delay-line.h"
#include "../core/macros.h"

typedef enum
{
  BRASS_ADSR_OFF,
  BRASS_ADSR_ATTACK,
  BRASS_ADSR_SUSTAIN,
  BRASS_ADSR_RELEASE
} BrassAdsrState;

typedef struct _BrassState BrassState;
struct _BrassState
{
  gdouble sampling_rate;

  BbDelayLine *delay_line;

  BrassAdsrState adsr_state;
  gdouble adsr_value, adsr_target, adsr_step;
  BbFilter1 *dc_block, *lip_filter;

  gdouble base_frequency, base_lip_tension;

  gdouble vibrato_pos, vibrato_step, vibrato_gain;
  gdouble max_pressure;
  gdouble feedback_delay;
};

enum
{
  PARAM_FREQUENCY,
  PARAM_VIBRATO_FREQUENCY,
  PARAM_VIBRATO_GAIN,
  PARAM_LIP_TENSION             /* -1 .. 1 */
};

static const BbInstrumentSimpleParam brass_params[] =
{
  { PARAM_FREQUENCY,           "frequency",                  440 },
  { PARAM_VIBRATO_FREQUENCY,   "vibrato_frequency",          6.137 },
  { PARAM_VIBRATO_GAIN,        "vibrato_gain",               0 },
  { PARAM_LIP_TENSION,         "lib_tension",                0 },
};

enum
{
  ACTION_START_BLOWING,
  ACTION_STOP_BLOWING
};

static void
set_vibrato_step (BrassState *state,
                  gdouble     freq)
{
  state->vibrato_step = freq * BB_2PI / state->sampling_rate;
}

static inline void
update_filters_and_delay (BrassState *state)
{
  /* update lipfilter */
  bb_filter1_3_2_set_resonance (state->lip_filter,
                                0.03,
                                pow (4.0, state->base_lip_tension) * state->base_frequency,
                                state->sampling_rate,
                                0.997,
                                FALSE);

  /* set delay (+3 for filter correction... todo: check) */
  state->feedback_delay = 2.0 * state->sampling_rate / state->base_frequency + 3.0;
}

static double *
brass_complex_synth   (BbInstrument *instrument,
                       BbRenderInfo *render_info,
                       double        duration,
                       GValue       *init_params,
                       guint         n_events,
                       const BbInstrumentEvent *events,
                       guint        *n_samples_out)
{
  double rate = render_info->sampling_rate;
  guint n = render_info->note_duration_samples;
  BrassState state;
  guint max_delay_line = rate / 10.0 + 5;
  gdouble *rv = g_new (gdouble, n);
  gdouble last_delay_out = 0;
  guint sample_at = 0, event_at = 0;

  state.sampling_rate = rate;
  state.base_frequency = g_value_get_double (init_params + PARAM_FREQUENCY);
  state.base_lip_tension = g_value_get_double (init_params + PARAM_LIP_TENSION);
  state.vibrato_pos = 0;
  set_vibrato_step (&state, g_value_get_double (init_params + PARAM_VIBRATO_FREQUENCY));
  state.vibrato_gain = g_value_get_double (init_params + PARAM_VIBRATO_GAIN);
  state.max_pressure = 0.0;
  state.delay_line = bb_delay_line_new (max_delay_line);
  state.adsr_state = BRASS_ADSR_OFF;
  state.adsr_value = 0;
  state.adsr_target = 0;
  state.adsr_step = 0;
  state.dc_block = bb_filter1_new_dc_block ();
  state.lip_filter = bb_filter1_new_3_2_defaults ();

  update_filters_and_delay (&state);

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
                  state.base_frequency = g_value_get_double (v);
                  update_filters_and_delay (&state);
                  break;
                case PARAM_VIBRATO_FREQUENCY:
                  state.vibrato_step = BB_2PI * g_value_get_double (v) / rate;
                  break;
                case PARAM_VIBRATO_GAIN:
                  state.vibrato_gain = g_value_get_double (v);
                  break;
                case PARAM_LIP_TENSION:
                  state.base_lip_tension = g_value_get_double (v);
                  update_filters_and_delay (&state);
                  break;
                default:
                  g_error ("%s, %u: no handling for event for param %s",
                           __FILE__, __LINE__,
                           instrument->param_specs[events[event_at].info.param.index]->name);
                }
            }
          else if (events[event_at].type == BB_INSTRUMENT_EVENT_ACTION)
            {
              GValue *args = events[event_at].info.action.args;
              switch (events[event_at].info.action.index)
                {
                case ACTION_START_BLOWING:
                  {
                    gdouble amplitude = g_value_get_double (args + 0);
                    gdouble blow_rate = g_value_get_double (args + 1);
                    state.adsr_step = ABS (blow_rate / rate);
                    state.adsr_target = amplitude;
                    if (state.adsr_target < state.adsr_value)
                      {
                        state.adsr_state = BRASS_ADSR_SUSTAIN;
                        state.adsr_step = 0;
                        state.adsr_value = state.adsr_target;
                      }
                    else
                      {
                        g_message ("attack");
                        state.adsr_state = BRASS_ADSR_ATTACK;
                      }
                    break;
                  }
                case ACTION_STOP_BLOWING:
                  {
                    gdouble blow_rate = g_value_get_double (args + 0);
                    state.adsr_step = - ABS (blow_rate / rate);
                    state.adsr_target = 0;
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
          gdouble breath_pressure = state.adsr_value;
          gdouble mouth_pressure, bore_pressure, delta_pressure;
          gdouble delay_input;

          /* update adsr */
          if (state.adsr_state == BRASS_ADSR_ATTACK)
            {
              state.adsr_value += state.adsr_step;
              if (state.adsr_value > state.adsr_target)
                {
                  state.adsr_state = BRASS_ADSR_SUSTAIN;
                  state.adsr_value = state.adsr_target;
                }
            }
          else if (state.adsr_state == BRASS_ADSR_RELEASE)
            {
              state.adsr_value += state.adsr_step;
              if (state.adsr_value < 0)
                {
                  state.adsr_value = 0.0;
                  state.adsr_state = BRASS_ADSR_OFF;
                }
            }


          breath_pressure += sin (state.vibrato_pos) * state.vibrato_gain;
          state.vibrato_pos += state.vibrato_step;
          if (state.vibrato_pos >= BB_2PI)
            state.vibrato_pos -= BB_2PI;

          mouth_pressure = 0.3 * breath_pressure;
          bore_pressure = 0.85 * last_delay_out;
          delta_pressure = mouth_pressure - bore_pressure;
          delta_pressure = bb_filter1_run (state.lip_filter, delta_pressure);
          delta_pressure *= delta_pressure;
          if (delta_pressure > 1.0)
            delta_pressure = 1.0;

          //g_message ("adsr: %.6f, breath_pressure=%.6f, mouth_pressure=%.6f, bore_pressure=%.6f; delta_pressure=%.6f",
                     //state.adsr_value,breath_pressure,mouth_pressure,bore_pressure,delta_pressure);

          delay_input = delta_pressure * mouth_pressure
                      + (1.0 - delta_pressure) * bore_pressure;
          //delay_input = bb_filter1_run (state.dc_block, delay_input);
          {gdouble tmp=bb_filter1_run (state.dc_block, delay_input);
          //g_message("delay_input=%.12f, dc_block_output=%.12f", delay_input, tmp);
          delay_input=tmp;}
          bb_delay_line_add (state.delay_line, delay_input);
          last_delay_out = bb_delay_line_get_lin (state.delay_line,
                                                  state.feedback_delay);
          rv[sample_at++] = last_delay_out;
        }
    }
  *n_samples_out = n;
  return rv;
}

BB_INIT_DEFINE(_bb_brass_init)
{
  BbInstrument *instrument = bb_instrument_new_from_simple_param (G_N_ELEMENTS (brass_params),
                                                                  brass_params);
  bb_instrument_add_action (instrument,
                            ACTION_START_BLOWING, "start_blowing",
                            g_param_spec_double ("amplitude", "Amplitude", NULL, 0, 2, 1, 0),
                            g_param_spec_double ("rate", "Rate", NULL, 0, 20, 1, 0),
                            NULL);
  bb_instrument_add_action (instrument,
                            ACTION_STOP_BLOWING, "stop_blowing",
                            g_param_spec_double ("rate", "Rate", NULL, 0, 20, 1, 0),
                            NULL);
  bb_instrument_set_name (instrument, "brass");
  instrument->complex_synth_func = brass_complex_synth;
}
