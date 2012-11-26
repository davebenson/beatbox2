#include "instrument-includes.h"
#include "random_envelope.h"
#include <stdlib.h>

enum
{
  PARAM_MIN_VALUE,
  PARAM_MAX_VALUE,
  PARAM_N_TIMES,
  PARAM_N_TIMES_JITTER,
  PARAM_TIME_JITTER,
  PARAM_DURATION,
  PARAM_MODE
};

static BbInstrumentSimpleParam random_envelope_params[] =
{
  { PARAM_MIN_VALUE,           "min_value",        0.0 },
  { PARAM_MAX_VALUE,           "max_value",        1.0 },
  { PARAM_N_TIMES,             "n_times",          10.0 },
  { PARAM_N_TIMES_JITTER,      "n_times_jitter",   0.0 },
  { PARAM_TIME_JITTER,         "time_jitter",      0.0 },       /* 0..1 */
};

typedef struct _TVPair TVPair;
struct _TVPair
{
  guint sample;
  gdouble time;
  gdouble value;
  gdouble value2;
};
static int
compare_tv_pairs (gconstpointer a, gconstpointer b)
{
  const TVPair *ta = a;
  const TVPair *tb = b;
  return (ta->time < tb->time) ? -1
       : (ta->time > tb->time) ? 1
       : 0;
}

static void
interp_CONSTANT (guint   n_pairs,
                 TVPair *pairs,
                 guint   index,
                 guint   n_output,
                 double *output,
                 gdouble time,
                 gdouble time_step)
{
  gdouble value = pairs[index].value;
  while (n_output--)
    *output = value;
}

static void
interp_LINEAR   (guint   n_pairs,
                 TVPair *pairs,
                 guint   index,
                 guint   n_output,
                 double *output,
                 gdouble time,
                 gdouble time_step)
{
  TVPair *p = pairs + index;
  gdouble slope = (p[1].value - p[0].value) / (p[1].time - p[0].time);
  gdouble init_value = (time - p[0].time) * slope + p[0].value;
  gdouble delta_value = time_step * slope;
  guint i;
  for (i = 0; i < n_output; i++)
    output[i] = init_value + delta_value * i;
}

static void
interp_LINE_SEGMENTS  (guint   n_pairs,
                       TVPair *pairs,
                       guint   index,
                       guint   n_output,
                       double *output,
                       gdouble time,
                       gdouble time_step)
{
  TVPair *p = pairs + index;
  gdouble slope = (p[1].value - p[0].value2) / (p[1].time - p[0].time);
  gdouble init_value = (time - p[0].time) * slope + p[0].value2;
  gdouble delta_value = time_step * slope;
  guint i;
  for (i = 0; i < n_output; i++)
    output[i] = init_value + delta_value * i;
}

typedef void (*InterpFunc) (guint   n_pairs,
                            TVPair *pairs,
                            guint   index,
                            guint   n_output,
                            double *output,
                            gdouble time,
                            gdouble time_step);

static const struct {
  BbRandomEnvelopeMode mode;
  InterpFunc interpolator;
  gboolean needs_value2;
} mode_infos[] =
{
  {
    BB_RANDOM_ENVELOPE_CONSTANT,
    interp_CONSTANT,
    FALSE
  },
  {
    BB_RANDOM_ENVELOPE_LINEAR,
    interp_LINEAR,
    FALSE
  },
  {
    BB_RANDOM_ENVELOPE_LINE_SEGMENTS,
    interp_LINE_SEGMENTS,
    TRUE
  }
};


static double *
random_envelope_synth  (BbInstrument *instrument,
                        BbRenderInfo *render_info,
                        GValue       *params,
                        guint        *n_samples_out)
{
  double max_value       = g_value_get_double (params + PARAM_MAX_VALUE);
  double min_value       = g_value_get_double (params + PARAM_MIN_VALUE);
  double n_times         = g_value_get_double (params + PARAM_N_TIMES);
  double n_times_jitter  = g_value_get_double (params + PARAM_N_TIMES_JITTER);
  double time_jitter_2   = g_value_get_double (params + PARAM_TIME_JITTER) * 0.5;
  int actual_n_times = (int) floor (n_times + g_random_double_range (-n_times_jitter, n_times_jitter) + 1.5);
  BbRandomEnvelopeMode mode = g_value_get_enum (params + PARAM_MODE);
  guint n = render_info->note_duration_samples;
  gdouble time_step = 1.0 / n;
  guint i;
  guint mode_index;
  double interval_len;
  double *rv = g_new (double, n);
  guint last_end = 0;
  TVPair *pairs;
  for (mode_index = 0; mode_index < G_N_ELEMENTS (mode_infos); mode_index++)
    if (mode_infos[mode_index].mode == mode)
      break;
  g_assert (mode_index != G_N_ELEMENTS (mode_infos));
  if (actual_n_times < 2)
    actual_n_times = 2;
  interval_len = 1.0 / actual_n_times;
  time_jitter_2 *= interval_len;
  pairs = g_new (TVPair, actual_n_times);
  for (i = 0; i < (guint) actual_n_times; i++)
    {
      pairs[i].time = (double)i * interval_len
                    + g_random_double_range (-time_jitter_2, time_jitter_2);;
      pairs[i].value = g_random_double_range (min_value, max_value);
      if (mode_infos[mode_index].needs_value2)
        pairs[i].value2 = g_random_double_range (min_value, max_value);
    }
  *n_samples_out = n;

  /* sort pairs */
  qsort (pairs, actual_n_times, sizeof (TVPair), compare_tv_pairs);

  for (i = 0; i < (guint) actual_n_times; i++)
    pairs[i].sample = pairs[i].time * n;

  /* handle endpoints */
  if (pairs[0].sample > 0)
    {
      if (pairs[0].sample >= n)
        {
          /* constant the whole way */
          for (i = 0; i < n; i++)
            rv[i] = pairs[0].value;
          return rv;
        }
      for (i = 0; i < pairs[0].sample; i++)
        rv[i] = pairs[0].value;
      last_end = i;
    }
  for (i = 0; i + 1 < (guint) actual_n_times; i++)
    {
      int range_start = ceil (pairs[i+0].time * n);
      int range_end = floor (pairs[i+1].time * n) + 1;
      if (range_start < 0)
        range_start = 0;
      if (range_end > (int) n)
        range_end = (int) n;
      if (range_start > range_end)
        continue;
      if (range_start == range_end)
        {
          if (range_start == (int) n)
            continue;
          rv[range_start] = pairs[i+0].value;
        }
      else
        {
          mode_infos[mode_index].interpolator (actual_n_times, pairs, i,
                                               range_end - range_start, rv + range_start,
                                               range_start * time_step, time_step);
        }
      last_end = range_end;
    }
  for (i = last_end; i < n; i++)
    rv[i] = pairs[actual_n_times-1].value;
  g_free (pairs);

  return rv;
}

BB_INIT_DEFINE(_bb_random_envelope_init)
{
  BbInstrument *instrument;
  instrument = bb_instrument_new_from_simple_param (G_N_ELEMENTS (random_envelope_params),
                                                    random_envelope_params);
  bb_instrument_add_param (instrument, PARAM_DURATION, param_spec_duration ());
  bb_instrument_add_param (instrument,
                           PARAM_MODE,
                           g_param_spec_enum ("mode", "Mode", NULL,
                                              BB_TYPE_RANDOM_ENVELOPE_MODE, BB_RANDOM_ENVELOPE_LINEAR, 0));
  instrument->synth_func = random_envelope_synth;
  bb_instrument_set_name (instrument, "random_envelope");
}
