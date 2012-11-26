#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "../core/bb-script.h"
#include "../core/bb-render.h"
#include "../core/bb-init.h"
#include "../core/bb-fft.h"
#include "bb-font.h"

void bb_vm_init ();

#define MARGIN 4
#define PART_MARGIN     20              /* vertical space demanded between parts */

typedef struct _AnalyzeInfo AnalyzeInfo;
struct _AnalyzeInfo
{
  guint width, height;
  guint8 *img_data;

  guint sampling_rate;

  guint start_top;
  guint step_size, window_size;

  guint fft_graph_width;
  guint amplitude_graph_width;
  guint part_fft_graph_width;
  guint part_amplitude_graph_width;

  guint beat_text_width;
  guint beat_text_right;
  guint sec_text_width;
  guint amplitude_left;
  guint amplitude_right;
  guint fft_left;
  guint fft_right;
  guint sec_text_left;
  guint part_0_left;
  guint part_width;

  guint inset_part_amplitude_left;
  guint inset_part_amplitude_right;
  guint inset_part_fft_left;
  guint inset_part_fft_right;

  gdouble log_min_freq, log_max_freq, delta_log_freq;

  gdouble norm_factor;
  gdouble norm_offset;
};

static guint
sample_to_y (AnalyzeInfo *info,
             guint        t)
{
  return (t / info->step_size) + info->start_top;
}

/* returns the first sample for the given y.
   for the fft, this is the first of window_size
   samples that are at that y.  For amplitude,
   it's the first of step_size samples. */
static guint
y_to_sample (AnalyzeInfo *info,
             guint        y)
{
  g_assert (y >= info->start_top);
  return (y - info->start_top) * info->step_size;
}

static guint8 *
get_image_pointer (AnalyzeInfo *info,
                   guint        x,
                   guint        y)
{
  return info->img_data + (info->width * y + x) * 3;
}

static gdouble
remap_sample_value (AnalyzeInfo *info,
                    gdouble      v)
{
  return v * info->norm_factor + info->norm_offset;
}

static void
draw_amplitude_streak (AnalyzeInfo *info,
                       const char  *data_filename,
                       guint        start_sample,
                       guint        n_samples,
                       guint        left,
                       guint        width)
{
  guint y,s;
  guint i;
  gdouble *buf = g_new (gdouble, info->step_size);
  guint start_inset, init, remaining;
  FILE *fp = fopen (data_filename, "rb");
  if (fp == NULL)
    g_error ("error opening %s: %s", data_filename, g_strerror (errno));
  y = sample_to_y (info, start_sample);
  s = y_to_sample (info, y);

  if (s > start_sample)
    g_message ("start_sample=%u, y=%u, s=%u; step_size=%u, start_top=%u",
               start_sample,y,s,info->step_size,info->start_top);

  /* handle the first, potentially partially filled pixel */
  g_assert (s <= start_sample);
  start_inset = start_sample - s;
  g_assert (start_inset < info->step_size);
  init = info->step_size - start_inset;
  if (init > n_samples)
    init = n_samples;
  if (fread (buf, sizeof (gdouble), init, fp) != init)
    g_error ("reading from %s", data_filename);
  for (i = 0; i < init; i++)
    buf[i] = remap_sample_value (info, buf[i]);
  bb_render_amplitude_window (get_image_pointer (info, left, y), width, init, buf, info->step_size);
  y++;

  remaining = n_samples - init;
  while (remaining >= info->step_size)
    {
      if (fread (buf, sizeof (gdouble), info->step_size, fp) != info->step_size)
        g_error ("reading from %s", data_filename);
      for (i = 0; i < info->step_size; i++)
        buf[i] = remap_sample_value (info, buf[i]);
      bb_render_amplitude_window (get_image_pointer (info, left, y), width, info->step_size, buf, info->step_size);
      y++;
      remaining -= info->step_size;
    }

  if (remaining > 0)
    {
      /* handle the last partially filled pixel, if any */
      if (fread (buf, sizeof (gdouble), remaining, fp) != remaining)
        g_error ("reading from %s", data_filename);
      for (i = 0; i < remaining; i++)
        buf[i] = remap_sample_value (info, buf[i]);
      bb_render_amplitude_window (get_image_pointer (info, left, y), width, remaining, buf, info->step_size);
    }
  fclose (fp);
  g_free (buf);
}

static void
safe_read_or_zero (FILE *fp, guint *zeroes_inout, guint *remaining_inout,
                   gdouble *buf, guint n)
{
  if (n == 0)
    return;
  if (*zeroes_inout >= n)
    {
      *zeroes_inout -= n;
      memset (buf, 0, sizeof(double)*n);
      return;
    }
  if (*zeroes_inout)
    {
      memset (buf, 0, sizeof (double) * *zeroes_inout);
      n -= *zeroes_inout;
      buf += *zeroes_inout;
      *zeroes_inout = 0;
    }

  if (*remaining_inout == 0)
    {
      memset (buf, 0, sizeof (double) * n);
    }
  else if (n > *remaining_inout)
    {
      if (fread (buf, sizeof (gdouble), *remaining_inout, fp) != *remaining_inout)
        g_error ("safe_read_or_zero failed");
      memset (buf + *remaining_inout, 0, sizeof (double) * (n - *remaining_inout));
      *remaining_inout = 0;
    }
  else
    {
      if (fread (buf, sizeof (gdouble), n, fp) != n)
        g_error ("safe_read_or_zero failed [2]");
      *remaining_inout -= n;
    }
}
static inline void
render_fft_line (AnalyzeInfo *info,
                 guint        width,
                 guint8      *out,
                 BbComplex   *freq_domain_data)
{
  bb_render_fft_window (info->window_size,
                        freq_domain_data,
                        info->sampling_rate,
                        info->log_min_freq,
                        info->delta_log_freq,
                        width, out);
}

static void
draw_fft_streak (AnalyzeInfo *info,
                 const char  *data_filename,
                 guint        start_sample,
                 guint        n_samples,
                 guint        left,
                 guint        width)
{
  FILE *fp = fopen (data_filename, "rb");
  gdouble *read_buf = g_new0 (gdouble, info->window_size);
  BbComplex *fft_buf = g_new (BbComplex, info->window_size);
  guint read_buf_at = 0;
  guint y,end_y,remaining,i,y_at;
  gint ss;
  gdouble *window = g_new (gdouble, info->window_size);
  guint zeroes = 0;
  g_assert (fp);
  y = sample_to_y (info, start_sample);
  end_y = sample_to_y (info, start_sample + n_samples);
  ss = y_to_sample (info, y)
     - info->window_size / 2
     + info->step_size / 2;

  remaining = n_samples;
  if (ss < (gint)start_sample)
    zeroes = start_sample - ss;
  else if (ss > (gint)start_sample)
    {
      gdouble tmp;
      guint skip = ss - start_sample;
      if (skip > n_samples)
        skip = n_samples;
      while (skip--)
        {
          if (fread (&tmp,sizeof(double),1,fp) != 1)
            g_error ("error skipping data");
          remaining--;
        }
    }

  if (zeroes >= info->window_size)
    {
      memset (read_buf, 0, sizeof (gdouble) * info->window_size);
      zeroes -= info->window_size;
    }
  else
    {
      guint rem = info->window_size - zeroes;
      guint to_read;
      memset (read_buf, 0, sizeof (gdouble) * zeroes);
      read_buf_at = zeroes;
      zeroes = 0;
      to_read = MIN (rem, remaining);
      if (to_read > 0)
        {
          if (fread (read_buf + read_buf_at, sizeof (gdouble), to_read, fp) != to_read)
            g_error ("error reading first buf");
          read_buf_at += to_read;
          remaining -= to_read;
        }
      if (read_buf_at < info->window_size)
        memset (read_buf + read_buf_at, 
                0, sizeof(double) * (info->window_size - read_buf_at));
    }
  read_buf_at = 0;

  /* TODO: make configurable */
  bb_fft_make_window (BB_FFT_WINDOW_HAMMING, 0.0,
                      info->window_size, window);

  for (y_at = y; y_at <= end_y; y_at++)
    {
      /* do fft */
      guint ws = info->window_size;
      for (i = 0; i < ws; i++)
        {
          fft_buf[i].re = read_buf[(read_buf_at + i) % ws] * info->norm_factor
                        + info->norm_offset;
          fft_buf[i].re *= window[i];
          fft_buf[i].im = 0;
        }
      bb_fft (info->window_size, FALSE, fft_buf);
      render_fft_line (info, width, get_image_pointer (info, left, y_at), fft_buf);

      /* read more data in */
      if (read_buf_at + info->step_size >= info->window_size)
        {
          safe_read_or_zero (fp, &zeroes, &remaining, read_buf + read_buf_at,
                             info->window_size - read_buf_at);
          safe_read_or_zero (fp, &zeroes, &remaining, read_buf,
                             info->step_size - (info->window_size - read_buf_at));
          read_buf_at += info->step_size;
          read_buf_at -= info->window_size;
        }
      else
        {
          safe_read_or_zero (fp, &zeroes, &remaining, read_buf + read_buf_at,
                             info->step_size);
          read_buf_at += info->step_size;
        }
    }
  fclose (fp);
  g_free (read_buf);
  g_free (fft_buf);
  g_free (window);
}


static void
usage (const char *prog)
{
  g_printerr ("usage: %s [OPTIONS] SCRIPT\n\n", prog);
  exit (1);
}

int main(int argc, char **argv)
{
  const char *script = NULL;
  guint i;
  GError *error = NULL;
  BbScore *score;
  AnalyzeInfo info;
  BbScoreRenderConfig *config;
  const char *output_fname = NULL;
  char *dir;
  BbScoreRenderResult *ri;
  gdouble score_max_sec;
  bb_init ();
  bb_vm_init ();
  config = bb_score_render_config_new ();
  info.step_size = 4096;
  info.window_size = 4096;              /* must be a power of two */
  for (i = 1; i < (guint) argc; i++)
    {
      if (argv[i][0] == '-')
	{
          if (strcmp (argv[i], "--g-fatal-warnings") == 0)
            {
              g_log_set_always_fatal (G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL);
            }
          else if (strcmp (argv[i], "-o") == 0)
            {
              output_fname = argv[++i];
            }
          else if (g_str_has_prefix (argv[i], "--step-size="))
            {
              info.step_size = atoi (strchr (argv[i], '=') + 1);
              if (info.step_size==0)
                g_error ("error parsing step-size");
            }
          else if (g_str_has_prefix (argv[i], "--window-size="))
            {
              info.window_size = atoi (strchr (argv[i], '=') + 1);
              if (info.window_size==0)
                g_error ("error parsing window-size");
              if ((info.window_size&(info.window_size-1))!=0)
                g_error ("window size %u is not a power of two", info.window_size);
            }
          else
            {
              g_warning ("unknown argument %s", argv[i]);
              usage ("bb-run");
              return 1;
            }
	}
      else if (script != NULL)
        g_error ("more than one script filename provided?");
      else
        script = argv[i];
    }
  if (script == NULL)
    usage ("bb-analyze");
  if (output_fname == NULL)
    g_error ("missing -o FILENAME argument");
  score = bb_script_execute (script, config, &error);
  if (score == NULL)
    g_error ("error running %s: %s", script, error->message);

  /* find the widest part name */
  guint max_part_name_len;
  max_part_name_len = 0;
  for (i = 0; i < score->n_parts; i++)
    {
      BbScorePart *p = score->parts[i];
      const char *part_name;
      char buf[128];
      guint part_name_len;
      if (p->name == NULL)
        {
          g_warning ("part %u is unnamed", i);
          g_snprintf(buf,sizeof(buf),"%u",i);
          part_name = buf;
        }
      else
        {
          part_name = p->name;
        }
      part_name_len = bb_font_get_text_width (part_name);
      if (part_name_len > max_part_name_len)
        max_part_name_len = part_name_len;
    }


  info.fft_graph_width = 300;
  info.amplitude_graph_width = 100;
  info.part_fft_graph_width = 200;
  info.part_amplitude_graph_width = 50;
  info.beat_text_width = bb_font_get_text_width ("9999b");
  info.beat_text_right = MARGIN + info.beat_text_width;
  info.amplitude_left = info.beat_text_right + MARGIN;
  info.amplitude_right = info.amplitude_left + info.amplitude_graph_width;
  info.fft_left = info.amplitude_right + MARGIN;
  info.fft_right = info.fft_left + info.fft_graph_width;
  info.sec_text_left = info.fft_right + MARGIN;
  info.sec_text_width = bb_font_get_text_width ("999s");
  info.part_0_left = info.sec_text_left + info.sec_text_width + MARGIN;
  info.start_top = bb_font_get_height () + 10;
  info.sampling_rate = bb_score_render_config_get_sampling_rate (config);

  /* and the width of an individual part */
  info.part_width = MAX (max_part_name_len, 
                         info.part_amplitude_graph_width + MARGIN
                         + info.part_fft_graph_width + MARGIN);


  dir = g_strdup_printf ("analyze-%u-%u",
                         (guint) getpid (),
                         (guint) time(NULL));
  ri = bb_score_render_for_analysis (score, config,
                                     dir, TRUE, &error);
  if (ri == NULL)
    g_error ("error invoking render_for_analysis: %s", error->message);

  /* allocate space for parts */
  guint n_columns = 0;
  guint *columns = g_new (guint, score->n_parts);
  gdouble score_max_beat = 0;
  for (i = 0; i < score->n_parts; i++)
    {
      BbScorePart *part = score->parts[i];
      guint c;
      score_max_beat = MAX (part->max_beat, score_max_beat);
      for (c = 0; c < n_columns; c++)
        {
          guint j;
          for (j = 0; j < i; j++)
            if (columns[j] == c)
              {
                BbScoreRenderResultPart *p1 = ri->parts + i;
                BbScoreRenderResultPart *p2 = ri->parts + j;
                guint start_y_1 = sample_to_y (&info, p1->start_sample);
                guint start_y_2 = sample_to_y (&info, p2->start_sample);
                guint end_y_1 = sample_to_y (&info, p1->start_sample + p1->n_samples);
                guint end_y_2 = sample_to_y (&info, p2->start_sample + p2->n_samples);
                if (start_y_1 <= end_y_2 + PART_MARGIN
                 && start_y_2 <= end_y_1 + PART_MARGIN)
                  {
                    g_message ("part %s[%u,%u] intersects with %s[%u,%u] so column %u is not viable",
                               score->parts[i]->name,start_y_1,end_y_1,
                               score->parts[j]->name,start_y_2,end_y_2,
                               c);
                    break;   /* they intersect, so column 'c' is not viable */
                  }
              }

          /* if no intersections encountered, 'c' is the correct column,
             so stop iterating. */
          if (j == i)
            break;
        }

      if (c == n_columns)
        {
          /* allocate new column */
          n_columns++;
        }
      g_message ("part %s allocated to column %u", part->name,c);
      columns[i] = c;
    }
  info.height = 0;
  for (i = 0; i < score->n_parts; i++)
    {
      guint y = sample_to_y (&info, ri->parts[i].start_sample + ri->parts[i].n_samples);
      g_message ("part %u: end-y=%u",i,y);
      if (y > info.height)
        info.height = y;
    }

  info.height += bb_font_get_height ();
  info.width = info.part_0_left + info.part_width * n_columns;
  info.img_data = g_malloc (info.width * info.height * 3);
  memset (info.img_data, 0x55, info.width * info.height * 3);

  /* three octaves from middle a */
  info.log_min_freq = log (440.0 / 8);
  info.log_max_freq = log (440.0 * 8);
  info.delta_log_freq = info.log_max_freq - info.log_min_freq;

  info.norm_factor = 1.0 / MAX (ABS (ri->min_value), ABS (ri->max_value));
  info.norm_offset = 0.0;

  g_message ("image will be %u x %u", info.width, info.height);

  /* draw labels */
  bb_font_draw_text (get_image_pointer (&info, 1, 1),
                     info.width * 3, (guint8*)"\377\377\277",
                     script);
  for (i = 0; i < score->n_parts; i++)
    {
      guint y = sample_to_y (&info, ri->parts[i].start_sample);
      guint y_top = y - 1 - bb_font_get_height ();
      guint x = info.part_0_left + info.part_width * columns[i];
      bb_font_draw_text (get_image_pointer (&info, x, y_top),
                         info.width * 3, (guint8 *) "\377\366\20",
                         score->parts[i]->name);
    }

  /* draw main ffts and amplitudes */
  draw_amplitude_streak (&info, ri->filename,
                         ri->start_sample, ri->n_samples,
                         info.amplitude_left, info.amplitude_graph_width);
  draw_fft_streak (&info, ri->filename,
                   ri->start_sample, ri->n_samples,
                   info.fft_left, info.fft_graph_width);

  guint last_label_y;
  last_label_y = info.start_top;
  for (i = 0; i < score_max_beat; i++)
    {
      gdouble sec = bb_score_get_time_from_beat (score, i);
      gdouble samp = sec * info.sampling_rate;
      guint y = sample_to_y (&info, samp);
      if (last_label_y + bb_font_get_height () + 2 < y
       && y < info.height - bb_font_get_descent ())
        {
          char buf[16];
          guint w;
          g_snprintf (buf, sizeof (buf), "%ub", i);
          w = bb_font_get_text_width (buf);
          bb_font_draw_text (get_image_pointer (&info, info.amplitude_left-1-w,
                                                y - bb_font_get_ascent ()),
                             info.width * 3, (guint8*)"\277\277\277", buf);
          last_label_y = y;
        }

      if (y < info.height)
        memcpy (get_image_pointer (&info, info.amplitude_left - 2, y),
                "\0\377\0\0\377\0",
                6);
    }
  score_max_sec = bb_score_get_time_from_beat (score, score_max_beat);
  last_label_y = info.start_top;
  for (i = 0; i < score_max_sec; i++)
    {
      gdouble samp = i * info.sampling_rate;
      guint y = sample_to_y (&info, samp);
      if (y < info.height)
        memcpy (get_image_pointer (&info, info.fft_right, y),
                "\0\377\377\0\377\377",
                6);
      if (last_label_y + bb_font_get_height () + 2 < y
       && y < info.height - bb_font_get_descent ())
        {
          char buf[16];
          guint w;
          g_snprintf (buf, sizeof (buf), "%us", i);
          w = bb_font_get_text_width (buf);
          bb_font_draw_text (get_image_pointer (&info, info.fft_right+3,
                                                y - bb_font_get_ascent ()),
                             info.width * 3, (guint8*)"\277\277\277", buf);
          last_label_y = y;
        }

    }

  /* draw part ffts and amplitudes */
  for (i = 0; i < score->n_parts; i++)
    {
      guint left = info.part_0_left + info.part_width * columns[i];
      guint fft_left = left + info.part_amplitude_graph_width + MARGIN;
      gdouble min,max;
      guint j;
      draw_amplitude_streak (&info, ri->parts[i].filename,
                             ri->parts[i].start_sample, ri->parts[i].n_samples,
                             left, info.part_amplitude_graph_width);
      draw_fft_streak (&info, ri->parts[i].filename,
                       ri->parts[i].start_sample, ri->parts[i].n_samples,
                       fft_left, info.part_fft_graph_width);

      min = ri->parts[i].start_sample / info.sampling_rate;
      max = (ri->parts[i].start_sample+ri->parts[i].n_samples) / info.sampling_rate;
      for (j = ceil (min); j < max; j++)
        {
          guint samp = info.sampling_rate * j;
          guint y = sample_to_y (&info, samp);
          gdouble fft_right = fft_left + info.part_fft_graph_width;
          if (y < info.height)
            memcpy (get_image_pointer (&info, fft_right, y),
                   "\0\377\377\0\377\377",
                   6);
        }
      min = bb_score_get_beat_from_time (score, min);
      max = bb_score_get_beat_from_time (score, max);
      for (j = ceil (min); j < max; j++)
        {
          gdouble sec = bb_score_get_time_from_beat (score, j);
          guint samp = info.sampling_rate * sec;
          guint y = sample_to_y (&info, samp);
          if (y < info.height)
            memcpy (get_image_pointer (&info, left - 2, y),
                   "\0\377\0\0\377\0",
                   6);
        }
    }

  {
    FILE *fp = fopen (output_fname, "wb");
    if (fp == NULL)
      g_error ("error creating %s: %s", output_fname, g_strerror (errno));
    fprintf (fp, "P6\n%u %u\n255\n", info.width, info.height);
    if (fwrite (info.img_data, info.width * info.height * 3, 1, fp) != 1)
      g_error ("error writing img data");
    fclose (fp);
  }

  g_free (dir);
  bb_score_render_config_free (config);
  bb_score_render_result_free (ri, TRUE);
  return 0;
}
