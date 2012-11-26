#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "bb-gnuplot.h"

void output_double_array_as_graph (guint n_samples,
                                   const double *samples,
		                   guint sampling_rate,
		                   const char *png_filename)
{
  static guint seqno = 0;
  char *tmp_filename = g_strdup_printf ("tmpg-%u-%u-%u.dat",(guint)time(NULL),(guint)getpid(), seqno++);
  FILE *out_fp = fopen (tmp_filename, "wb");
  FILE *gnuplot_fp;
  guint i;
  g_message ("output_double_array_as_graph (n_samples=%u)", n_samples);
  for (i = 0; i < n_samples; i++)
    {
      fprintf(out_fp,"%.15f\t%.15f\n", (double)i / sampling_rate, samples[i]);
    }
  fclose (out_fp);
  gnuplot_fp = popen ("gnuplot", "w");
  fprintf (gnuplot_fp,
           "set terminal png\n"
	   "set output \"%s\"\n", png_filename);
  fprintf (gnuplot_fp,
           "plot \"%s\" with lines\n", tmp_filename);
  fflush (gnuplot_fp);
  if (pclose (gnuplot_fp) != 0)
    g_error ("error running gnuplot");
  unlink (tmp_filename);
  g_free (tmp_filename);
  g_message ("output_double_array_as_graph: done");
}
