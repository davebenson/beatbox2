#include <stdlib.h>
#include "../core/bb-tapped-delay-line.h"
#include "../core/bb-utils.h"
#include "../core/bb-delay-line.h"

int main()
{
  gdouble rad_per_sample = 1.0;		/* approx 10khz sound */
  gdouble delays[20];
  guint i;
  guint n_delays = G_N_ELEMENTS (delays);
  BbDelayLine *dl;
  BbTappedDelayLine *tdl;
  gdouble total_lin = 0, total_ap = 0;
  guint total_count = 0;
  for (i = 0; i < n_delays; i++)
    delays[i] = g_random_double_range (0, 10); 
  qsort (delays, n_delays, sizeof (double), bb_compare_doubles);
  for (i = 0; i < n_delays; i++)
    g_message ("delay[%u] is %.6f", i, delays[i]);
  g_print ("ideal\taperror\tlinerror\n");
  tdl = bb_tapped_delay_line_new (n_delays, delays);
  dl = bb_delay_line_new (15);
  for (i = 0; i < 10000; i++)
    {
      /* feed data in, ignoring the taps */
      gdouble rad = fmod (rad_per_sample * i, BB_2PI);
      gdouble in = sin (rad);
      bb_tapped_delay_line_tick (tdl, in);
      bb_delay_line_add (dl, in);
      if (i > 100)
        {
          guint t;
	  for (t = 0; t < n_delays; t++)
	    {
              gdouble erad = rad - delays[t] * rad_per_sample;
	      gdouble expected = sin(erad);
              gdouble ap_out = tdl->taps[t].last_output;
              gdouble lin_out = bb_delay_line_get_lin (dl, delays[t]);
              gdouble ap_delta = ABS (ap_out - expected);
              gdouble lin_delta = ABS (lin_out - expected);
#if 0
	      g_print ("%.7f\t%.7f\t%.7f\n",
	               expected,
		       - expected,
		        - expected);
#elif 0
	      g_print ("%u\t%.7f\t%.7f\t%.7f\t%.7f\n",
                       t, erad,
	               expected,
		       tdl->taps[t].last_output,
		       bb_delay_line_get_lin (dl, delays[t]));
#endif
              total_ap += ap_delta;
              total_lin += lin_delta;
              total_count++;
	    }
        }
    }
  g_message ("Allpass: Average absolute error: %.6f", total_ap/total_count);
  g_message ("Linear: Average absolute error: %.6f", total_lin/total_count);
  return 0;
}
