#include "instrument-includes.h"

static void combine_mul (double *inout, const double *in, guint len)
{
  guint i;
  for (i = 0; i < len; i++)
    *inout++ *= *in++;
}

static BbInstrument *
construct_modulate (guint            n_args,
                    char           **args,
                    GError         **error)
{
  return bb_construct_instrument_multiplex ("modulate", combine_mul,
                                            n_args, args, error);
}

BB_INIT_DEFINE(_bb_modulate_init)
{
  bb_instrument_template_constructor_register ("modulate", construct_modulate);
}
