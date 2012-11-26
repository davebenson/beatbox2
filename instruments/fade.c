#include "instrument-includes.h"

static void combine_fade (double *inout, const double *in, guint len)
{
  guint i;
  double frac = 0;
  double step = 1.0 / len;
  for (i = 0; i < len; i++)
    {
      double v0 = (1.0 - frac) * (*inout);
      double v1 = (frac)       * (*in);
      *inout = v0 + v1;
      frac += step;
      inout++;
      in++;
    }
}
static BbInstrument *
construct_fade     (guint            n_args,
                    char           **args,
                    GError         **error)
{
  if (n_args != 2)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                   "fade can only take two arguments");
      return NULL;
    }
  return bb_construct_instrument_multiplex ("fade", combine_fade,
                                            n_args, args, error);
}

BB_INIT_DEFINE(_bb_fade_init)
{
  bb_instrument_template_constructor_register ("fade", construct_fade);
}
