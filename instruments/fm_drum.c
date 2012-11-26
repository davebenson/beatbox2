/* jan mattox's fm drum */
#include "instrument-includes.h"
#include "../core/bb-waveform.h"

/*
def fm_drum(start, dur, freq, amp, index, high = false,
            degree = 0.0, distance = 1.0, rev_amount = 0.01)
  casrat = high ? 8.525 : 3.515
  fmrat = high ? 3.414 : 1.414
  glsf = make_env(:envelope, [0, 0, 25, 0, 75, 1, 100, 1],
                  :scaler, high ? hz2radians(66) : 0.0, :duration, dur)
  ampfun = [0, 0, 3, 0.05, 5, 0.2, 7, 0.8, 8, 0.95, 10, 1.0, 12, 0.95, 20, 0.3, 30, 0.1, 100, 0]
  atdrpt = 100 * (high ? 0.01 : 0.015) / dur
  ampf = make_env(:envelope, stretch_envelope(ampfun, 10, atdrpt, 15,
                                              [atdrpt + 1, 100 - 100 * ((dur - 0.2) / dur)].max),
                  :scaler, amp, :duration, dur)
  indxfun = [0, 0, 5, 0.014, 10, 0.033, 15, 0.061, 20, 0.099,
             25, 0.153, 30, 0.228, 35, 0.332, 40, 0.477,
             45, 0.681, 50, 0.964, 55, 0.681, 60, 0.478, 65, 0.332,
             70, 0.228, 75, 0.153, 80, 0.099, 85, 0.061,
             90, 0.033, 95, 0.0141, 100, 0]
  indxpt = 100 - 100 * ((dur - 0.1) / dur)
  divindxf = stretch_envelope(indxfun, 50, atdrpt, 65, indxpt)
  indxf = make_env(:envelope, divindxf, :duration, dur,
                   :scaler, [hz2radians(index * fmrat * freq), PI].min)
  mindxf = make_env(:envelope, divindxf, :duration, dur,
                    :scaler, [hz2radians(index * casrat * freq), PI].min)
  devf = make_env(:envelope, stretch_envelope(ampfun, 10, atdrpt, 90,
                                              [atdrpt + 1, 100 - 100 * ((dur - 0.05) / dur)].max),
                  :scaler, [hz2radians(7000), PI].min, :duration, dur)
  rn = make_rand(:frequency, 7000, :amplitude, 1)
  carrier = make_oscil(:frequency, freq)
  fmosc = make_oscil(:frequency, freq * fmrat)
  cascade = make_oscil(:frequency, freq * casrat)
  run_instrument(start, dur, :degree, degree, :distance, distance, :reverb_amount, rev_amount) do
    gls = env(glsf)
    env(ampf) * oscil(carrier,
                      gls + env(indxf) * oscil(fmosc,
                                               gls * fmrat +
                                                    env(mindxf) * oscil(cascade,
                                                                        gls * casrat +
                                                                             env(devf) *
                                                                             rand(rn))))
  end
end
*/

enum
{
  PARAM_FREQUENCY,
  PARAM_GLS_SCALE,
  PARAM_INDEX,
  PARAM_CASCADE_RATIO,
  PARAM_FM_RATIO,
  PARAM_DURATION
};

static BbInstrumentSimpleParam fm_drum_params[] =
{
  { PARAM_FREQUENCY,            "frequency",                 440 },
  { PARAM_GLS_SCALE,            "gls_scale",                 66 },              /* high:66, !high:0 */
  { PARAM_INDEX,                "index",                     5 },
  { PARAM_CASCADE_RATIO,        "cascade_ratio",             8.525 },           /* high:8.525, !high:3.515 */
  { PARAM_FM_RATIO,             "fm_ratio",                  3.414 },           /* high:3.414, !high:1.414 */
};

/* so, to turn off "high" use:
   {gls_scale=0} {cascade_ratio=3.515} {fm_ratio=1.414}
 */

static BbWaveform *
peek_gls_waveform (void)
{
  static gdouble envelope[] = { 0,0, 0.25,0, 0.75,1.0, 1.0,1.0 };
  static BbWaveform *rv = NULL;
  if (rv == NULL)
    rv = bb_waveform_new_linear (G_N_ELEMENTS (envelope) / 2, envelope);
  return rv;
}

static BbWaveform *
peek_ampfun_waveform (void)
{
  static gdouble envelope[] = {
    0.00, 0,
    0.03, 0.05,
    0.05, 0.2,
    0.07, 0.8,
    0.08, 0.95,
    0.10, 1.0,
    0.12, 0.95,
    0.20, 0.3,
    0.30, 0.1,
    1.00, 0
  };
  static BbWaveform *rv = NULL;
  if (rv == NULL)
    rv = bb_waveform_new_linear (G_N_ELEMENTS (envelope) / 2, envelope);
  return rv;
}

static BbWaveform *
peek_index_waveform (void)
{
  static gdouble envelope[] = {
    0.00, 0,
    0.05, 0.014,
    0.10, 0.033,
    0.15, 0.061,
    0.20, 0.099,
    0.25, 0.153,
    0.30, 0.228,
    0.35, 0.332,
    0.40, 0.477,
    0.45, 0.681,
    0.50, 0.964,
    0.55, 0.681,
    0.60, 0.478,
    0.65, 0.332,
    0.70, 0.228,
    0.75, 0.153,
    0.80, 0.099,
    0.85, 0.061,
    0.90, 0.033,
    0.95, 0.0141,
    1.00, 0
  };
  static BbWaveform *rv = NULL;
  if (rv == NULL)
    rv = bb_waveform_new_linear (G_N_ELEMENTS (envelope) / 2, envelope);
  return rv;
}

static double *
fm_drum_synth   (BbInstrument *instrument,
                 BbRenderInfo *render_info,
                 GValue       *params,
                 guint        *n_samples_out)
{
  double frequency       = g_value_get_double (params + PARAM_FREQUENCY);
  double gls_scale       = g_value_get_double (params + PARAM_GLS_SCALE);
  double index           = g_value_get_double (params + PARAM_INDEX);
  double cascade_ratio   = g_value_get_double (params + PARAM_CASCADE_RATIO);
  double fm_ratio        = g_value_get_double (params + PARAM_FM_RATIO);
  gdouble duration_secs = render_info->note_duration_secs;
  guint n = render_info->note_duration_samples;
  double one_over_n = 1.0 / n;
  double *rv;
  double *gls_env, *amp_env, *index_env, *mindx_env, *dev_env;
  gdouble index_scale, mindex_scale, dev_scale;
  gdouble rate = render_info->sampling_rate;
  gdouble atdr_point, index_point;
  guint i;
  BbWaveform *waveform;
  gdouble carrier_pos, carrier_step;
  gdouble fm_pos, fm_step;
  gdouble cascade_pos, cascade_step;

  gdouble rand_pos = 0, rand_step = 7000.0 / rate;
  gdouble rand_value = g_random_double_range (-1, 1);

  *n_samples_out = n;
  rv = g_new (double, n);
  gls_env = g_new (double, n);
  amp_env = g_new (double, n);
  index_env = g_new (double, n);
  mindx_env = g_new (double, n);
  dev_env = g_new (double, n);

  /* HACK */
  {static gboolean displayed_warnings=FALSE;
    if (!displayed_warnings){
     g_message ("warning: fm_drum parameters may change... i think it's buggy!");
     displayed_warnings=TRUE;}}

  for (i = 0; i < n; i++)
    gls_env[i] = amp_env[i] = index_env[i] = mindx_env[i] = dev_env[i] = one_over_n * i;

  /* Compute 'gls' envelope */
  bb_waveform_eval_array (peek_gls_waveform (), gls_env, n);
  gls_scale *= BB_2PI / rate;
  for (i = 0; i < n; i++)
    gls_env[i] *= gls_scale;

  atdr_point = 0.01 / duration_secs;   /* NOTE: if high, used to be 0.015 */

  /* Compute 'amp' envelope */
  waveform = bb_waveform_new_adjust_attack_decay (peek_ampfun_waveform (),
                                           0.1, atdr_point,
                                           0.15, MAX (0.01 + atdr_point, 1.0 - (duration_secs - 0.2) / duration_secs));
  bb_waveform_eval_array (waveform, amp_env, n);
  bb_waveform_unref (waveform);

  /* Compute 'index' and 'mindex' envelopes */
  index_point = 1.0 - (duration_secs - 0.1) / duration_secs;
  /* set waveform to what was called divindxf */
  waveform = bb_waveform_new_adjust_attack_decay (peek_index_waveform (),
                                           0.5, atdr_point, 0.65, index_point);
  bb_waveform_eval_array (waveform, index_env, n);
  index_scale = index * fm_ratio * frequency * BB_2PI / rate;
  if (index_scale > G_PI)
    index_scale = G_PI;
  mindex_scale = index * cascade_ratio * frequency * BB_2PI / rate;
  if (mindex_scale > G_PI)
    mindex_scale = G_PI;
  for (i = 0; i < n; i++)
    {
      gdouble in = index_env[i];
      index_env[i] = in * index_scale;
      mindx_env[i] = in * mindex_scale;
    }
  bb_waveform_unref (waveform);


  /* Compute 'dev' envelope */
  waveform = bb_waveform_new_adjust_attack_decay (peek_ampfun_waveform (),
                                           0.1, atdr_point,
                                           0.9, MAX (0.01 + atdr_point, 1.0 - (duration_secs - 0.05) / duration_secs));
  dev_scale = 7000 * BB_2PI / rate;
  bb_waveform_eval_array (waveform, dev_env, n);
  for (i = 0; i < n; i++)
    dev_env[i] *= dev_scale;
  bb_waveform_unref (waveform);

  /* our oscillators are mere doubles telling the position and step
     of each oscillator */
  carrier_pos = 0;
  carrier_step = BB_2PI * frequency / rate;
  fm_pos = 0;
  fm_step = carrier_step * fm_ratio;
  cascade_pos = 0;
  cascade_step = carrier_step * cascade_ratio;

#define RENORMALIZE_ANGLE(var)                                        \
  G_STMT_START{                                                       \
    if (var > M_PI * 3.0 || var < -M_PI * 3.0)                        \
      var = fmod (var, BB_2PI);                                       \
  }G_STMT_END

  for (i = 0; i < n; i++)
    {
      gdouble cascade_output;
      gdouble fm_output;
      gdouble carrier_output;

      rand_pos += rand_step;
      if (rand_pos >= 1.0)
        {
          rand_value = g_random_double_range (-1, 1);
          rand_pos -= 1.0;
        }
      cascade_output = sin (cascade_pos);
      cascade_pos += cascade_step;
      cascade_pos += gls_env[i] * cascade_ratio;
      cascade_pos += dev_env[i] * rand_value;
      RENORMALIZE_ANGLE (cascade_pos);

      fm_output = sin (fm_pos);
      fm_pos += fm_step;
      fm_pos += gls_env[i] * fm_ratio;
      fm_pos += mindx_env[i] * cascade_output;
      RENORMALIZE_ANGLE (fm_pos);

      carrier_output = sin (carrier_pos);
      carrier_pos += carrier_step;
      carrier_pos += gls_env[i];
      carrier_pos += index_env[i] * fm_output;
      RENORMALIZE_ANGLE (carrier_pos);

      rv[i] = carrier_output * amp_env[i];
    }

#undef RENORMALIZE_ANGLE

  g_free (gls_env);
  g_free (amp_env);
  g_free (index_env);
  g_free (mindx_env);
  g_free (dev_env);

  return rv;
}
BB_INIT_DEFINE(_bb_fm_drum_init)
{
  BbInstrument *instrument = bb_instrument_new_from_simple_param (G_N_ELEMENTS (fm_drum_params),
                                                                  fm_drum_params);
  bb_instrument_add_param (instrument, PARAM_DURATION, param_spec_duration ());
  instrument->synth_func = fm_drum_synth;
  bb_instrument_set_name (instrument, "fm_drum");
}
