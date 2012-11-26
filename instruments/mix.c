#include "instrument-includes.h"

static void combine_add (double *inout, const double *in, guint len)
{
  guint i;
  for (i = 0; i < len; i++)
    *inout++ += *in++;
}

static BbInstrument *
construct_mix (guint            n_args,
               char           **args,
               GError         **error)
{
  return bb_construct_instrument_multiplex ("mix", combine_add,
                                            n_args, args, error);
}

BB_INIT_DEFINE(_bb_mix_init)
{
  bb_instrument_template_constructor_register ("mix", construct_mix);
}
