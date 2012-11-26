/* fractal instrument from
 * http://ccrma.stanford.edu/software/snd/snd/clm.html#oscil
 */

#include "instrument-includes.h"


enum
{
  PARAM_START_VALUE,
  PARAM_FACTOR,
  PARAM_DURATION
};

static const BbInstrumentSimpleParam fractal0_params[] =
{
  { PARAM_START_VALUE, "start_value", 0.0 },
  { PARAM_FACTOR, "factor", 0.5 },
};

/* --- sin --- */
static double *
fractal0_synth  (BbInstrument *instrument,
            BbRenderInfo *render_info,
            GValue       *params,
            guint        *n_samples_out)
{
  gdouble x = g_value_get_double (params + PARAM_START_VALUE);
  gdouble m = g_value_get_double (params + PARAM_FACTOR);
  guint n = render_info->note_duration_samples;
  guint i;
  gdouble *rv = g_new (double, n);
  *n_samples_out = n;

  for (i = 0; i < n; i++)
    {
      rv[i] = x;
      x = 1.0 - m * x * x;
    }
  return rv;
}

/* from attract on
 * http://ccrma.stanford.edu/software/snd/snd/clm.html#oscil
 * which apparently comes from James McCartney, from CMJ vol 21 no 3 p 6.
 */
enum
{
  PARAM_FRACTAL1_C,             /* 1..10 */
  PARAM_FRACTAL1_DURATION
};

static const BbInstrumentSimpleParam fractal1_params[] =
{
  { PARAM_FRACTAL1_C, "c", 5.0 },
};

static double *
fractal1_synth  (BbInstrument *instrument,
            BbRenderInfo *render_info,
            GValue       *params,
            guint        *n_samples_out)
{
  gdouble c = g_value_get_double (params + PARAM_FRACTAL1_C);
  guint n = render_info->note_duration_samples;
  guint i;
  gdouble *rv = g_new (double, n);
  gdouble a, b, dt, scale, x, y, z;
  *n_samples_out = n;

  a = 0.2;
  b = 0.2;
  dt = 0.04;
  scale = 0.5 / c;
  x = -1.0;
  y = 0.0;
  z = 0.0;
  for (i = 0; i < n; i++)
    {
      gdouble x1 = x - dt * (y + z);
      y += dt * (x + a * y);
      z += dt * ((b + x * z) - c * z);
      x = x1;
      rv[i] = x * scale;
    }
  return rv;
}



BB_INIT_DEFINE(_bb_fractal_init)
{
  BbInstrument *instrument;

  instrument =
  bb_instrument_new_from_simple_param (G_N_ELEMENTS (fractal0_params),
                                       fractal0_params);
  bb_instrument_add_param (instrument, PARAM_DURATION, param_spec_duration ());
  instrument->synth_func = fractal0_synth;
  bb_instrument_set_name (instrument, "fractal0");

  instrument =
  bb_instrument_new_from_simple_param (G_N_ELEMENTS (fractal1_params),
                                       fractal1_params);
  bb_instrument_add_param (instrument, PARAM_FRACTAL1_DURATION, param_spec_duration ());
  instrument->synth_func = fractal1_synth;
  bb_instrument_set_name (instrument, "fractal1");
}
