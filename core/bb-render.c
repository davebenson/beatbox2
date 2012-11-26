#include <math.h>
#include "bb-render.h"
#include <stdlib.h>

static int compare_doubles (gconstpointer a, gconstpointer b)
{
  const gdouble *da = a;
  const gdouble *db = b;
  double A = *da;
  double B = *db;
  return (A<B) ? -1 : (A>B) ? 1 : 0;
}
  

/* NOTE: this corrupts the 'out' array: it must be copied
   to save it. */
void
bb_render_amplitude_window (guint8      *out,
                            guint        width,
                            guint        n_samples,
                            gdouble     *samples,
                            guint        window_size)
{
  guint i;
  gdouble half_width = 0.5 * width;
  guint used;
  qsort (samples, n_samples, sizeof (gdouble), compare_doubles);
  for (i = 0; i < n_samples; i++)
    samples[i] *= half_width;
  used = 0;
  for (i = 0; i < width / 2; i++)
    {
      guint brightness;
      while (used < n_samples
         && samples[used] < (gdouble)i - half_width)
        used++;
      brightness = 255 * used / window_size;
      *out++ = brightness;
      *out++ = brightness;
      if (brightness == 0)
        *out++ = 0;
      else
        *out++ = 255;
    }
  out -= (width / 2) * 3;
  out += width * 3;
  used = 0;
  for (i = 0; i < width / 2; i++)
    {
      guint brightness;
      while (used < n_samples
         && samples[n_samples-1-used] > (gdouble)half_width - i)
        used++;
      brightness = 255 * used / window_size;
      if (brightness == 0)
        *(--out) = 0;
      else
        *(--out) = 255;
      *(--out) = brightness;
      *(--out) = brightness;
    }
  if (width % 2 == 1)
    {
      guint brightness = n_samples * 255 / window_size;
      if (brightness == 0)
        *(--out) = 0;
      else
        *(--out) = 255;
      *(--out) = brightness;
      *(--out) = brightness;
    }
}

void bb_render_fft_window  (guint          window_size,
                            BbComplex     *freq_domain_data,
                            gdouble        sampling_rate,
                            gdouble        log_min_freq,
                            gdouble        delta_log_freq,
                            guint          width,
                            guint8        *out)
{
  gdouble *vals = g_new (gdouble, window_size);
  guint i;
  gdouble factor = 300.0         /* uh */
                 / window_size;


  for (i = 0; i < window_size; i++)
    vals[i] = hypot (freq_domain_data[i].im, freq_domain_data[i].re);

  /* compute the intensity pixel by pixel */
  for (i = 0; i < width; i++)
    {
      gdouble log_freq = (log_min_freq + delta_log_freq * (double)i / width);
      gdouble freq = exp (log_freq);
      gdouble sfreq = freq / sampling_rate;
      gdouble index = (double)window_size * sfreq;
      guint index_i;
      gdouble index_f;
      gdouble val;
      guint8 color;

      /* triggering these assertions means that
         the window_size or sampling-rate is too small
         for the frequency range */
      g_assert (index >= 1.0);
      g_assert (index < (gdouble)(window_size - 1));

      index_i = (gint) floor (index);
      index_f = index - (gdouble) index_i;
      val = (1.0 - index_f) * vals[index_i]
          + index_f * vals[index_i + 1];
      val *= factor;
      val = log (1.0 + val) * 0.4;
      if (val >= 1.0)
        color = 255;
      else
        color = (int) floor (255 * val);

      *out++ = color;
      *out++ = color;
      *out++ = color;
    }
  g_free (vals);
}
