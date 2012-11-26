#include <math.h>
#include "bb-tapped-delay-line.h"

BbTappedDelayLine *
bb_tapped_delay_line_new (guint              n_delays,
                          const gdouble     *delays)
{
  guint i;
  guint delay_line_len;
  BbTappedDelayLine *dl;
  g_assert (n_delays > 0);
  for (i = 1; i < n_delays; i++)
    g_assert (delays[i-1] <= delays[i]);

  delay_line_len = (guint) (gint) ceil (delays[n_delays-1]) + 3;
  dl = g_new (BbTappedDelayLine, 1);
  dl->n_taps = n_delays;
  dl->taps = g_new (BbTappedDelayLineTap, n_delays);
  dl->n_samples = delay_line_len;
  dl->samples = g_new0 (gdouble, delay_line_len);
  dl->next_to_fill = 0;

  /* handle short delays that will require linear interpolation */
  for (i = 0; i < n_delays; i++)
    {
      if (delays[i] > 0.5)
        break;
      dl->taps[i].coeff = delays[i];
      dl->taps[i].int_delay = 0;
      /* last output is unneeded in the linear interp case */
    }

  for (     ; i < n_delays; i++)
    {
      /* The allpass filter will have a delay between 0.5 and 1.5. */
      gint real_int_delay;
      gdouble allpass_delay;
      dl->taps[i].int_delay = floor (delays[i] + 0.5);
      g_assert (dl->taps[i].int_delay > 0);
      /* an int_delay of 1 corresponds to using the info from
         times t-0 and t-1.  that's cuz int_delay==0 is reserved
         for linear-interp mode */
      real_int_delay = dl->taps[i].int_delay - 1;

      /* and thus the delay needed for the allpass is: */
      allpass_delay = delays[i] - real_int_delay;

      /* and the allpass coefficient is: */
      dl->taps[i].coeff = (1.0 - allpass_delay) / (1.0 + allpass_delay);

      dl->taps[i].last_output = 0;
    }
  return dl;
}

void bb_tapped_delay_line_tick_n(BbTappedDelayLine *line,
                                 guint              n,
                                 const gdouble     *inputs,
                                 gdouble          **tap_outputs)
{
  /* TODO: should have an optimized codepath for n > line->n_samples */
  guint i;
  guint t;
  guint n_taps = line->n_taps;
  for (i = 0; i < n; i++)
    {
      bb_tapped_delay_line_tick (line, inputs[i]);
      for (t = 0; t < n_taps; t++)
        tap_outputs[t][i] = line->taps[t].last_output;
    }
}

void
bb_tapped_delay_line_free(BbTappedDelayLine *line)
{
  g_free (line->taps);
  g_free (line->samples);
  g_free (line);
}
