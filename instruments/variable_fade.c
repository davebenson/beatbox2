#include "instrument-includes.h"
#include <gsk/gskghelpers.h>

typedef struct _VariableFadeInfo VariableFadeInfo;
struct _VariableFadeInfo
{
  BbInstrument *subs[3];
  guint *sub_arg_sources[3];
  guint max_instrument_params;
};

static const char *arg_names[3] =
{
  "a",
  "b",
  "fader"
};

static double *
variable_fade_synth  (BbInstrument *instrument,
                      BbRenderInfo *render_info,
                      GValue       *params,
                      guint        *n_samples_out)
{
  gdouble *a, *b, *fade;
  guint na,nb,nf;
  guint n;
  gdouble *rv;
  VariableFadeInfo *fi = instrument->data;
  GValue *remapped_params = g_newa (GValue, fi->max_instrument_params);
  guint i;
  for (i = 0; i < fi->subs[0]->n_param_specs; i++)
    remapped_params[i] = params[fi->sub_arg_sources[0][i]];
  a = bb_instrument_synth (fi->subs[0], render_info, remapped_params, &na);
  for (i = 0; i < fi->subs[1]->n_param_specs; i++)
    remapped_params[i] = params[fi->sub_arg_sources[1][i]];
  b = bb_instrument_synth (fi->subs[1], render_info, remapped_params, &nb);
  for (i = 0; i < fi->subs[2]->n_param_specs; i++)
    remapped_params[i] = params[fi->sub_arg_sources[2][i]];
  fade = bb_instrument_synth (fi->subs[2], render_info, remapped_params, &nf);
  n = MIN (na, nb);
  n = MIN (n, nf);
  *n_samples_out = n;
  rv = g_new (double, n);
  for (i = 0; i < n; i++)
    {
      //g_message ("fade %u: %.6f [a,b=%.6f,%.6f]",i,fade[i],a[i],b[i]);
      rv[i] = a[i] * (1.0-fade[i]) + b[i] * fade[i];
    }
  g_free (a);
  g_free (b);
  g_free (fade);
  return rv;
}

static void
free_variable_fade_info (gpointer data)
{
  VariableFadeInfo *fi = data;
  guint i;
  for (i = 0; i < 3; i++)
    {
      g_object_unref (fi->subs[i]);
      g_free (fi->sub_arg_sources[i]);
    }
  g_free (fi);
}

static BbInstrument *
construct_variable_fade (guint            n_args,
                         char           **args,
                         GError         **error)
{
  VariableFadeInfo *fi;
  guint i;
  GPtrArray *params = g_ptr_array_new ();
  BbInstrument *rv;
  if (n_args != 3)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                   "variable_fade needs three instruments: a,b,fader");
      return NULL;
    }

  fi = g_new (VariableFadeInfo, 1);
  fi->max_instrument_params = 0;
  for (i = 0; i < 3; i++)
    {
      fi->subs[i] = bb_instrument_parse_string (args[i], error);
      if (fi->subs[i] == NULL)
        {
          gsk_g_error_add_prefix (error, "parsing '%s' instrument of fader",
                                  arg_names[i]);
          return NULL;
        }
      fi->sub_arg_sources[i] = g_new (guint, fi->subs[i]->n_param_specs);
      if (!bb_instrument_map_params (fi->subs[i],
                                     fi->sub_arg_sources[i],
                                     params, error))
        {
          gsk_g_error_add_prefix (error, "mapping '%s' arguments of fader",
                                  arg_names[i]);
          return NULL;
        }
      fi->max_instrument_params = MAX (fi->max_instrument_params,
                                       fi->subs[i]->n_param_specs);
    }
  rv = bb_instrument_new_generic_v (NULL,
                                    params->len,
                                    (GParamSpec **) params->pdata);
  rv->synth_func = variable_fade_synth;
  rv->data = fi;
  rv->free_data = free_variable_fade_info;
  g_ptr_array_free (params, TRUE);
  return rv;
}

BB_INIT_DEFINE(_bb_variable_fade_init)
{
  bb_instrument_template_constructor_register ("variable_fade", construct_variable_fade);
}
