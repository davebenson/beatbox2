#ifndef __BB_GNUPLOT_H_
#define __BB_GNUPLOT_H_

#include <glib.h>

void output_double_array_as_graph (guint n_samples,
                                   const double *samples,
		                   guint sampling_rate,
		                   const char *png_filename);

#endif
