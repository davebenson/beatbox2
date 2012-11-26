/* parameters:
  0 frequency
  1 frequency_change_rate
  2 volume
  3 volume_change_rate
  4 freqmod_frequency
  5 freqmod_depth
  6 volmod_frequency
  7 volmod_depth
  8 volmod_abs_depth
 */
#include "instrument-includes.h"

typedef enum
{
  PARAM_FREQUENCY,
  PARAM_FREQUENCY_CHANGE_RATE,
  PARAM_VOLUME,
  PARAM_VOLUME_CHANGE_RATE,
  PARAM_FREQMOD_FREQUENCY,
  PARAM_FREQMOD_DEPTH,		/* as a fraction of frequency */
  PARAM_VOLMOD_FREQUENCY,
  PARAM_VOLMOD_DEPTH,		/* as a fraction of volume */
  PARAM_VOLMOD_ABS_DEPTH,	/* in absolute volume */
  PARAM_FREQUENCY_CHANGE_MUTE
} OscillatorParam;

static const BbInstrumentSimpleParam slide_whistle_params[] =
{
  { PARAM_FREQUENCY,             "frequency",                  440 },
  { PARAM_FREQUENCY_CHANGE_RATE, "frequency_change_rate",      200 },		/* hertz/second */
  { PARAM_VOLUME,                "volume",                     1   },
  { PARAM_VOLUME_CHANGE_RATE,    "volume_change_rate",         20  },
  { PARAM_FREQMOD_FREQUENCY,     "freqmod_frequency",          10  },
  { PARAM_FREQMOD_DEPTH,         "freqmod_depth",              0   },
  { PARAM_VOLMOD_FREQUENCY,      "volmod_frequency",           10  },
  { PARAM_VOLMOD_DEPTH,          "volmod_depth",               0   },
  { PARAM_VOLMOD_ABS_DEPTH,      "volmod_abs_depth",           0   },
  { PARAM_FREQUENCY_CHANGE_MUTE, "frequency_change_mute",      0.4 },
};


static double *
slide_whistle_complex_synth  (BbInstrument *instrument,
			   BbRenderInfo *render_info,
                           double        duration,
			   GValue       *init_params,
			   guint         n_events,
			   const BbInstrumentEvent *events,
			   guint        *n_samples_out)
{
  double rate = render_info->sampling_rate;
  guint n = render_info->note_duration_samples;

  /* TODO: waveforms for the three slide_whistles */

  /* frequency, and its dampening */
  double frequency = g_value_get_double (init_params + PARAM_FREQUENCY);
  double real_frequency = frequency;
  gboolean at_frequency_goal = TRUE;
  double frequency_change_rate = g_value_get_double (init_params + PARAM_FREQUENCY_CHANGE_RATE) / rate;
  double freq_radians = 0;

  /* volume, and its dampening */
  double volume = g_value_get_double (init_params + PARAM_VOLUME);
  double real_volume = volume;
  gboolean at_volume_goal = TRUE;
  double volume_change_rate = g_value_get_double (init_params + PARAM_VOLUME_CHANGE_RATE) / rate;
  double frequency_change_mute = g_value_get_double (init_params + PARAM_FREQUENCY_CHANGE_MUTE);

  /* frequency modulation (un-dampened) */
  double freqmod_radians = 0;
  double freqmod_delta_radians = BB_2PI * g_value_get_double (init_params + PARAM_FREQMOD_FREQUENCY) / rate;
  double freqmod_depth = g_value_get_double (init_params + PARAM_FREQMOD_DEPTH);

  /* volume modulation (un-dampened) */
  double volmod_radians = 0;
  double volmod_delta_radians = BB_2PI * g_value_get_double (init_params + PARAM_VOLMOD_DEPTH) / rate;
  double volmod_depth = g_value_get_double (init_params + PARAM_VOLMOD_DEPTH);
  double volmod_abs_depth = g_value_get_double (init_params + PARAM_VOLMOD_ABS_DEPTH);

  double *rv = g_new (double, n);
  guint sample_at = 0;
  guint event_at = 0;
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
                  at_frequency_goal = FALSE;
                  at_volume_goal = FALSE;	/* to deal with frequency-change-mute */
                  break;
                case PARAM_FREQUENCY_CHANGE_RATE:
                  frequency_change_rate = g_value_get_double (v) / rate;
                  break;
                case PARAM_VOLUME:
                  volume = g_value_get_double (v);
                  at_volume_goal = FALSE;
                  break;
                case PARAM_VOLUME_CHANGE_RATE:
                  volume_change_rate = g_value_get_double (v) / rate;
                  break;
                case PARAM_FREQMOD_FREQUENCY:
                  freqmod_delta_radians = BB_2PI * g_value_get_double (v) / rate;
                  break;
                case PARAM_FREQMOD_DEPTH:
		  freqmod_depth = g_value_get_double (v);
                  break;
                case PARAM_VOLMOD_FREQUENCY:
                  volmod_delta_radians = BB_2PI * g_value_get_double (v) / rate;
                  break;
                case PARAM_VOLMOD_DEPTH:
		  volmod_depth = g_value_get_double (v);
                  break;
                case PARAM_VOLMOD_ABS_DEPTH:
		  volmod_abs_depth = g_value_get_double (v);
                  break;
                case PARAM_FREQUENCY_CHANGE_MUTE:
                  frequency_change_mute = g_value_get_double (v);
                  break;
                default:
                  g_error ("%s, %u: no handling for event for param %s",
                           __FILE__, __LINE__,
                           instrument->param_specs[events[event_at].info.param.index]->name);
                }
              event_at++;
            }
          else
            g_error ("event of unknown type %u", events[event_at].type);
        }
      
      while (sample_at < end_sample)
        {
          /* process: no events anticipated */
          double volmod = sin (volmod_radians);
          double freqmod = sin (freqmod_radians);
          double modded_volume = real_volume * (1.0 + volmod * volmod_depth) + volmod_abs_depth * volmod;
          double modded_freq = real_frequency * (1.0 + freqmod * freqmod_depth);
          rv[sample_at] = modded_volume * sin (freq_radians);
          freq_radians += modded_freq * BB_2PI / rate;

          /* dampened approach to desired vol/freq */
          if (!at_frequency_goal)
            {
              if (real_frequency + frequency_change_rate < frequency)
                real_frequency += frequency_change_rate;
              else if (real_frequency - frequency_change_rate > frequency)
                real_frequency -= frequency_change_rate;
              else
                {
                  real_frequency = frequency;
                  at_frequency_goal = TRUE;
                  at_volume_goal = FALSE;	/* to deal with frequency-change-mute */
                }
            }
          if (!at_volume_goal)
            {
              double target_volume = volume;
              if (!at_frequency_goal)
                target_volume *= frequency_change_mute;
              if (real_volume + volume_change_rate < target_volume)
                real_volume += volume_change_rate;
              else if (real_volume - volume_change_rate > target_volume)
                real_volume -= volume_change_rate;
              else
                {
                  real_volume = target_volume;
                  at_volume_goal = TRUE;
                }
            }

          /* update slide_whistles */
          if (freq_radians >= G_PI)
            freq_radians -= BB_2PI;
          volmod_radians += volmod_delta_radians;
          if (volmod_radians >= G_PI)
            volmod_radians -= BB_2PI;
          freqmod_radians += freqmod_delta_radians;
          if (freqmod_radians >= G_PI)
            freqmod_radians -= BB_2PI;

          /* update sample index */
          sample_at++;
        }
    }
  *n_samples_out = n;
  return rv;
}


BB_INIT_DEFINE(_bb_slide_whistle_init)
{
  BbInstrument *instrument = bb_instrument_new_from_simple_param (G_N_ELEMENTS (slide_whistle_params),
                                                                  slide_whistle_params);
  bb_instrument_set_name (instrument, "slide_whistle");
  instrument->complex_synth_func = slide_whistle_complex_synth;
}
