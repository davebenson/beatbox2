#ifndef __BB_DELAY_LINE_H_
#define __BB_DELAY_LINE_H_

typedef struct _BbDelayLine BbDelayLine;

#include <math.h>
#include <glib.h>

struct _BbDelayLine
{
  guint n;
  guint write_offset;
  gdouble *buffer;
};

G_INLINE_FUNC BbDelayLine *bb_delay_line_new     (guint n);
G_INLINE_FUNC gdouble      bb_delay_line_add     (BbDelayLine *line,
                                                  gdouble      value);
G_INLINE_FUNC gdouble      bb_delay_line_get_0   (BbDelayLine *line,
                                                  guint        t);
G_INLINE_FUNC gdouble      bb_delay_line_get_lin (BbDelayLine *line,
                                                  gdouble      t);
G_INLINE_FUNC gdouble      bb_delay_line_get_sinc (BbDelayLine *line,
                                                   gdouble      t);
G_INLINE_FUNC void         bb_delay_line_free    (BbDelayLine *line);
 

#if defined (G_CAN_INLINE) || defined (__BB_DEFINE_INLINES__)
G_INLINE_FUNC BbDelayLine *
bb_delay_line_new (guint n)
{
  BbDelayLine *line = g_new (BbDelayLine, 1);
  line->buffer = g_new0 (gdouble, n);
  line->write_offset = 0;
  line->n = n;
  return line;
}

G_INLINE_FUNC void
bb_delay_line_free (BbDelayLine *line)
{
  g_free (line->buffer);
  g_free (line);
}

G_INLINE_FUNC double
bb_delay_line_add (BbDelayLine *line,
                   double       value)
{
  double rv = line->buffer[line->write_offset];
  line->buffer[line->write_offset] = value;
  if (++line->write_offset == line->n)
    line->write_offset = 0;
  return rv;
}

G_INLINE_FUNC gdouble
bb_delay_line_get_0   (BbDelayLine *line,
                       guint        t)
{
  g_assert (t < line->n);
  if (line->write_offset >= t + 1)
    return line->buffer[line->write_offset - (t + 1)];
  else
    return line->buffer[line->write_offset + line->n - (t + 1)];
}

G_INLINE_FUNC gdouble      bb_delay_line_get_lin (BbDelayLine *line,
                                                  gdouble      t)
{
  guint tfloor = (guint) (int) floor (t);
  gdouble tfrac = t - tfloor;
  guint t0i,t1i;
  g_assert (0 <= t && t < (double) line->n);
  if (line->write_offset >= tfloor + 1)
    t0i = line->write_offset - (tfloor + 1);
  else
    t0i = line->write_offset + line->n - (tfloor + 1);
  t1i = t0i ? (t0i - 1) : (line->n-1);
  return line->buffer[t0i] * (1.0 - tfrac)
       + line->buffer[t1i] * tfrac;
}

#if 0
G_INLINE_FUNC gdouble      bb_delay_line_get_sinc (BbDelayLine *line,
                                                   gdouble      t)
{
}
#endif

#endif

#endif
