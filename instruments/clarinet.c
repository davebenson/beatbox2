#include "instrument-includes.h"
#include "../core/bb-reed-table.h"
#include "../core/bb-rate-limiter.h"
#include "../core/bb-delay-line.h"
#include "../core/bb-filter1.h"

#define DEFAULT_VIBRATO_FREQ	5.735
#define DEFAULT_NOISE_GAIN	0.2

enum
{
  PARAM_FREQUENCY,
  PARAM_REED_STIFFNESS,
  PARAM_NOISE_GAIN,
  PARAM_VIBRATO_FREQUENCY,
  PARAM_VIBRATO_GAIN,
  PARAM_VOLUME
};

static const BbInstrumentSimpleParam clarinet_params[] =
{
  { PARAM_FREQUENCY,             "frequency",                  440 },
  { PARAM_REED_STIFFNESS,        "reed_stiffness",             -0.3 },
  { PARAM_NOISE_GAIN,            "noise_gain",                 0.2 },
  { PARAM_VIBRATO_FREQUENCY,     "vibrato_frequency",              5.735 },
  { PARAM_VIBRATO_GAIN,          "vibrato_gain",                   0.1 },
  { PARAM_VOLUME,                "volume",                     0.0 },
};

enum
{
  ACTION_NOTE_ON,
  ACTION_NOTE_OFF,
  ACTION_START_BLOWING,
  ACTION_STOP_BLOWING,
};

static inline double
get_delay (double sampling_rate, double freq)
{
  double delay;
  if (freq < 10.0)
    freq = 10.0;
  delay = (sampling_rate / freq) * 0.5 - 1.5;		/* XXX: however, we need to see if our delay line is ok..
                                                           eg maybe it introduces another sample of latency?
                                                           probably should test this! */
  return MAX (delay, 0.3);
}


static double *
clarinet_complex_synth  (BbInstrument *instrument,
			   BbRenderInfo *render_info,
                           double        duration,
			   GValue       *init_params,
			   guint         n_events,
			   const BbInstrumentEvent *events,
			   guint        *n_samples_out)
{
  double rate = render_info->sampling_rate;
  guint n = render_info->note_duration_samples;

  /* frequency, and its dampening */
  double frequency = g_value_get_double (init_params + PARAM_FREQUENCY);

  double vibrato = 0;
  double vibrato_step = g_value_get_double (init_params + PARAM_VIBRATO_FREQUENCY) * 2.0 * M_PI / rate;
  double vibrato_gain = g_value_get_double (init_params + PARAM_VIBRATO_GAIN);
  double last_delay_line_output = 0;
  BbDelayLine *delay_line = bb_delay_line_new ((int)(rate / 5.0));
  double delay = get_delay (rate, frequency);
  double *rv = g_new (double, n);
  guint sample_at = 0;
  guint event_at = 0;
  BbRateLimiter *rate_limiter = bb_rate_limiter_new ();
  BbReedTable *reed_table = bb_reed_table_new ();
  double noise_gain = g_value_get_double (init_params + PARAM_NOISE_GAIN);
  double output_gain = 1.0;

  /* XXX: probably the one-zero filter is faster */
  BbFilter1 *filter = bb_filter1_new_square (2);

  bb_rate_limiter_set_target (rate_limiter, g_value_get_double (init_params + PARAM_VOLUME));

  while (vibrato_step >= 2.0 * M_PI)
    vibrato_step -= 2.0 * M_PI;

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
                  frequency = g_value_get_double (v);
                  delay = get_delay (rate, frequency);
                  break;
                case PARAM_REED_STIFFNESS:
                  bb_reed_table_set_slope (reed_table, g_value_get_double (v));
                  break;
                case PARAM_NOISE_GAIN:
                  noise_gain = g_value_get_double (v);
                  break;
                case PARAM_VIBRATO_FREQUENCY:
                  vibrato_step = fmod (g_value_get_double (v) * 2.0 * M_PI / rate, 2.0 * M_PI);
                  break;
                case PARAM_VIBRATO_GAIN:
                  vibrato_gain = g_value_get_double (v);
                  break;
                case PARAM_VOLUME:
                  bb_rate_limiter_set_value (rate_limiter, g_value_get_double (v));
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
                case ACTION_NOTE_ON:
                  {
                    gdouble frequency = g_value_get_double (args + 0);
                    gdouble amplitude = g_value_get_double (args + 1);
                    delay = get_delay (rate, frequency);
                    bb_rate_limiter_set_target (rate_limiter, 0.55 * amplitude * 0.30);
                    bb_rate_limiter_set_rate (rate_limiter, amplitude / rate);
                    output_gain = amplitude + 0.001;
                    break;
                  }
                case ACTION_NOTE_OFF:
                  {
                    gdouble amplitude = g_value_get_double (args + 0);
                    bb_rate_limiter_set_target (rate_limiter, 0.0);
                    bb_rate_limiter_set_rate (rate_limiter, amplitude * 100 / rate);
                    break;
                  }
                case ACTION_START_BLOWING:
                  {
		    gdouble amplitude = g_value_get_double (args + 0);
		    gdouble change_rate = g_value_get_double (args + 1) / rate;
                    bb_rate_limiter_set_target (rate_limiter, amplitude);
                    bb_rate_limiter_set_rate (rate_limiter, change_rate);
		    break;
                  }
                case ACTION_STOP_BLOWING:
                  {
		    gdouble change_rate = g_value_get_double (args + 0) / rate;
                    bb_rate_limiter_set_target (rate_limiter, 0);
                    bb_rate_limiter_set_rate (rate_limiter, change_rate);
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
          double breath_pressure;
          double pressure_diff;
          double delay_input;

          breath_pressure = bb_rate_limiter_tick (rate_limiter);
          breath_pressure += breath_pressure * noise_gain * g_random_double_range (-1, 1);
          breath_pressure += breath_pressure * sin (vibrato) * vibrato_gain;
          vibrato += vibrato_step;
          if (vibrato > M_PI)
            vibrato -= 2.0 * M_PI;
          //bb_rate_limiter_dump(rate_limiter);
          pressure_diff = -0.95 * bb_filter1_run (filter, last_delay_line_output)
                        - breath_pressure;

          delay_input = breath_pressure
                      + pressure_diff * bb_reed_table_tick (reed_table, pressure_diff);
          bb_delay_line_add (delay_line, delay_input);
          last_delay_line_output = bb_delay_line_get_lin (delay_line, delay);
          rv[sample_at] = output_gain * last_delay_line_output;

          /* update sample index */
          sample_at++;
        }
    }
  *n_samples_out = n;
  bb_filter1_free (filter);
  bb_delay_line_free (delay_line);
  bb_rate_limiter_free (rate_limiter);
  bb_reed_table_free (reed_table);
  return rv;
}


BB_INIT_DEFINE(_bb_clarinet_init)
{
  BbInstrument *instrument = bb_instrument_new_from_simple_param (G_N_ELEMENTS (clarinet_params),
                                                                  clarinet_params);
  bb_instrument_add_action (instrument,
                            ACTION_NOTE_ON, "note_on",
                            g_param_spec_double ("frequency", "Frequency", NULL, 10, 20000, 440, 0),
                            g_param_spec_double ("amplitude", "Amplitude", NULL, 0, 2, 1, 0),
                            NULL);
  bb_instrument_add_action (instrument,
                            ACTION_NOTE_OFF, "note_off",
                            g_param_spec_double ("amplitude", "Amplitude", NULL, 0, 2, 1, 0),
                            NULL);
  bb_instrument_add_action (instrument,
                            ACTION_NOTE_OFF, "start_blowing",
                            g_param_spec_double ("amplitude", "Amplitude", NULL, 0, 2, 1, 0),
                            g_param_spec_double ("rate", "Rate", NULL, 0, 2, 1, 0),
                            NULL);
  bb_instrument_add_action (instrument,
                            ACTION_NOTE_OFF, "stop_blowing",
                            g_param_spec_double ("rate", "Rate", NULL, 0, 2, 1, 0),
                            NULL);
  bb_instrument_set_name (instrument, "clarinet");
  instrument->complex_synth_func = clarinet_complex_synth;
}
