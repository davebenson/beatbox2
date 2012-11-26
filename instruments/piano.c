#include "instrument-includes.h"
#include "../core/bb-filter1.h"
#include "../core/bb-delay-line.h"

/*   A------ P -----------H--------B   */
/* 'H' is where the hammer is.
 * 'P' is where the pickup is.
 */
typedef struct _PianoState PianoState;
struct _PianoState
{
  gboolean impulse_running;
  gdouble *impulse;
  guint impulse_at, impulse_len;

  BbDelayLine *AP_delay_line;		/* delay from P -> A -> P */
  BbDelayLine *PB_delay_line;		/* delay from P -> B -> P */
  gdouble ap_delay, pb_delay;

  gdouble twang;			/* twang==0 passes all through the filter, twang==1 passes none. */
  BbFilter1 *a_filter, *b_filter;
  gdouble damping_per_cycle;
  gdouble real_damping;
  gdouble frequency;

  BbFilter1 *body_filter;
  BbDelayLine *simple_comb;

  gdouble sampling_rate;
};

/* P and H defined as a fraction of the distance from A to B.
   Note that P_POS < H_POS for things to work. */
#define P_POS           0.333333
#define H_POS           0.666667

enum
{
  PARAM_FREQUENCY,
  PARAM_DAMPING,
  PARAM_TWANG,
  PARAM_COMB_VOLUME
};

enum
{
  ACTION_STRIKE
};

static const BbInstrumentSimpleParam piano_params[] =
{
  { PARAM_FREQUENCY,           "frequency",                  440 },
  { PARAM_DAMPING,             "damping",                    0.7 },
  { PARAM_TWANG,               "twang",                      0.5 },
  { PARAM_COMB_VOLUME,         "comb_volume",                0.6 },
};
static gdouble *
compute_raw_impulse (PianoState *state,
                     gdouble     amplitude,
                     guint       impulse_length)
{
  /* use 3 quadratic humps, for now */
  gdouble *rv = g_new (gdouble, impulse_length);
  guint i;
  gdouble scale = 1.0 / impulse_length;
  for (i = 0; i < impulse_length; i++)
    {
      gdouble x = scale * i;
      guint which = floor ((double)x * 3.0);
      gdouble frac = fmod (x, 0.33333333333);
      gdouble coeff = 1.0;
      while (which--)
        coeff *= 0.5;
      frac *= 6;
      frac -= 1;
      rv[i] = coeff * (1.0 - frac * frac) * amplitude;
    }
  return rv;
}

static gdouble *
compute_impulse (PianoState *state,
                 gdouble     amplitude,
                 guint      *n_samples_out)
{
  gdouble wavelength = state->sampling_rate / state->frequency;
  guint raw_impulse_length = wavelength / 8;
  guint impulse_length = wavelength;
  gdouble *raw = compute_raw_impulse (state, amplitude, raw_impulse_length);
  gdouble *impulse = g_new0 (gdouble, impulse_length);
  gdouble d1 = (H_POS - P_POS) * wavelength / 2.0;
  gdouble d2 = ((H_POS - P_POS) + (1.0 - H_POS) * 2.0) * wavelength / 2.0;
  guint d1i = (int)d1;
  guint d2i = (int)d2;
  BbFilter1 *copy = bb_filter1_copy (state->b_filter);
  gdouble *out_at = impulse + d2i;
  guint i;

  /* filter, negate and delay impulse from raw for
     bounce from H -> B -> P */
  for (i = 0; i < raw_impulse_length; i++)
    *out_at++ = -bb_filter1_run (copy, raw[raw_impulse_length - 1 - i]);
  for (i = 0; i < impulse_length - d2i - raw_impulse_length; i++)
    *out_at++ = -bb_filter1_run (copy, 0);

  /* copy and delay impulse from 'raw' (H -> P) */
  out_at = impulse + d1i;
  for (i = 0; i < raw_impulse_length; i++)
    *out_at += raw[i];

  g_free (raw);
  *n_samples_out = impulse_length;
  return impulse;
}

static void
recompute_damping (PianoState *state)
{
  gdouble log_dpsecond = log(state->real_damping);
  gdouble log_dpsample = log_dpsecond / state->frequency;
  state->damping_per_cycle = exp (log_dpsample);
}

static void
set_frequency (PianoState *state,
               gdouble     freq)
{
  gdouble tone_period = state->sampling_rate / freq;
  state->frequency = freq;

  /* compute delay line length */
  /* delay_a + delay_b == tone_period */
  /* delay_a     delay_b
     -------  == -------
      P_POS      1-P_POS  */
  
  /* so we have a system of linear equations:
     delay_a + delay_b == tone_period
     delay_a*(1-P_POS) - delay_b*P_POS == 0 */
  /* or, substituting:
     delay_a*(1-P_POS) - (tone_period-delay_a)*P_POS == 0;
    simplifying:
     delay_a*(1-P_POS+P_POS) - tone_period*P_POS==0
     delay_a = tone_period*P_POS.
    and hence:
     delay_b = tone_period*(1-P_POS). */
  state->ap_delay = tone_period * P_POS;
  state->pb_delay = tone_period * (1.0 - P_POS);

  /* update filters */
  //bb_filter1_3_2_set_resonance (state->a_filter, 1.0,freq,state->sampling_rate,0.9,FALSE);
  //bb_filter1_3_2_set_resonance (state->b_filter, 1.0,freq,state->sampling_rate,0.9,FALSE);
  bb_filter1_3_2_set_butterworth (state->a_filter,BB_BUTTERWORTH_BAND_PASS,state->sampling_rate,
                                  freq, 2, 1.0);
  bb_filter1_3_2_set_butterworth (state->b_filter,BB_BUTTERWORTH_BAND_PASS,state->sampling_rate,
                                  freq, 2, 1.0);

  recompute_damping (state);
}

static double *
piano_complex_synth   (BbInstrument *instrument,
                       BbRenderInfo *render_info,
                       double        duration,
                       GValue       *init_params,
                       guint         n_events,
                       const BbInstrumentEvent *events,
                       guint        *n_samples_out)
{
  double rate = render_info->sampling_rate;
  guint n = render_info->note_duration_samples;

  PianoState state;
  guint max_delay_line = rate / 10.0 + 5;
  guint sample_at = 0, event_at = 0;
  gdouble last_comb_output = 0;
  gdouble comb_volume = g_value_get_double (init_params + PARAM_COMB_VOLUME);
  gdouble *rv = g_new (gdouble, n);

  state.impulse_running = FALSE;
  state.impulse = NULL;
  state.impulse_len = state.impulse_at = 0;
  state.AP_delay_line = bb_delay_line_new (max_delay_line);
  state.PB_delay_line = bb_delay_line_new (max_delay_line);
  state.a_filter = bb_filter1_new_3_2_defaults ();
  state.b_filter = bb_filter1_new_3_2_defaults ();
  state.body_filter = bb_filter1_butterworth (BB_BUTTERWORTH_LOW_PASS, render_info->sampling_rate,
                                              100,      /* frequency */
                                              1,        /* bandwidth */
                                              3);       /* db-gain */
  state.simple_comb = bb_delay_line_new (rate / 100);
  state.sampling_rate = rate;

  state.real_damping = g_value_get_double (init_params + PARAM_DAMPING);
  state.twang = g_value_get_double (init_params + PARAM_TWANG);

  /* this updates the ap_delay, pb_delay and damping_per_sample */
  set_frequency (&state, g_value_get_double (init_params + PARAM_FREQUENCY));

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
                case PARAM_DAMPING:
                  state.real_damping = g_value_get_double (v);
                  recompute_damping (&state);
                  break;
                case PARAM_TWANG:
                  state.twang = g_value_get_double (v);
                  break;
                case PARAM_COMB_VOLUME:
                  comb_volume = g_value_get_double (v);
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
                case ACTION_STRIKE:
                  {
                    gdouble gain = g_value_get_double (args + 0) * (state.frequency / 440.0);
                    gdouble *impulse = compute_impulse (&state, gain, &state.impulse_len);
                    if (state.impulse_running)
                      g_free (state.impulse);
                    state.impulse = impulse;
                    state.impulse_at = 0;
                    state.impulse_running = TRUE;
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
          gdouble toward_a;
          gdouble toward_b = 0;
          gdouble filtered;
          gdouble tmp;
          gdouble no_body_output;

          if (state.impulse_running)
            {
              toward_a = state.impulse[state.impulse_at++];
              if (state.impulse_at >= state.impulse_len)
                {
                  state.impulse_running = FALSE;
                  g_free (state.impulse);
                  state.impulse = NULL;
                }
            }
          else
            toward_a = 0;
          tmp = -bb_delay_line_get_lin (state.PB_delay_line, state.pb_delay);
          filtered = bb_filter1_run (state.b_filter, tmp);
          toward_a += (state.twang * tmp)
                    + ((1.0-state.twang) * filtered);

          tmp = -bb_delay_line_get_lin (state.AP_delay_line, state.ap_delay);
          filtered = bb_filter1_run (state.a_filter, tmp);
          toward_b = (state.twang * tmp)
                   + ((1.0-state.twang) * filtered);

          //g_message ("toward_a=%.6f, toward_b=%.6f", toward_a,toward_b);

          bb_delay_line_add (state.AP_delay_line, toward_a * state.damping_per_cycle);
          bb_delay_line_add (state.PB_delay_line, toward_b * state.damping_per_cycle);

          no_body_output = bb_filter1_run (state.body_filter,
                                           toward_a + toward_b);
          last_comb_output = bb_delay_line_add (state.simple_comb,
                                                comb_volume * (no_body_output + last_comb_output));
          rv[sample_at] = no_body_output + last_comb_output;

          /* update sample index */
          sample_at++;
        }
    }
  *n_samples_out = n;
  bb_filter1_free (state.a_filter);
  bb_filter1_free (state.b_filter);
  bb_filter1_free (state.body_filter);
  bb_delay_line_free (state.AP_delay_line);
  bb_delay_line_free (state.PB_delay_line);
  bb_delay_line_free (state.simple_comb);
  if (state.impulse_running)
    g_free (state.impulse);
  return rv;
}

BB_INIT_DEFINE(_bb_piano_init)
{
  BbInstrument *instrument = bb_instrument_new_from_simple_param (G_N_ELEMENTS (piano_params),
                                                                  piano_params);
  bb_instrument_add_action (instrument,
                            ACTION_STRIKE, "strike",
                            g_param_spec_double ("amplitude", "Amplitude", NULL, 0, 2, 1, 0),
                            NULL);
  bb_instrument_set_name (instrument, "piano");
  instrument->complex_synth_func = piano_complex_synth;
}
