#include "instrument-includes.h"

typedef struct _VibratoData VibratoData;
struct _VibratoData
{
  BbInstrument *freq_instrument;
};

static double *
vibrato_synth   (BbInstrument *instrument,
                 BbRenderInfo *render_info,
                 GValue       *params,
                 guint        *n_samples_out)
{
  VibratoData *vd = instrument->data;
  guint n_subrv;
  double *subrv = bb_instrument_synth (vd->freq_instrument, render_info, params, &n_subrv);
  double *rv = g_new (double, n_subrv);
  double radians_per_hertz_per_sample = 2.0 * M_PI / render_info->sampling_rate;
  double radians = 0;
  guint i;
  for (i = 0; i < n_subrv; i++)
    {
      rv[i] = sin (radians);
      radians += subrv[i] * radians_per_hertz_per_sample;
      if (radians < 0 || radians >= 2.0 * M_PI)
        radians = fmod (radians, 2.0 * M_PI);
    }
  *n_samples_out = n_subrv;
  g_free (subrv);
  return rv;
}

static void
vibrato_data_free (gpointer data)
{
  VibratoData *vd = data;
  g_object_unref (vd->freq_instrument);
  g_free (vd);
}

static BbInstrument *
construct_vibrato (guint            n_args,
                   char           **args,
                   GError         **error)
{
  VibratoData *vd;
  BbInstrument *in = bb_instrument_from_string (args[0]);
  BbInstrument *rv;
  g_assert (in);
  vd = g_new (VibratoData, 1);
  vd->freq_instrument = in;
  rv = bb_instrument_new_generic_v (NULL,
                                    in->n_param_specs,
                                    in->param_specs);
  rv->synth_func = vibrato_synth;
  rv->data = vd;
  rv->free_data = vibrato_data_free;
  return rv;
}

BB_INIT_DEFINE(_bb_vibrato_init)
{
  bb_instrument_template_constructor_register ("vibrato", construct_vibrato);
}
