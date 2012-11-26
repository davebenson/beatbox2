#include "instrument-includes.h"
#include <gsk/gskghelpers.h>
#include "../core/bb-vm.h"

typedef struct _ParamMapData ParamMapData;
struct _ParamMapData
{
  BbProgram **vars;
  BbInstrument *base;
};

static void
reverse (guint n, double *inout)
{
  double *end = inout + n - 1;
  n /= 2;
  while (n-- > 0)
    {
      double tmp = *inout;
      *inout = *end;
      *end = tmp;
      end--;
      inout++;
    }
}

static double *
reverse_synth (BbInstrument *instrument,
               BbRenderInfo *render_info,
               GValue       *params,
               guint        *n_samples_out)
{
  BbInstrument *sub = instrument->data;
  double *rv = bb_instrument_synth (sub, render_info, params, n_samples_out);
  reverse (*n_samples_out, rv);
  return rv;
}

static BbInstrument *
construct_reverse (guint            n_args,
                   char           **args,
                   GError         **error)
{
  BbInstrument *base;
  BbInstrument *rv;
  if (n_args != 1)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                   "reverse requires 1 arg");
      return NULL;
    }
  base = bb_instrument_from_string (args[0]);
  rv = bb_instrument_new_generic_v (NULL,
                                    base->n_param_specs,
                                    base->param_specs);
  rv->synth_func = reverse_synth;
  rv->data = base;
  rv->free_data = g_object_unref;
  return rv;
}

BB_INIT_DEFINE(_bb_reverse_init)
{
  bb_instrument_template_constructor_register ("reverse", construct_reverse);
}
