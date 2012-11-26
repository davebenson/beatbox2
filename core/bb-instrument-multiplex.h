#ifndef __BB_INSTRUMENT_MULTIPLEX_H_
#define __BB_INSTRUMENT_MULTIPLEX_H_

typedef void (*BbMultiplexCombinerFunc) (double       *in_out,
                                         const double *mod,
                                         guint         len);

BbInstrument * bb_construct_instrument_multiplex(const char             *name,
                                                 BbMultiplexCombinerFunc combiner,
                                                 guint                   n_args,
                                                 char                  **args,
                                                 GError                **error);

#endif
