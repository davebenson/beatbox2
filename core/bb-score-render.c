#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include "bb-score.h"
#include "bb-error.h"
#include "bb-gnuplot.h"
#include "bb-parse.h"
#include "bb-utils.h"
#include "bb-duration.h"
#include "wav-header.h"
#include <gsk/gskrbtreemacros.h>

/* whether to normalize the waveform insisting that max == -min */
#define SYMMETRIC_NORMALIZATION         1

/* --- render config --- */
struct _BbScoreRenderConfig
{
  gdouble sampling_rate;
  GArray *note_filters;
  gboolean has_start_time;
  gdouble start_time;
  gboolean has_end_time;
  gdouble end_time;
};

typedef struct _NoteFilter NoteFilter;
struct _NoteFilter
{
  BbScoreNoteFilterFunc func;
  gpointer              data;
  GDestroyNotify        destroy;
};

BbScoreRenderConfig *
bb_score_render_config_new        (void)
{
  BbScoreRenderConfig *rv = g_new (BbScoreRenderConfig, 1);
  rv->sampling_rate = 44100;
  rv->note_filters = g_array_new (FALSE, FALSE, sizeof (NoteFilter));
  rv->has_start_time = TRUE;
  rv->start_time = 0;
  rv->has_end_time = FALSE;
  rv->end_time = 0;
  return rv;
}

void
bb_score_render_config_free            (BbScoreRenderConfig  *config)
{
  NoteFilter *filter_at = (NoteFilter *) (config->note_filters->data);
  guint i;
  for (i = 0; i < config->note_filters->len; i++, filter_at++)
    if (filter_at[i].destroy)
      filter_at[i].destroy (filter_at[i].data);
  g_array_free (config->note_filters, TRUE);
  g_free (config);
}

void  bb_score_render_config_add_filter      (BbScoreRenderConfig  *config,
                                              BbScoreNoteFilterFunc func,
                                              gpointer              data,
                                              GDestroyNotify        destroy)
{
  NoteFilter dummy = {func,data,destroy};
  g_array_append_val (config->note_filters, dummy);
}

void  bb_score_render_config_set_sampling_rate(BbScoreRenderConfig  *config,
                                              gdouble               sampling_rate)
{
  config->sampling_rate = sampling_rate;
}

gdouble bb_score_render_config_get_sampling_rate (BbScoreRenderConfig *config)
{
  return config->sampling_rate;
}


void  bb_score_render_config_set_start_time  (BbScoreRenderConfig  *config,
                                              gdouble               start_secs)
{
  config->has_start_time = TRUE;
  config->start_time = start_secs;
}
void  bb_score_render_config_set_end_time    (BbScoreRenderConfig  *config,
                                              gdouble               end_secs)
{
  config->has_end_time = TRUE;
  config->end_time = end_secs;
}
void  bb_score_render_config_unset_start_time(BbScoreRenderConfig  *config)
{
  config->has_start_time = FALSE;
}
void  bb_score_render_config_unset_end_time  (BbScoreRenderConfig  *config)
{
  config->has_end_time = FALSE;
}

static gboolean
score_config_check_note (BbScoreRenderConfig *config,
                         BbScorePart         *part,
                         guint                note_index)
{
  guint i;
  for (i = 0; i < config->note_filters->len; i++)
    {
      NoteFilter *f = &g_array_index (config->note_filters, NoteFilter, i);
      switch (f->func (part, note_index, f->data))
        {
        case BB_SCORE_NOTE_FILTER_REJECT:
          return FALSE;
        case BB_SCORE_NOTE_FILTER_ACCEPT:
          return TRUE;
        default:
          /* next filter */
          break;
        }
    }
  return TRUE;          /* accept by default */
}

/* --- rendering --- */
typedef struct _NoteRef NoteRef;
struct _NoteRef
{
  guint part_index, note_index;
  guint start_sample;
  gdouble start_time;
};

typedef struct _Buffer Buffer;
struct _Buffer
{
  guint start_sample;
  guint n_samples;
  double *data;
  BbScorePart *part;
  guint note_index;

  Buffer *left, *right, *parent;
  gboolean is_red;
};

#define COMPARE_BUFFER_END_TIME(a,b,rv)               \
G_STMT_START{                                         \
    guint ea = a->start_sample + a->n_samples;        \
    guint eb = b->start_sample + b->n_samples;        \
    if (ea < eb)                                      \
      rv = -1;                                        \
    else if (ea > eb)                                 \
      rv = 1;                                         \
    else                                              \
      rv = ((a < b) ? -1 : (a > b) ? 1 : 0);          \
}G_STMT_END
#define IS_RED(n)       n->is_red
#define SET_IS_RED(n,v) n->is_red = v

#define GET_BUFFER_TREE(top)  top, Buffer*, IS_RED, SET_IS_RED, \
                              parent, left, right, \
                              COMPARE_BUFFER_END_TIME

static void
add_samples (gdouble *out,
             const gdouble *in,
             guint N)
{
  while (N--)
    *out++ += *in++;
}

static void
add_samples_recursive (double *out,
                       Buffer *buf,
                       guint   start,
                       guint   n)
{
  const double *in;
  register guint i;
  if (buf == NULL)
    return;
  in = buf->data + (start - buf->start_sample);
  for (i = 0; i < n; i++)
    out[i] += in[i];
  add_samples_recursive (out, buf->left, start, n);
  add_samples_recursive (out, buf->right, start, n);
}

static int
compare_note_ref_by_start_time (gconstpointer a,
                                gconstpointer b)
{
  const NoteRef *na = a;
  const NoteRef *nb = b;
  return (na->start_sample < nb->start_sample) ? -1
       : (na->start_sample > nb->start_sample) ? 1
       : 0;
}

static void
free_tree_data_recursive (Buffer *buf)
{
  if (buf == NULL)
    return;
  g_free (buf->data);
  free_tree_data_recursive (buf->left);
  free_tree_data_recursive (buf->right);
}


static void
prepare_note_refs (BbScore            *score,
                   BbScoreRenderConfig *config,
                   guint               *n_notes_out,
                   NoteRef            **notes_out)
{
  guint max_notes = 0;
  NoteRef *notes = NULL;
  guint i, j, note_at = 0;

  for (i = 0; i < score->n_parts; i++)
    max_notes += score->parts[i]->n_notes;
  notes = g_new (NoteRef, max_notes);

  for (i = 0; i < score->n_parts; i++)
    for (j = 0; j < score->parts[i]->n_notes; j++)
      if (score_config_check_note (config, score->parts[i], j))
        {
          double t = bb_score_get_time_from_beat (score, score->parts[i]->notes[j].start);
          notes[note_at].part_index = i;
          notes[note_at].note_index = j;
          notes[note_at].start_time = t;
          notes[note_at].start_sample = (gint) (t * config->sampling_rate);
          note_at++;
        }

  qsort (notes, note_at, sizeof (NoteRef), compare_note_ref_by_start_time);
  *n_notes_out = note_at;
  *notes_out = notes;
}

/* WARNING: caller must initialize start_sample
   and tree members. */
static void
create_note_buffer (BbScorePart      *part,
                    guint             note_index,
                    gdouble           sampling_rate,
                    Buffer           *out)
{
  BbRenderInfo render_info;
  BbScoreNote *note = part->notes + note_index;
  render_info.sampling_rate = sampling_rate;
  render_info.beat_period = bb_score_get_beat_period_from_beat (part->score, note->start);
  out->part = part;
  out->note_index = note_index;
  switch (note->type)
    {
    case BB_SCORE_NOTE_SIMPLE:
      if (part->instrument->duration_param >= 0)
        {
          BbDuration dur;
          bb_value_get_duration (note->values + part->instrument->duration_param, &dur);
          switch (dur.units)
            {
            case BB_DURATION_UNITS_SECONDS:
              render_info.note_duration_secs = dur.value;
              render_info.note_duration_samples = dur.value * render_info.sampling_rate;
              render_info.note_duration_beats = dur.value / render_info.beat_period;
              break;
            case BB_DURATION_UNITS_SAMPLES:
              render_info.note_duration_samples = dur.value;
              render_info.note_duration_secs = dur.value / render_info.sampling_rate;
              render_info.note_duration_beats = render_info.note_duration_secs / render_info.beat_period;
              break;
            case BB_DURATION_UNITS_BEATS:
              render_info.note_duration_beats = dur.value;
              render_info.note_duration_secs = dur.value * render_info.beat_period;
              render_info.note_duration_samples = render_info.note_duration_secs * render_info.sampling_rate;
              break;
            case BB_DURATION_UNITS_NOTE_LENGTHS:
              {
                g_error ("note duration cannot be defined in note-lengths: circular");
              }
            }
        }
      else
        {
          render_info.note_duration_beats = 0;
          render_info.note_duration_secs = 0;
          render_info.note_duration_samples = 0;
        }

      out->data = bb_instrument_synth (part->instrument,
                                       &render_info,
                                       note->values,
                                       &out->n_samples);
      break;
    case BB_SCORE_NOTE_COMPLEX:
      {
        render_info.note_duration_secs = note->events[note->n_events].time;
        render_info.note_duration_beats = render_info.note_duration_secs / render_info.beat_period;
        render_info.note_duration_samples = render_info.sampling_rate * render_info.note_duration_secs;
        out->data = bb_instrument_complex_synth (part->instrument,
                                                 &render_info,
                                                 note->events[note->n_events].time,
                                                 note->values, note->n_events, note->events,
                                                 &out->n_samples);
        break;
      }
    default: 
      g_assert_not_reached ();
    }
  if (out->n_samples == 0)
    g_warning ("note of length 0 encountered");
  if (note->volume != 1)
    bb_rescale (out->data, out->n_samples, note->volume);

  if (bb_score_gnuplot_notes)
    {
      char *fname = g_strdup_printf ("part%u-note%u.png", part->index, note_index);
      output_double_array_as_graph (out->n_samples,
                                    out->data,
                                    sampling_rate,
                                    fname);
      g_free (fname);
    }
}

gboolean
bb_score_render             (BbScore             *score,
                             BbScoreRenderConfig *config,
                             BbScoreRenderHandler handler,
                             gpointer             handler_data,
                             gboolean             print_status,
                             GError             **error)
{
  guint n_notes;
  NoteRef *notes;
  guint i;
  guint note_at, time_at;
  Buffer *buffers;
  Buffer *buffer_tree = NULL;
  GArray *doubles = g_array_new (FALSE, FALSE, sizeof (double));
  guint status_n_dots = 0;
  guint approx_n_samples;
  gdouble min_time;
  gdouble start_time;
  guint sample_offset;
  prepare_note_refs (score, config, &n_notes, &notes);
  if (n_notes == 0)
    {
      if (print_status)
        g_printerr ("no notes\n");
      return TRUE;
    }
  min_time = notes[0].start_time;
  if (!config->has_start_time)
    start_time = min_time;
  else
    start_time = config->start_time;
  g_assert (start_time >= 0);
  sample_offset = (guint) (start_time * config->sampling_rate);
  for (i = 0; i < n_notes; i++)
    {
      notes[i].start_time -= start_time;
      notes[i].start_sample = (gint) (notes[i].start_time * config->sampling_rate);
    }

  buffers = g_new (Buffer, n_notes);
  approx_n_samples = notes[n_notes-1].start_sample + config->sampling_rate;
  if (print_status)
    g_printerr ("%u notes, approx %.03fs: ",
                n_notes, (gdouble)approx_n_samples / config->sampling_rate);

  buffer_tree = NULL;
  time_at = 0;
  note_at = 0;
  for (;;)
    {
      guint next_time;
      Buffer *buf;

      if (print_status)
        {
          guint n_dots = ((guint64)time_at * 50) / approx_n_samples;
          if (n_dots > 50)
            n_dots = 50;
          while (status_n_dots < n_dots)
            {
              g_printerr (".");
              status_n_dots++;
            }
        }

      /* remove any notes that end at time 'time_at' */
      GSK_RBTREE_FIRST (GET_BUFFER_TREE (buffer_tree), buf);
      while (buf && buf->start_sample + buf->n_samples == time_at)
        {
          Buffer *next;
          GSK_RBTREE_NEXT (GET_BUFFER_TREE (buffer_tree), buf, next);
          g_free (buf->data);
          buf->data = NULL;
          GSK_RBTREE_REMOVE (GET_BUFFER_TREE (buffer_tree), buf);
          buf = next;
        }

      /* add notes that start at 'at' */
      while (note_at < n_notes && notes[note_at].start_sample == time_at)
        {
          BbScorePart *part = score->parts[notes[note_at].part_index];
          guint ni = notes[note_at].note_index;
          Buffer *collision;
          create_note_buffer (part, ni, config->sampling_rate, buffers + note_at);
          buffers[note_at].start_sample = time_at;
          GSK_RBTREE_INSERT (GET_BUFFER_TREE (buffer_tree), (buffers+note_at), collision);
          g_assert (collision == NULL);

          note_at++;
        }

      /* figure the time of the next event */
      if (buffer_tree == NULL)
        {
          if (note_at == n_notes)
            break;
          next_time = notes[note_at].start_sample;
        }
      else
        {
          Buffer *mn;
          GSK_RBTREE_FIRST (GET_BUFFER_TREE (buffer_tree), mn);
          if (note_at < n_notes)
            next_time = MIN (mn->start_sample + mn->n_samples,
                             notes[note_at].start_sample);
          else
            next_time = mn->start_sample + mn->n_samples;
        }

      /* render next_time - time_at samples */
      guint n = next_time - time_at;
      g_array_set_size (doubles, n);
      if (buffer_tree)
        {
          double *out = (double*)(doubles->data);
          guint offset = time_at - buffer_tree->start_sample;
          memcpy (out, buffer_tree->data + offset, n * sizeof (double));
          add_samples_recursive (out, buffer_tree->left, time_at, n);
          add_samples_recursive (out, buffer_tree->right, time_at, n);
        }
      else
        {
          memset (doubles->data, 0, n * sizeof (double));
        }
      if (!handler (time_at + sample_offset, n, (double*)(doubles->data), handler_data, error))
        {
          g_free (notes);
          free_tree_data_recursive (buffer_tree);
          g_free (buffers);
          g_array_free (doubles, TRUE);
          return FALSE;
        }
      time_at = next_time;
    }
  if (print_status)
    g_printerr (" done.\n");
  g_free (notes);
  g_free (buffers);
  g_array_free (doubles, TRUE);
  return TRUE;
}

#if 0
static int
compare_buffer_by_part_index (gconstpointer a, gconstpointer b)
{
  const Buffer *buf_a = a;
  const Buffer *buf_b = b;
  return (buf_a->part->index < buf_b->part->index) ? -1
       : (buf_a->part->index > buf_b->part->index) ? 1
       : 0;
}
#endif
typedef struct _AnalysisInfo AnalysisInfo;
struct _AnalysisInfo
{
  BbScorePart *part;
  gboolean first_time;
  BbScoreRenderResultPart *out;
  FILE *fp;
};
typedef struct _DiskPartTree DiskPartTree;
struct _DiskPartTree
{
  DiskPartTree *parent, *left, *right;
  gboolean is_red;
  gboolean is_active;
  guint part;
  guint part_start, part_end;
  FILE *fp;
};
      /* macros used by rbtree macros */
#define DTP_COMPARE_BY_PART_START(a,b,rv) _DTP_COMPARE(a,b,part_start,rv)
#define DTP_COMPARE_BY_PART_END(a,b,rv) _DTP_COMPARE(a,b,part_end,rv)
#define DTP_SET_IS_RED(node,r) node->is_red = r
#define DTP_GET_IS_RED(node) node->is_red

/* helper comparator macro: compare by any member, then fallback to part index */
#define _DTP_COMPARE(a,b,member,rv)             \
      if      (a->member < b->member) rv = -1;  \
      else if (a->member > b->member) rv = +1;  \
      else if (a->part < b->part)     rv = -1;  \
      else if (a->part > b->part)     rv = +1;  \
      else rv = 0

static BbScoreNoteFilterResult only_certain_part (BbScorePart *part,
                                                  guint        note_index,
                                                  gpointer     data)
{
  return (part->index == GPOINTER_TO_UINT (data))
        ? BB_SCORE_NOTE_FILTER_ACCEPT
        : BB_SCORE_NOTE_FILTER_REJECT;
}

static gboolean
handle_part_data (guint         start_sample,
                  guint         n_doubles,
                  const double *doubles,
                  gpointer      handler_data,
                  GError      **error)
{
  AnalysisInfo *ai = handler_data;
  gdouble min, max;
  guint i;
  g_assert (n_doubles > 0);
  if (ai->first_time)
    {
      min = max = doubles[0];
      ai->first_time = FALSE;
      ai->out->start_sample = start_sample;
    }
  else
    {
      min = ai->out->min_value;
      max = ai->out->max_value;
    }
  for (i = 0; i < n_doubles; i++)
    if (doubles[i] < min)
      min = doubles[i];
    else if (doubles[i] > max)
      max = doubles[i];
  if (fwrite (doubles, n_doubles * sizeof (double), 1, ai->fp) != 1)
    g_error ("error writing double data");
  ai->out->min_value = max;
  ai->out->max_value = min;
  ai->out->n_samples += n_doubles;
  return TRUE;
}

static void
safe_read (FILE *fp, gpointer buf, gsize size)
{
  g_assert (size != 0);
  if (fread (buf, size, 1, fp) != 1)
    g_error ("safe_read: read failed");
}

static void
mix_disk_file_tree_recursive_add (DiskPartTree *tree,
                                  guint         n,
                                  gdouble      *out,
                                  gdouble      *tmp)
{
  if (tree->left)
    mix_disk_file_tree_recursive_add (tree->left, n, out, tmp);
  if (tree->right)
    mix_disk_file_tree_recursive_add (tree->right, n, out, tmp);
  safe_read (tree->fp, tmp, sizeof(double)*n);
  add_samples (out, tmp, n);
}
static void
mix_disk_file_tree_recursive_replace (DiskPartTree *tree,
                                      guint         n,
                                      gdouble      *out,
                                      gdouble      *tmp)
{
  if (tree->left == NULL && tree->right == NULL)
    {
      safe_read (tree->fp, out, sizeof(double)*n);
    }
  else
    {
      if (tree->left != NULL)
        {
          mix_disk_file_tree_recursive_replace (tree->left, n, out, tmp);
          if (tree->right)
            mix_disk_file_tree_recursive_add (tree->right, n, out, tmp);
        }
      else
        {
          mix_disk_file_tree_recursive_replace (tree->right, n, out, tmp);
        }
      safe_read (tree->fp, tmp, sizeof(double)*n);
      add_samples (out, tmp, n);
    }
}
static void
mix_disk_file_tree (DiskPartTree *tree,
                    guint         n,
                    gdouble      *out,
                    gdouble      *tmp)
{
  if (n == 0)
    return;
  if (tree == NULL)
    memset (out, 0, sizeof (gdouble) * n);
  else
    mix_disk_file_tree_recursive_replace (tree, n, out, tmp);
}

BbScoreRenderResult *
bb_score_render_for_analysis(BbScore             *score,
                             BbScoreRenderConfig *config,
                             const char          *output_dir,
                             gboolean             print_status,
                             GError             **error)
{
  BbScoreRenderResultPart *parts;
  BbScoreRenderResult *rv;

  gboolean first_sample;
  DiskPartTree *ptree_nodes;
  DiskPartTree *ptree_inactive_top, *ptree_active_top;

  guint i;
  g_assert (config->note_filters->len == 0);

  if (mkdir (output_dir, 0755) < 0)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_FILE_WRITE,
                   "error creating directory %s: %s", output_dir, g_strerror (errno));
      return NULL;
    }

  parts = g_new0 (BbScoreRenderResultPart, score->n_parts);
  for (i = 0; i < score->n_parts; i++)
    {
      AnalysisInfo ai;
      bb_score_render_config_add_filter (config, only_certain_part, GUINT_TO_POINTER (i), NULL);
      ai.part = score->parts[i];
      ai.first_time = TRUE;
      ai.out = parts + i;
      parts[i].filename = g_strdup_printf ("%s/part-%04d.raw", output_dir, i);
      ai.fp = fopen (parts[i].filename, "w");
      if (ai.fp == NULL)
        {
          g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_FILE_WRITE,
                       "error creating %s: %s", parts[i].filename, g_strerror (errno));
          return NULL;
        }
      bb_score_render_config_unset_start_time (config);
      if (!bb_score_render (score, config, handle_part_data, &ai, print_status, error))
        return NULL;
      g_array_set_size (config->note_filters, 0);
      if (ai.first_time)
        {
          g_error ("part %u was empty", i);
        }
      fclose (ai.fp);
      g_message ("part %u: start_sample=%u, n_samples=%u", i, parts[i].start_sample, parts[i].n_samples);
    }

  rv = g_new (BbScoreRenderResult, 1);
  rv->n_parts = score->n_parts;
  rv->parts = parts;
  rv->filename = g_strdup_printf ("%s/song.raw", output_dir);
  rv->dir = g_strdup (output_dir);
  rv->n_samples = 0;

  /* put all the parts in a tree */
  first_sample = TRUE;
  ptree_nodes = g_new (DiskPartTree, score->n_parts);
  ptree_inactive_top = NULL;
  ptree_active_top = NULL;
#define GET_ACTIVE_TREE() \
  ptree_active_top, DiskPartTree*, DTP_GET_IS_RED, DTP_SET_IS_RED, \
  parent, left, right, DTP_COMPARE_BY_PART_END
#define GET_INACTIVE_TREE() \
  ptree_inactive_top, DiskPartTree*, DTP_GET_IS_RED, DTP_SET_IS_RED, \
  parent, left, right, DTP_COMPARE_BY_PART_START
  for (i = 0; i < score->n_parts; i++)
    {
      DiskPartTree *collision;
      ptree_nodes[i].is_active = FALSE;
      ptree_nodes[i].part = i;
      ptree_nodes[i].part_start = parts[i].start_sample;
      ptree_nodes[i].part_end = parts[i].start_sample + parts[i].n_samples;
      GSK_RBTREE_INSERT (GET_INACTIVE_TREE (), ptree_nodes + i, collision);
      g_assert (collision == NULL);
    }

  /* find the starting sample */
  if (config->has_start_time)
    {
      rv->start_sample = (guint) (config->start_time * config->sampling_rate);
    }
  else
    {
      DiskPartTree *min_node;
      GSK_RBTREE_FIRST (GET_INACTIVE_TREE (), min_node);
      rv->start_sample = min_node->part_start;
    }
  guint at;
  GArray *tmp_buffer;
  at = rv->start_sample;
  rv->min_value = rv->max_value = 0;
  gboolean has_minmax_values = FALSE;
  tmp_buffer = g_array_new (FALSE, FALSE, sizeof (gdouble));
  FILE *out_fp;
  out_fp = fopen (rv->filename, "wb");
  if (out_fp == NULL)
    g_error ("error creating %s: %s", rv->filename, g_strerror (errno));
  while (ptree_inactive_top != NULL || ptree_active_top != NULL)
    {
      guint next_event;
      if (ptree_inactive_top)
        {
          DiskPartTree *min_node;
          GSK_RBTREE_FIRST (GET_INACTIVE_TREE (), min_node);
          if (ptree_active_top)
            {
              DiskPartTree *min_node2;
              GSK_RBTREE_FIRST (GET_ACTIVE_TREE (), min_node2);
              next_event = MIN (min_node->part_start, min_node2->part_end);
            }
          else
            next_event = min_node->part_start;
        }
      else
        {
          DiskPartTree *min_node2;
          GSK_RBTREE_FIRST (GET_ACTIVE_TREE (), min_node2);
          next_event = min_node2->part_end;
        }

      /* render all active nodes together
         (sample index ranging from 'at' to 'next_event') */
      g_array_set_size (tmp_buffer, (next_event - at) * 2);
      mix_disk_file_tree (ptree_active_top,
                          next_event - at,
                          ((gdouble*) (tmp_buffer->data)),
                          ((gdouble*) (tmp_buffer->data)) + (next_event - at));
      if (next_event > at)
        {
          guint n = next_event - at;
          guint s;
          if (fwrite (tmp_buffer->data,
                      n * sizeof (gdouble),
                      1, out_fp) != 1)
            g_error ("error writing to song.raw");
          if (!has_minmax_values)
            {
              rv->min_value = rv->max_value = g_array_index (tmp_buffer, gdouble, 0);
              has_minmax_values = TRUE;
            }
          for (s = 0; s < n; s++)
            {
              gdouble v = g_array_index (tmp_buffer, gdouble, s);
              if (v < rv->min_value)
                rv->min_value = v;
              else if (v > rv->max_value)
                rv->max_value = v;
            }
        }

      rv->n_samples += (next_event - at);

      /* cleanup any parts that have ended */
      while (ptree_active_top != NULL)
        {
          DiskPartTree *min_node2;
          GSK_RBTREE_FIRST (GET_ACTIVE_TREE (), min_node2);
          g_assert (min_node2->part_end >= next_event);
          if (min_node2->part_end > next_event)
            break;

          fclose (min_node2->fp);
          GSK_RBTREE_REMOVE (GET_ACTIVE_TREE (), min_node2);
        }

      /* move any buffers that have started from inactive to active;
         open files */
      while (ptree_inactive_top != NULL)
        {
          DiskPartTree *min_node;
          DiskPartTree *collision_node;
          GSK_RBTREE_FIRST (GET_INACTIVE_TREE (), min_node);
          g_assert (min_node->part_start >= next_event);
          if (min_node->part_start > next_event)
            break;
          GSK_RBTREE_REMOVE (GET_INACTIVE_TREE (), min_node);
          min_node->is_active = TRUE;
          min_node->fp = fopen (parts[min_node->part].filename, "rb");
          if (min_node->fp == NULL)
            g_error ("error opening %s: %s", parts[min_node->part].filename, g_strerror (errno));
          GSK_RBTREE_INSERT (GET_ACTIVE_TREE (), min_node, collision_node);
          g_assert (collision_node == NULL);
        }

      at = next_event;
    }
  fclose (out_fp);
  g_array_free (tmp_buffer, TRUE);
  return rv;
}
void
bb_score_render_result_free (BbScoreRenderResult *result,
                             gboolean             do_unlink)
{
  guint i;
  for (i = 0; i < result->n_parts; i++)
    {
      if (do_unlink)
        unlink (result->parts[i].filename);
      g_free (result->parts[i].filename);
    }
  g_free (result->parts);
  if (do_unlink)
    {
      unlink (result->filename);
      rmdir (result->dir);
    }
  g_free (result->filename);
  g_free (result->dir);
  g_free (result);
}

typedef struct _RangeSave RangeSave;
struct _RangeSave
{
  FILE *doubles_out;
  guint n_samples;
  double min, max;
};

static gboolean
handle_range_save_data (guint         start_sample,
                        guint         n_doubles,
                        const double *doubles,
                        gpointer      handler_data,
                        GError      **error)
{
  RangeSave *rs = handler_data;
  guint i;
  double min = rs->min, max = rs->max;
  rs->n_samples += n_doubles;
  if (fwrite (doubles, sizeof (double), n_doubles, rs->doubles_out)
      != n_doubles)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_FILE_WRITE,
                   "error writing double-valued data");
      return FALSE;
    }
  for (i = 0; i < n_doubles; i++)
    if (doubles[i] < min)
      min = doubles[i];
    else if (doubles[i] > max)
      max = doubles[i];
  rs->min = min;
  rs->max = max;
  return TRUE;
}

gboolean bb_score_render_wavfile     (BbScore      *score,
                                      BbScoreRenderConfig *config,
                                      const char   *wav_filename,
                                      gboolean      print_status,
                                      GError      **error)
{
  RangeSave rs;
  static guint seqno = 0;
  double factor, offset;
  FILE *out_fp;
  FILE *in_fp;
  guint n_transferred;
  char *fname = g_strdup_printf ("tmp-%u-%u-%u",
                                 (guint) time (NULL), getpid (), seqno++);
  rs.min = rs.max = 0;
  rs.n_samples = 0;
  rs.doubles_out = fopen (fname, "wb");
  if (rs.doubles_out == NULL)
    {
      g_free (fname);
      return FALSE;
    }
  if (!bb_score_render (score, config,
                        handle_range_save_data, &rs, print_status, error))
    {
      fclose (rs.doubles_out);
      unlink (fname);
      g_free (fname);
      return FALSE;
    }
  fclose (rs.doubles_out);
  rs.doubles_out = NULL;

  if (rs.min == rs.max)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_FILE_WRITE,
                   "silent or empty output");
      return FALSE;
    }
  if (print_status)
    g_printerr ("Creating %.3fs wav file: ", (double) rs.n_samples / config->sampling_rate);

  out_fp = fopen (wav_filename, "wb");
  if (out_fp == NULL)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_FILE_WRITE,
                   "error creating %s for writing: %s",
                   wav_filename, g_strerror (errno));
      unlink (fname);
      g_free (fname);
      return FALSE;
    }
  in_fp = fopen (fname, "rb");

  if (config->sampling_rate != (double) (int) config->sampling_rate)
    g_warning ("sampling rate is not an exact integer (not supported by .wav file format)");

  /* write wav file header */
  {
    guint hdr_size;
    guint8 *hdr = get_wav_header (rs.n_samples, config->sampling_rate, &hdr_size);
    if (fwrite (hdr, hdr_size, 1, out_fp) != 1)
      {
        g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_FILE_WRITE,
                     "error writing wav file header");
        unlink (fname);
        g_free (fname);
        fclose (out_fp);
        fclose (in_fp);
        g_free (hdr);
        return FALSE;
      }
    g_free (hdr);
  }

#if SYMMETRIC_NORMALIZATION
  {
  gdouble ma = MAX (ABS (rs.max), ABS(rs.min));
  rs.min = -ma;
  rs.max = ma;
  }
#endif

  factor = (60000.0) / (rs.max - rs.min);
  offset = - factor * (rs.max + rs.min) / 2.0;
  /* transfer data, rescaled */
  n_transferred = 0;
  while (n_transferred < rs.n_samples)
    {
      guint xfer = MIN (rs.n_samples - n_transferred, 4096);
      double buf[4096];
      guint16 scaled_buf[4096];
      guint nread = fread (buf, sizeof (double), xfer, in_fp);
      guint i;
      if (nread < xfer)
        {
          g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_FILE_READ,
                       "error reading double data");
          return FALSE;
        }
      for (i = 0; i < xfer; i++)
        {
          double v = buf[i] * factor + offset;
          g_assert (-32760.0 < v && v < 32760.0);
          scaled_buf[i] = v;

#if G_BYTE_ORDER == G_BIG_ENDIAN
          scaled_buf[i] = GUINT16_TO_LE (scaled_buf[i]);
#endif
        }
      if (fwrite (scaled_buf, sizeof (gint16), xfer, out_fp) != xfer)
        {
          g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_FILE_WRITE,
                       "error writing wav file data");
          unlink (fname);
          g_free (fname);
          fclose (out_fp);
          unlink (wav_filename);
          return FALSE;
        }
      n_transferred += xfer;
    }
  unlink (fname);
  g_free (fname);
  fclose (out_fp);
  if (print_status)
    g_printerr ("done.\n");
  return TRUE;
}

static gboolean
handle_raw_save_data (guint         start_sample,
                      guint         n_doubles,
                      const double *doubles,
                      gpointer      handler_data,
                      GError      **error)
{
  FILE *fp = handler_data;
  if (fwrite (doubles, sizeof (double), n_doubles, fp) != n_doubles)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_FILE_WRITE,
                   "error writing raw data");
      return FALSE;
    }
  return TRUE;
}

gboolean bb_score_render_raw         (BbScore      *score,
                                      BbScoreRenderConfig *config,
                                      const char   *raw_filename,
                                      gboolean      print_status,
                                      GError      **error)
{
  FILE * fp = fopen (raw_filename, "wb");
  if (fp == NULL)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_FILE_WRITE,
                   "error creating %s: %s", raw_filename, g_strerror (errno));
      return FALSE;
    }
  if (!bb_score_render (score, config,
                        handle_raw_save_data, fp, print_status, error))
    {
      fclose (fp);
      return FALSE;
    }
  fclose (fp);
  return TRUE;
}
