#include "../core/bb-fft.h"

void run_test ()
{
  guint a_len = g_random_int_range (3, 128);
  guint b_len = g_random_int_range (3, 128);
  gdouble *a_data = g_new (gdouble, a_len);
  gdouble *b_data = g_new (gdouble, b_len);
  gdouble *out_data = g_new (gdouble, a_len + b_len);
  gdouble *out2_data = g_new (gdouble, a_len + b_len);
  guint i,j;
  g_message ("run_test: a_len=%u, b_len=%u",a_len,b_len);
  for (i = 0; i < a_len; i++)
    a_data[i] = g_random_double ();
  for (i = 0; i < b_len; i++)
    b_data[i] = g_random_double ();
  bb_convolve (a_len, a_data, b_len, b_data, out_data);
  for (i = 0; i < a_len + b_len - 1; i++)
    {
      gint a_start = 0;
      gint count = a_len;
      gint b_start = i;
      gdouble outval = 0;
      if (b_start >= (gint) b_len)
        {
          guint remove = b_start - b_len + 1;
          b_start -= remove;
          a_start += remove;
          count -= remove;
        }
      if (b_start + 1 < count)
        {
          count = b_start + 1;
        }
      g_assert (count > 0);
      for (j = 0; j < (guint) count; j++)
        outval += a_data[a_start + j] * b_data[b_start - j];
      out2_data[i] = outval;
    }
  out2_data[i] = 0;

  for (i = 0; i < a_len + b_len; i++)
    {
      gdouble ratio;
#define IS_VERY_CLOSE_TO_ZERO(a)  (ABS(a) < 1e-7)
#define SAFE_RATIO(a,b) ((IS_VERY_CLOSE_TO_ZERO(a)&&IS_VERY_CLOSE_TO_ZERO(b)) ? 1.0 : ((a)/(b)))
      ratio = SAFE_RATIO (out_data[i], out2_data[i]);
      if (!(0.99999 <= ratio && ratio <= 1.00001))
        {
          g_warning ("mismatch on sample %u", i);
          for (i = 0; i < a_len + b_len; i++)
            {
              ratio = SAFE_RATIO (out_data[i], out2_data[i]);
              g_message ("sample %4u: fft=%.6f; brute-force=%.6f (ratio=%.6f)", i, out_data[i], out2_data[i], ratio);
            }
          g_error ("TEST FAILED");
        }
    }
  g_free (a_data);
  g_free (b_data);
  g_free (out_data);
  g_free (out2_data);
}

int main ()
{
  guint i;
  for (i = 0; i < 50; i++)
    run_test ();
  return 0;
}
