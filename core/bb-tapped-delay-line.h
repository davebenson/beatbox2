#ifndef __BB_TAPPED_DELAY_LINE_H_
#define __BB_TAPPED_DELAY_LINE_H_

typedef struct _BbTappedDelayLineTap BbTappedDelayLineTap;
typedef struct _BbTappedDelayLine BbTappedDelayLine;

#include <glib.h>

struct _BbTappedDelayLineTap
{
  guint int_delay;      /* if 0, resort to linear interpolation.
                           else, the delay is actualy 1 int shorter than
                           this, by the time the all-pass is considered. */
  gdouble coeff;
  gdouble last_output;
};

struct _BbTappedDelayLine
{
  guint n_taps;
  BbTappedDelayLineTap *taps;
  guint n_samples;
  gdouble *samples;
  guint next_to_fill;   /* next buffer location to write into. */
};

/* warning: delays must be sorted ascending; must be positive */
BbTappedDelayLine *bb_tapped_delay_line_new (guint              n_delays,
                                             const gdouble     *delays);
G_INLINE_FUNC void bb_tapped_delay_line_tick(BbTappedDelayLine *line,
                                             gdouble            input);
              void bb_tapped_delay_line_tick_n(BbTappedDelayLine *line,
                                               guint              n,
                                               const gdouble     *inputs,
                                               gdouble          **tap_outputs);
void               bb_tapped_delay_line_free(BbTappedDelayLine *line);


#if defined (G_CAN_INLINE) || defined (__BB_DEFINE_INLINES__)
G_INLINE_FUNC void
bb_tapped_delay_line_tick(BbTappedDelayLine *line,
                          gdouble            input)
{
  guint tap_index = 0;
  guint write_index;
  guint delay_0_index = line->next_to_fill;
  guint n_taps = line->n_taps;
  BbTappedDelayLineTap *taps = line->taps;
  line->samples[line->next_to_fill++] = input;
  if (line->next_to_fill == line->n_samples)
    line->next_to_fill = 0;
  write_index = line->next_to_fill;

  //g_message ("input %.6f written to index %u", input, delay_0_index);

  /* the initial taps may require linear interpolation */
  if (taps[0].int_delay == 0)
    {
      gdouble input1 = (write_index >= 2) ? line->samples[write_index - 2] : line->samples[line->n_samples-2+write_index];
      do
        {
          taps[tap_index].last_output = taps[tap_index].coeff * input1
                                      + (1.0 - taps[tap_index].coeff) * input;
          tap_index++;
        }
      while (tap_index < n_taps && taps[tap_index].int_delay == 0);
    }

  /* subsequent taps go through the all-pass filter pathway */
  for (             ; tap_index < n_taps; tap_index++)
    {
      /* find input to allpass filter */
      guint id = taps[tap_index].int_delay - 1;     /* ranges from [0..n_samples-1] */
      guint d0i = delay_0_index;
      gdouble ap_input, last_ap_input;
      gdouble coeff, out;
      if (d0i > id)
        {
          last_ap_input = line->samples[d0i - id - 1];
          ap_input = line->samples[d0i - id];
        }
      else if (d0i == id)
        {
          last_ap_input = line->samples[line->n_samples - 1];
          ap_input = line->samples[0];
        }
      else
        {
          last_ap_input = line->samples[line->n_samples + d0i - id - 1];
          ap_input = line->samples[line->n_samples + d0i - id];
        }

      /* update filter */
      coeff = taps[tap_index].coeff;
      out = -coeff * taps[tap_index].last_output
          + coeff * ap_input
          + last_ap_input;
      //g_message ("tap[%u]: last-out=%.6f, ap_input=%.6f, last_ap_input=%.6f, coeff=%.6f; new out=%.6f",tap_index,taps[tap_index].last_output,ap_input,last_ap_input,coeff,out);
      taps[tap_index].last_output = out;
    }
}
#endif

#endif

