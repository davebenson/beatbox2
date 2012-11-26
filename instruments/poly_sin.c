#include "instrument-includes.h"

typedef struct _Osc Osc;
struct _Osc
{
  BbComplex cur, factor;
};

static inline void
osc_init (double radians_per_sample,
          double amplitude,
          Osc    *out)
{
  out->cur.re = amplitude;
  out->cur.im = 0;
  out->factor.re = cos (radians_per_sample);
  out->factor.im = sin (radians_per_sample);
}

static inline void
osc_step (Osc *inout)
{
  BB_COMPLEX_MUL (inout->cur, inout->cur, inout->factor);
}

/* --- poly_sin --- */
static double *
poly_sin_synth  (BbInstrument *instrument,
                 BbRenderInfo *render_info,
                 GValue       *params,
                 guint        *n_samples_out)
{
  double freq            = g_value_get_double (params + 1);
  double radians_per_sample = M_PI * 2.0 * freq / render_info->sampling_rate;
  guint n = render_info->note_duration_samples;
  guint i, j;
  gdouble *rv = g_new (double, n);
  const BbDoubleArray *arr = g_value_get_boxed (params + 2);
  guint n_osc;
  Osc *osc;
  *n_samples_out = n;

  g_assert (arr != NULL);
  if (arr->n_values % 2 != 0)
    g_error ("double-array for poly-sin is expected to be freq/amp pairs, got odd number of elements");

  n_osc = arr->n_values / 2;
  osc = g_newa (Osc, n_osc);
  for (j = 0; j < n_osc; j++)
    osc_init (radians_per_sample * arr->values[2*j+0],
              arr->values[2*j+1],
              osc + j);

  for (i = 0; i < n; i++)
    {
      gdouble samp = osc[0].cur.re;
      for (j = 1; j < n_osc; j++)
        samp += osc[j].cur.re;
      rv[i] = samp;
      for (j = 0; j < n_osc; j++)
        osc_step (osc + j);
    }
  return rv;
}

BB_INIT_DEFINE(_bb_poly_sin_init)
{
  BbInstrument *instrument =
  bb_instrument_new_generic ("poly_sin",
                             param_spec_duration (),
                             param_spec_frequency (),
			     g_param_spec_boxed ("overtones", "Overtones", NULL,
                                                 BB_TYPE_DOUBLE_ARRAY, 0),
                             NULL);
  instrument->synth_func = poly_sin_synth;
}
