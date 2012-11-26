#include <string.h>
#include "bb-error.h"
#include "bb-instrument.h"
#include "bb-utils.h"
#include "bb-instrument-multiplex.h"
#include <gsk/gskghelpers.h>

typedef struct _MultiplexData MultiplexData;
struct _MultiplexData
{
  guint n_instruments;
  BbInstrument **instruments;
  guint **subparam_sources;
  gboolean use_min_time;                /* else use max time */
  BbMultiplexCombinerFunc combiner;
};

static double *
multiplex_synth (BbInstrument *instrument,
                 BbRenderInfo *render_info,
                 GValue       *params,
                 guint        *n_samples_out)
{
  MultiplexData *md = instrument->data;
  GValue *vbuf = g_newa (GValue, instrument->n_param_specs);
  double **subrvs = g_newa (double *, md->n_instruments);
  guint *subrv_lens = g_newa (guint, md->n_instruments);
  guint rv_len = md->use_min_time ? G_MAXUINT : 0;
  double *rv;
  guint i;
  /* the init_index is the one to initialize the returned-buffer with.
     This only matters in the !use_min_time case, because we
     want to copy the longest buffer first */
  guint init_index = 0;
  for (i = 0; i < md->n_instruments; i++)
    {
      BbInstrument *in = md->instruments[i];
      guint subrv;
      guint p;
      for (p = 0; p < in->n_param_specs; p++)
        vbuf[p] = params[md->subparam_sources[i][p]];
      subrvs[i] = bb_instrument_synth (in, render_info, vbuf, &subrv);
      subrv_lens[i] = subrv;
      if (md->use_min_time)
        rv_len = MIN (rv_len, subrv);
      else
        rv_len = MAX (rv_len, subrv);
      if (rv_len == subrv)
        init_index = i;
    }
  rv = g_new (gdouble, rv_len);
  /* if use_min_time, then the following holds for any init_index value.
     if !use_min_time, then this implies that subrv_lens[init_index]==rv_len,
     ie this is a maximal length subsound. */
  g_assert (subrv_lens[init_index] >= rv_len);

  /* however, we wish to support fading in a trivial, albeit hackish, way,
     so we are going to make use_min_time cause the combiner
     to always set init_index==0, meaning that the "combined" source
     is always the second one */
  if (md->use_min_time)
    init_index = 0;

  memcpy (rv, subrvs[init_index], rv_len * sizeof (double));

  for (i = 0; i < md->n_instruments; i++)
    if (i != init_index)
      md->combiner (rv, subrvs[i], MIN (subrv_lens[i], rv_len));

  for (i = 0; i < md->n_instruments; i++)
    g_free (subrvs[i]);
  *n_samples_out = rv_len;
  return rv;
}

BbInstrument *
bb_construct_instrument_multiplex(const char      *name,
                                  BbMultiplexCombinerFunc combiner,
                                  guint            n_args,
                                  char           **args,
                                  GError         **error)
{
  MultiplexData *data = g_new (MultiplexData, 1);
  BbInstrument *rv;
  BbInstrument **instruments = g_new (BbInstrument *, n_args);
  GPtrArray *params = g_ptr_array_new ();
  guint i;
  data->subparam_sources = g_new (guint *, n_args);
  data->n_instruments = n_args;
  data->instruments = instruments;
  data->use_min_time = TRUE;
  data->combiner = combiner;
  for (i = 0; i < n_args; i++)
    {
      instruments[i] = bb_instrument_from_string (args[i]);
      if (instruments[i] == NULL)
        {
          g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                       "error parsing instrument from arg %u of %s", i, name);
          return NULL;
        }
      data->subparam_sources[i] = g_new (guint, instruments[i]->n_param_specs);
      if (!bb_instrument_map_params (instruments[i], data->subparam_sources[i], params, error))
        {
          gsk_g_error_add_prefix (error, "error adding parameters to %s", name);
          return NULL;
        }
    }

  rv = bb_instrument_new_generic_v (NULL, params->len, (GParamSpec **) (params->pdata));
  rv->synth_func = multiplex_synth;
  g_ptr_array_free (params, TRUE);
  rv->data = data;
  return rv;
}
