/* repeat a waveform at regular intervals.
 * features:
 *   - frequency waveform
 *   - amplitude waveform
 *   - subinstrument to splatter all over
 * the wave_train from whence this comes (from CLM in "snd", vesion 7.18)
 * has an "FM" parameter.  We do not need this b/c frequency is a waveform.
 */

#include "instrument-includes.h"
#include "../core/bb-waveform.h"
#include <gsk/gskghelpers.h>

typedef struct _WaveTrainInfo WaveTrainInfo;
struct _WaveTrainInfo
{
  BbInstrument *subinstrument;
  guint *subinstrument_params;
  BbInstrument *amplitude_ins;
  guint *amplitude_params;
  BbInstrument *frequency_ins;
  guint *frequency_params;
  guint max_params;             /* max of n_param_specs over the three instruments */
};

enum
{
  PARAM_DURATION,
};

static gdouble *
wave_train_synth  (BbInstrument *instrument,
                   BbRenderInfo *render_info,
                   GValue       *params,
                   guint        *n_samples_out)
{
  WaveTrainInfo *info = instrument->data;
  BbInstrument *subinstr = info->subinstrument;
  GValue *subinstrument_values = g_newa (GValue, info->max_params);
  gdouble *rv;
  guint n_sub_rv;
  gdouble *sub_rv;
  gdouble *amplitude;
  guint n_amplitude;
  gdouble *frequency;
  guint n_frequency;
  guint n = render_info->note_duration_samples;
  guint i;
  gdouble cur_fraction;
  gdouble one_over_sample_rate = 1.0 / render_info->sampling_rate;

  if (n == 0)
    {
      *n_samples_out = 0;
      return NULL;
    }

  /* invoke subinstrument (we only invoke it once).
   * first, remap parameters for subinstrument */
  for (i = 0; i < subinstr->n_param_specs; i++)
    subinstrument_values[i] = params[info->subinstrument_params[i]];
  sub_rv = bb_instrument_synth (subinstr, render_info, subinstrument_values, &n_sub_rv);

  /* evaluate amplitude and frequency waveforms */
  for (i = 0; i < info->amplitude_ins->n_param_specs; i++)
    subinstrument_values[i] = params[info->amplitude_params[i]];
  amplitude = bb_instrument_synth (info->amplitude_ins, render_info, subinstrument_values, &n_amplitude);
  for (i = 0; i < info->frequency_ins->n_param_specs; i++)
    subinstrument_values[i] = params[info->frequency_params[i]];
  frequency = bb_instrument_synth (info->frequency_ins, render_info, subinstrument_values, &n_frequency);

  if (n_amplitude != n || n_frequency != n)
    g_warning ("wave_train: short amplitude or frequency instrument?");

  n = MIN (n, MIN (n_amplitude, n_frequency));
  *n_samples_out = n;

  /* make raw (unmodulated) wave_train waveform */
  rv = g_new0 (gdouble, n);
  cur_fraction = 0;             /* what fraction of a overall waveform we are through */
  for (i = 0; i < n; i++)
    {
      cur_fraction += frequency[i] * one_over_sample_rate;
      if (cur_fraction >= 1.0)
        {
          guint j;
          guint n_add;
          do
            cur_fraction -= 1.0;
          while (cur_fraction >= 1.0);
          n_add = MIN (n - i, n_sub_rv);
          for (j = 0; j < n_add; j++)
            rv[i + j] += sub_rv[j];
        }
    }

  /* modulate raw wave_train data with amplitude waveform */
  for (i = 0; i < n; i++)
    rv[i] *= amplitude[i];

  /* cleanup */
  g_free (amplitude);
  g_free (frequency);
  g_free (sub_rv);

  return rv;
}

static void
free_wave_train_info (gpointer data)
{
  WaveTrainInfo *info = data;
  g_object_unref (info->subinstrument);
  g_free (info->subinstrument_params);
  g_free (info);
}

static BbInstrument *
construct_wave_train    (guint            n_args,
                         char           **args,
                         GError         **error)
{
  BbInstrument *burst, *amp, *freq;
  BbInstrument *rv;
  GPtrArray *params;
  WaveTrainInfo *info;
  if (n_args != 3)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                   "wavetrain requires 1 args: a burst instrment, an amplitude and frequency instruments");
      return NULL;
    }
  burst = bb_instrument_parse_string (args[0], error);
  if (burst == NULL)
    {
      gsk_g_error_add_prefix (error, "making burst for wave_train");
      return NULL;
    }
  amp = bb_instrument_parse_string (args[1], error);
  if (amp == NULL)
    {
      gsk_g_error_add_prefix (error, "making amplitude for wave_train");
      return NULL;
    }
  freq = bb_instrument_parse_string (args[2], error);
  if (freq == NULL)
    {
      gsk_g_error_add_prefix (error, "making frequency for wave_train");
      return NULL;
    }

  /* create initial params */
  params = g_ptr_array_new ();
  g_assert (params->len == PARAM_DURATION);
  g_ptr_array_add (params, param_spec_duration ());

  /* map params */
  info = g_new (WaveTrainInfo, 1);
  info->subinstrument = burst;
  info->subinstrument_params = g_new (guint, burst->n_param_specs);
  if (!bb_instrument_map_params (burst, info->subinstrument_params, params, error))
    {
      gsk_g_error_add_prefix (error, "wave_train's burst");
      return FALSE;
    }
  info->amplitude_ins = amp;
  info->amplitude_params = g_new (guint, amp->n_param_specs);
  if (!bb_instrument_map_params (amp, info->amplitude_params, params, error))
    {
      gsk_g_error_add_prefix (error, "wave_train's amplitude");
      return FALSE;
    }
  info->frequency_ins = freq;
  info->frequency_params = g_new (guint, freq->n_param_specs);
  if (!bb_instrument_map_params (freq, info->frequency_params, params, error))
    {
      gsk_g_error_add_prefix (error, "wave_train's frequency");
      return FALSE;
    }
  info->max_params = MAX (burst->n_param_specs, MAX (amp->n_param_specs, freq->n_param_specs));

  rv = bb_instrument_new_generic_v (NULL, params->len, (GParamSpec **) (params->pdata));
  rv->data = info;
  rv->free_data = free_wave_train_info;
  rv->synth_func = wave_train_synth;
  g_ptr_array_free (params, TRUE);
  return rv;
}

BB_INIT_DEFINE(_bb_wave_train_init)
{
  bb_instrument_template_constructor_register ("wave_train", construct_wave_train);
}
