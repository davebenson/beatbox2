#include <math.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "bb-score.h"
#include "bb-error.h"
#include "bb-parse.h"
#include "bb-utils.h"
#include "bb-duration.h"
#include "macros.h"
#include <gsk/gskmacros.h>
#include <gsk/gskghelpers.h>

gboolean bb_score_gnuplot_notes = FALSE;

BbScore *bb_score_new                (void)
{
  BbScore *rv = g_new0 (BbScore, 1);
  rv->beats_alloced = 4;
  rv->beats = g_new (BbScoreBeat, rv->beats_alloced);
  rv->parts_alloced = 8;
  rv->parts = g_new (BbScorePart*, rv->parts_alloced);
  rv->marks = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  rv->parts_by_name = g_hash_table_new (g_str_hash, g_str_equal);
  return rv;
}

double   bb_score_get_last_beat      (BbScore      *score)
{
  if (score->n_beats == 0)
    return 0;
  else
    {
      BbScoreBeat *b = score->beats + (score->n_beats - 1);
      return b->first_beat + b->n_beats;
    }
}

double   bb_score_get_last_time      (BbScore      *score)
{
  if (score->n_beats == 0)
    return 0;
  else
    {
      BbScoreBeat *b = score->beats + (score->n_beats - 1);
      return b->first_beat_time + b->n_seconds;
    }
}

static BbScoreBeat *
add_beats_generic (BbScore *score)
{
  double last_beat = bb_score_get_last_beat (score);
  double last_time = bb_score_get_last_time (score);
  if (score->beats_alloced == score->n_beats)
    {
      guint new_n_alloced = score->beats_alloced * 2;
      score->beats = g_renew (BbScoreBeat, score->beats, new_n_alloced);
      score->beats_alloced = new_n_alloced;
    }
  score->beats[score->n_beats].first_beat = last_beat;
  score->beats[score->n_beats].first_beat_time = last_time;
  return &score->beats[(score->n_beats)++];
}

void     bb_score_add_beats          (BbScore      *score,
                                      gdouble       n_beats,
				      double        beat_period)
{
  BbScoreBeat *b = add_beats_generic (score);
  g_assert (n_beats > 0);
  b->mode = BB_BEAT_CONSTANT;
  b->n_beats = n_beats;
  b->start_beat_period = b->end_beat_period = beat_period;
  b->n_seconds = n_beats * beat_period;
}

void     bb_score_add_beats_variable (BbScore      *score,
                                      gdouble       n_beats,
                                      BbBeatMode    beat_mode,
				      double        start_beat_period,
                                      double        end_beat_period)
{
  BbScoreBeat *b = add_beats_generic (score);
  gdouble delta_bp = end_beat_period - start_beat_period;
  g_assert (n_beats > 0);
  b->mode = beat_mode;
  b->n_beats = n_beats;
  b->start_beat_period = start_beat_period;
  b->end_beat_period = end_beat_period;
  if (start_beat_period * -1e-6 <= delta_bp && delta_bp <= start_beat_period * 1e-6)
    {
      /* constant approximation */
      b->n_seconds = n_beats * start_beat_period;
    }
  else
    {
      switch (beat_mode)
        {
        case BB_BEAT_CONSTANT:
          g_error ("add_beats_variable called with beat_mode==CONSTANT?");
          b->n_seconds = start_beat_period * n_beats;
          break;

        case BB_BEAT_LINEAR:
          b->n_seconds = 0.5 * (start_beat_period + end_beat_period) * n_beats;
          break;

        case BB_BEAT_GEOMETRIC:
          {
            /* see derivation in bb_score_get_time_from_beat() */
            gdouble alpha = end_beat_period / start_beat_period;
            b->n_seconds = start_beat_period * n_beats / log (alpha) * (alpha - 1.0);
            break;
          }

        case BB_BEAT_HARMONIC:
          {
            /* see derivation in bb_score_get_time_from_beat() */
            gdouble alpha = ((1.0 / end_beat_period) - (1.0 / start_beat_period)) / n_beats;
            gdouble beta = 1.0 / start_beat_period;
            b->n_seconds = (log (alpha * n_beats + beta) - log (beta)) / alpha;
            break;
          }

        default:
          g_assert_not_reached ();
        }
    }
}

void         bb_score_part_set_name   (BbScorePart  *part,
                                       const char   *name)
{
  if (g_hash_table_lookup (part->score->parts_by_name, name) != NULL)
    g_error ("score part with name '%s' already exists", name);

  g_return_if_fail (part->name == NULL);
  part->name = g_strdup (name);
  g_hash_table_insert (part->score->parts_by_name, part->name, part);
}

BbScorePart *bb_score_get_part_by_name(BbScore      *score,
                                       const char   *name)
{
  return g_hash_table_lookup (score->parts_by_name, name);
}

void
bb_score_set_mark (BbScore    *score,
                   const char *name,
                   gdouble     beat)
{
  g_hash_table_replace (score->marks,
                        g_strdup (name),
                        g_memdup (&beat, sizeof (gdouble)));
}

gboolean
bb_score_get_mark (BbScore    *score,
                   const char *name,
                   gdouble    *beat_out)
{
  gdouble *b = g_hash_table_lookup (score->marks, name);
  if (b == NULL)
    return FALSE;
  *beat_out = *b;
  return TRUE;
}

#define N_EXTRA_VALUES  4               /* time, part_time, note_time, bps */

enum
{
  EXTRA_TIME,
  EXTRA_PART_TIME,
  EXTRA_NOTE_TIME,
  EXTRA_BPS
};

static GParamSpec **
peek_extra_pspecs (void)
{
  static GParamSpec *specs[N_EXTRA_VALUES];
  if (specs[0] == NULL)
    {
      specs[EXTRA_TIME] = g_param_spec_double ("time", "Time", NULL,
                                               -1e6,1e6,0,0);
      specs[EXTRA_PART_TIME] = g_param_spec_double ("part-time", "Part Time", NULL,
                                               -1e6,1e6,0,0);
      specs[EXTRA_NOTE_TIME] = g_param_spec_double ("note-time", "Note Time", NULL,
                                               -1e6,1e6,0,0);
      specs[EXTRA_BPS] = g_param_spec_double ("bps", "Beats Per Second", NULL,
                                               1e-6,1e6,1,0);
    }
  return specs;
}

BbScorePart * bb_score_add_part           (BbScore      *score,
                                           BbInstrument *instrument,
                                           gdouble       beat)
{
  BbScorePart *part;
  guint i;
  GParamSpec **extra_pspecs;
  /* create part */
  part = g_new (BbScorePart, 1);
  part->score = score;
  part->instrument = instrument;
  part->n_notes = 0;
  part->notes_alloced = 8;
  part->notes = g_new (BbScoreNote, part->notes_alloced);
  part->first_beat = beat;
  part->cur_beat = beat;
  part->cur_pitchshift = 0;
  part->cur_octave = 0;
  part->complex_note_events = NULL;
  part->cur_note_step = 1.0;
  part->cur_comma_step = 0.5;
  part->cur_dot_beats = 0.5;
  part->cur_volume = 1.0;
  part->cur_volume_factor = 2.0;
  part->cur_tuning = bb_tuning_get ("default");
  part->volume_program = NULL;
  part->max_beat = part->min_beat = beat;
  part->name = NULL;
  part->index = score->n_parts;
  if (instrument->complex_synth_func)
    part->complex_note_init_values = g_new (GValue, instrument->n_param_specs);
  else
    part->complex_note_init_values = NULL;
  //double complex_note_start, complex_note_volume, complex_note_start_time;

  /* add part */
  if (score->n_parts == score->parts_alloced)
    {
      guint new_n_alloced = score->parts_alloced * 2;
      score->parts = g_renew (BbScorePart *, score->parts, new_n_alloced);
      score->parts_alloced = new_n_alloced;
    }
  score->parts[score->n_parts++] = part;

  extra_pspecs = peek_extra_pspecs ();

  part->program_pspecs = g_ptr_array_new ();
  part->value_programs = g_new0 (BbProgram *, instrument->n_param_specs);
  part->latest_values = g_new0 (GValue, instrument->n_param_specs + N_EXTRA_VALUES);
  for (i = 0; i < instrument->n_param_specs; i++)
    {
      GParamSpec *pspec = instrument->param_specs[i];
      g_ptr_array_add (part->program_pspecs, pspec);
      g_value_init (part->latest_values + i, G_PARAM_SPEC_VALUE_TYPE (pspec));
      g_param_value_set_default (pspec, part->latest_values + i);
    }
  for (i = 0; i < N_EXTRA_VALUES; i++)
    g_ptr_array_add (part->program_pspecs, extra_pspecs[i]);
  part->n_program_pspecs = part->program_pspecs->len;
  g_assert (part->n_program_pspecs  == instrument->n_param_specs + N_EXTRA_VALUES);
  for (i = 0; i < N_EXTRA_VALUES; i++)
    g_value_init (part->latest_values + instrument->n_param_specs + i, G_TYPE_DOUBLE);

  return part;
}

#if 0
static void
init_render_info (BbScorePart *part,
                  BbRenderInfo *render_info)
{
  ...
}
#endif

void
bb_score_part_set_octave (BbScorePart *part,
                          gint         octave)
{
  part->cur_octave = octave;
}

void
bb_score_part_adjust_octave (BbScorePart *part,
                             gint         octave)
{
  part->cur_octave += octave;
}

static void
init_instrument_params (BbScorePart *part,
                        GValue      *values)
{
  GValue *latest_extra = part->latest_values + part->instrument->n_param_specs;
  double time, part_time;
  guint i;
  BbScore *score = part->score;
  double beat_period;

  /* setup time-dependent params */
  //init_render_info (part, &render_info);
  time = bb_score_get_time_from_beat (part->score, part->cur_beat);
  part_time = time - bb_score_get_time_from_beat (score, part->first_beat);
  beat_period = bb_score_get_beat_period_from_beat (score, part->cur_beat);
  g_value_set_double (latest_extra + EXTRA_TIME, time);
  g_value_set_double (latest_extra + EXTRA_PART_TIME, part_time);
  g_value_set_double (latest_extra + EXTRA_NOTE_TIME, 0);
  g_value_set_double (latest_extra + EXTRA_BPS, 1.0 / beat_period);

  /* evaluate each values program */
  for (i = 0; i < part->instrument->n_param_specs; i++)
    {
      g_value_init (&values[i], G_VALUE_TYPE (&part->latest_values[i]));
      if (part->value_programs[i] == NULL)
        g_value_copy (&part->latest_values[i], &values[i]);
      else
        {
          bb_vm_run_get_value_or_die (part->value_programs[i],
                                      NULL,     /* no render-info */
                                      part->n_program_pspecs,
                                      part->latest_values,
                                      &values[i]);
          g_value_copy (&values[i], &part->latest_values[i]);
        }
    }
}

void
bb_score_part_note_on (BbScorePart *part)
{
  /* note on event */
  BbInstrument *instrument = part->instrument;
  if (part->complex_note_events != NULL)
    g_error ("got two ON directives with no OFF directive in between");

  /* initialize complex_note_events, complex_note_init_values to honor the ON event */
  part->complex_note_events = g_array_new (FALSE, FALSE, sizeof (BbInstrumentEvent));
  memset (part->complex_note_init_values, 0, sizeof (GValue) * instrument->n_param_specs);
  part->complex_note_volume = part->cur_volume;
  part->complex_note_start = part->cur_beat;
  part->complex_note_start_time = bb_score_get_time_from_beat (part->score, part->cur_beat);

  /* evaluate parameters */
  init_instrument_params (part, part->complex_note_init_values);
}

static BbScoreNote *
add_notes_raw (BbScorePart    *part,
               guint           space)
{
  BbScoreNote *rv;
  if (part->n_notes + space > part->notes_alloced)
    {
      guint new_alloced = part->notes_alloced * 2;
      while (part->n_notes + space > new_alloced)
        new_alloced += new_alloced;
      part->notes = g_renew (BbScoreNote, part->notes, new_alloced);
      part->notes_alloced = new_alloced;
    }
  rv = part->notes + part->n_notes;
  part->n_notes += space;
  return rv;
}

void
bb_score_part_note_off (BbScorePart *part)
{
  /* note off event */
  BbInstrumentEvent dummy;
  BbScoreNote *note = add_notes_raw (part, 1);
  BbInstrument *instrument = part->instrument;
  if (part->complex_note_events == NULL)
    g_error ("got OFF event without an ON event");
  note->type = BB_SCORE_NOTE_COMPLEX;
  note->values = g_memdup (part->complex_note_init_values,
                           sizeof (GValue) * instrument->n_param_specs);
  note->volume = part->complex_note_volume;
  note->start = part->complex_note_start;

  /* add dummy event to give duration information */
  dummy.time = bb_score_get_time_from_beat (part->score, part->cur_beat) - part->complex_note_start_time;
  g_array_append_val (part->complex_note_events, dummy);

  note->n_events = part->complex_note_events->len - 1;	/* subtract off dummy */
  note->events = (BbInstrumentEvent *) g_array_free (part->complex_note_events, FALSE);
  part->complex_note_events = NULL;
}

/* takes into account 'octave', but not pitchshift */
static gint
get_note_id (BbScorePart *part,
             const char  *start,
             const char **end_out)
{
#if 0
                                   /*a  b  c  d  e  f  g   */
  static gint notes_by_letter[7] = { 0, 2, 3, 5, 7, 8, 10 };
  const char *str = start;
  gint note_id = notes_by_letter[*str - 'a'];
  g_assert ('a' <= *start && *start <= 'g');
  str++;
  if (*str == '+')
    {
      note_id++;
      str++;
    }
  else if (*str == '-')
    {
      note_id--;
      str++;
    }
  else if (*str == '=')
    {
      str++;
    }
  *end_out = str;
  return note_id + part->cur_octave * 12;
#else
  const char *end = start + 1;
  gint rv;
  GError *error = NULL;
  while (*end == '+' || *end == '-' || *end == '=')
    end++;
  if (!bb_tuning_get_key_len (part->cur_tuning,
                              start, end - start, part->cur_octave,
                              &rv,
                              &error))
    g_error ("error getting note number for key %.*s: %s",
             (guint)(end-start), start, error->message);
  *end_out = end;
  return rv;
#endif
}

static inline gdouble
get_complex_note_time (BbScorePart *part)
{
  g_assert (part->complex_note_events);
  return bb_score_get_time_from_beat (part->score, part->cur_beat)
       - part->complex_note_start_time;
}

void
bb_score_part_set_param (BbScorePart *part,
                         guint        param_index,
                         const GValue *value)
{
  GParamSpec *pspec = part->instrument->param_specs[param_index];

  if (G_VALUE_TYPE (value) != G_PARAM_SPEC_VALUE_TYPE (pspec))
    g_error ("bb_score_part_set_param: setting instrument parameter %s to type %s not allowed, allowed type is %s",
             pspec->name, g_type_name (G_VALUE_TYPE (value)),
             g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)));

  g_value_copy (value, part->latest_values + param_index);
  if (part->complex_note_events)
    {
      BbInstrumentEvent event;
      event.type = BB_INSTRUMENT_EVENT_PARAM;
      event.time = get_complex_note_time (part);
      event.info.param.index = param_index;
      memset (&event.info.param.value, 0, sizeof (GValue));
      g_value_init (&event.info.param.value, G_VALUE_TYPE (value));
      g_value_copy (value, &event.info.param.value);
      g_array_append_val (part->complex_note_events, event);
    }
}

void
bb_score_part_set_note (BbScorePart *part,
                        gint         note_id)
{
  /* set frequency parameter */
  double freq;
  int p = part->instrument->frequency_param;
  GValue v = BB_VALUE_INIT;
  if (p < 0)
    {
      g_warning ("bb_score_part_set_note: instrument has no frequency param");
      return;
    }
  freq = bb_tuning_key_to_freq (part->cur_tuning, note_id + part->cur_pitchshift);
  g_value_init (&v, G_TYPE_DOUBLE);
  g_value_set_double (&v, freq);
  bb_score_part_set_param (part, p, &v);
}

void
bb_score_part_add_note (BbScorePart *part)
{
  /* add note */
  BbScoreNote *note;
  const GValue *cur = part->latest_values;
  guint i;
  BbInstrument *instrument = part->instrument;

  g_assert (part->complex_note_events == NULL);

  note = add_notes_raw (part, 1);
  /* evaluate programs */
  note->type = BB_SCORE_NOTE_SIMPLE;
  note->values = g_new0 (GValue, part->instrument->n_param_specs);
  note->start = part->cur_beat;
  note->n_events = 0;
  note->events = NULL;
  init_instrument_params (part, note->values);
  for (i = 0; i < instrument->n_param_specs; i++)
    {
      if (part->value_programs[i] != NULL)
        {
          bb_vm_run_get_value_or_die (part->value_programs[i],
                                      NULL,
                                      part->n_program_pspecs,
                                      part->latest_values,
                                      &part->latest_values[i]);
        }
    }
  if (part->volume_program != NULL)
    {
      part->cur_volume = bb_vm_run_get_double_or_die (part->volume_program,
                                                      NULL,
                                                      part->n_program_pspecs,
                                                      part->latest_values);
    }
  note->volume = part->cur_volume;

  note->values = g_new0 (GValue, instrument->n_param_specs);
  for (i = 0; i < instrument->n_param_specs; i++)
    {
      g_value_init (note->values + i, G_VALUE_TYPE (cur + i));
      g_value_copy (cur + i, note->values + i);
    }
  part->cur_beat += part->cur_note_step;
}

static gboolean
handle_curly_clause (BbScorePart *part,
                     const char  *name,
                     char         type_char,    /* ':', '=' or '~' */
                     BbProgram   *program,
                     GError     **error)
{
  BbInstrument *instrument = part->instrument;
  switch (type_char)
    {
    case '=':
    case '~':
      {
      GValue constant_value = BB_VALUE_INIT;
      if (type_char == '=')
        {
          bb_vm_run_get_value_or_die (program, NULL,
                                      part->n_program_pspecs,
                                      part->latest_values,
                                      &constant_value);
        }
      if (instrument->volume_param < 0
        && strcmp (name, "volume") == 0)
        {
          if (part->volume_program != NULL)
            {
              bb_program_unref (part->volume_program);
              part->volume_program = NULL;
            }
          if (type_char == '=')
            part->cur_volume = g_value_get_double (&constant_value);
          else
            part->volume_program = bb_program_ref (program);
        }
      else 
        {
          /* parameter: either deferred-eval or immediate-eval. */
          /* lookup param */
          guint p;
          for (p = 0; p < instrument->n_param_specs; p++)
            if (bb_param_spec_names_equal (name, instrument->param_specs[p]->name))
              break;
          if (p == instrument->n_param_specs)
            {
              g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                           "no parameter %s found", name);
              return FALSE;
            }
          if (part->value_programs[p] != NULL)
            bb_program_unref (part->value_programs[p]);
          if (type_char == '=')
            {
              part->value_programs[p] = NULL;
              bb_score_part_set_param (part, p, &constant_value);
              g_value_unset (&constant_value);
            }
          else
            {
              part->value_programs[p] = bb_program_ref (program);
            }
        }
      }
      break;
    case ':':
      {
        guint a;
        guint i;
        BbVm *vm;
        BbInstrumentActionSpec *aspec;
        BbInstrumentEvent event;
        if (part->complex_note_events == NULL)
          g_error ("actions cannot be called outside of ON/OFF sections");

        /* find the action index */
        for (a = 0; a < instrument->n_actions; a++)
          if (bb_param_spec_names_equal (name, instrument->actions[a]->name))
            break;
        if (a == instrument->n_actions)
          g_error ("no action named '%s' found", name);
        aspec = instrument->actions[a];

        /* run the program to get the list of action inputs */
        vm = bb_vm_new (part->n_program_pspecs, part->latest_values, FALSE);
        if (program != NULL)
          if (!bb_vm_run (vm, program, NULL, error))
            {
              gsk_g_error_add_prefix (error, "error running bb-vm for action %s", name);
              bb_vm_free (vm);
              return FALSE;
            }

        /* check action signature */
        if (aspec->n_args != vm->n_values)
          {
            g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                         "action '%s' expected %u args but got %u",
                         name, aspec->n_args, vm->n_values);
            bb_vm_free (vm);
            return FALSE;
          }
        for (i = 0; i < vm->n_values; i++)
          if (!g_type_is_a (G_VALUE_TYPE (vm->values + i),
                            G_PARAM_SPEC_VALUE_TYPE (aspec->args[i])))
            {
              g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                 "type mismatch in argument %u of action %s: got %s but expected %s",
                           i, name,
                           g_type_name (G_VALUE_TYPE (vm->values + i)),
                           g_type_name (G_PARAM_SPEC_VALUE_TYPE (aspec->args[i])));
              bb_vm_free (vm);
              return FALSE;
            }

        /* add action */
        event.type = BB_INSTRUMENT_EVENT_ACTION;
        event.time = get_complex_note_time (part);
        event.info.action.index = a;
        event.info.action.args = g_new0 (GValue, vm->n_values);
        for (i = 0; i < vm->n_values; i++)
          {
            g_value_init (event.info.action.args + i, G_VALUE_TYPE (vm->values + i));
            g_value_copy (vm->values + i, event.info.action.args + i);
          }
        g_array_append_val (part->complex_note_events, event);
        bb_vm_free (vm);
        break;
      }
    }
  return TRUE;
}

static gboolean
is_string (const char *start, const char *end, const char *test)
{
  guint len = strlen (test);
  if ((guint)(end - start) != len)
    return FALSE;
  return memcmp (start, test, len) == 0;
}

static gboolean
handle_fade (const char *str, 
             const char **p_end,
             BbScorePart *part,
             GError **error)
{
  gdouble start_beat = part->cur_beat;
  gdouble end_beat = part->cur_beat;
  GValue start_value = BB_VALUE_INIT;
  GValue end_value = BB_VALUE_INIT;
  gint param_index = -1;
  gboolean adjust_volume = FALSE;
  const char *end;
  BbInterpolationMode interpolation_mode = BB_INTERPOLATION_LINEAR;
  BbAdjustmentMode adjust_mode = BB_ADJUSTMENT_REPLACE;
  GValue tmp = BB_VALUE_INIT;
  guint i;
  while (*str != '}')
    {
      const char *equals;
      char *value_str;
      end = bb_program_string_scan (str);
      if (end == NULL)
        {
          g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                       "error scanning end of fade");
          return FALSE;
        }
      if (*end == 0)
        {
          g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                       "end-of-file in FADE{...}");
          return FALSE;
        }
      if (*end != ',' && *end != '}')
        {
          g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                       "bad character '%c' in FADE{...}", *end);
          return FALSE;
        }
        
      equals = memchr (str, '=', end - str); 
      if (equals == NULL)
        {
          g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                       "fade arguments consist of key=value pairs (missing '=')");
          return FALSE;
        }
      value_str = g_strndup (equals + 1, end - (equals + 1));
      if (is_string (str, equals, "param"))
        {
          guint p;
          GType value_type;
          if (param_index >= 0 || adjust_volume)
            {
              g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                           "'param' fade argument specified twice");
              return FALSE;
            }
          if (!bb_instrument_lookup_param (part->instrument, value_str, &p))
            {
              if (strcmp (value_str, "volume") == 0)
                {
                  adjust_volume = TRUE;
                }
              else
                {
                   g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                                "instrument does not have a parameter '%s' (in FADE{...})",
                                value_str);
                   return FALSE;
               }
             value_type = G_TYPE_DOUBLE;
           }
         else
           {
             param_index = p;
             value_type = G_PARAM_SPEC_VALUE_TYPE (part->instrument->param_specs[p]);
           }
          g_value_init (&start_value, value_type);
          g_value_init (&end_value, value_type);
          g_value_init (&tmp, value_type);
        }
      else if (is_string (str, equals, "start_mark"))
        {
          if (!bb_score_get_mark (part->score, value_str, &start_beat))
            {
              g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                           "no mark '%s' found for start_mark in FADE{}",
                           value_str);
              return FALSE;
            }
        }
      else if (is_string (str, equals, "end_mark"))
        {
          if (!bb_score_get_mark (part->score, value_str, &end_beat))
            {
              g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                           "no mark '%s' found for end_mark in FADE{}",
                           value_str);
              return FALSE;
            }
        }
      else if (is_string (str, equals, "start_value"))
        {
          if (param_index < 0 && !adjust_volume)
            {
              g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                           "'param' fade argument must precede 'start_value'");
              return FALSE;
            }
          if (!bb_vm_parse_run_string_get_value (value_str, NULL, &start_value, error))
            return FALSE;
        }
      else if (is_string (str, equals, "end_value"))
        {
          if (param_index < 0 && !adjust_volume)
            {
              g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                           "'param' fade argument must precede 'end_value'");
              return FALSE;
            }
          if (!bb_vm_parse_run_string_get_value (value_str, NULL, &end_value, error))
            return FALSE;
        }
      else if (is_string (str, equals, "interpolate"))
        {
          guint v;
          if (!bb_enum_value_lookup (BB_TYPE_INTERPOLATION_MODE, value_str, &v))
            {
              g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                           "no interpolation mode '%s' in FADE{...}",
                           value_str);
              return FALSE;
            }
          interpolation_mode = v;
        }
      else if (is_string (str, equals, "adjust"))
        {
          guint v;
          if (!bb_enum_value_lookup (BB_TYPE_ADJUSTMENT_MODE, value_str, &v))
            {
              g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                           "no adjustment mode '%s' in FADE{...}",
                           value_str);
              return FALSE;
            }
          adjust_mode = v;
        }
      else
        {
          g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                       "unexpected option %.*s in FADE{}", (guint)(equals-str), str);
          return FALSE;
        }
     g_free (value_str);
     str = end;
     if (*str == ',')
       {
         str++;
         GSK_SKIP_WHITESPACE (str);
       }
    }

  if (param_index < 0 && !adjust_volume)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                   "no param or values specified to FADE{}");
      return FALSE;
    }
  if (start_beat >= end_beat)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                   "FADE{} is specified backward or zero-length");
      return FALSE;
    }

  /* perform fade */
  for (i = 0; i < part->n_notes; i++)
    if (start_beat <= part->notes[i].start 
     && part->notes[i].start < end_beat)
      {
        double frac = (part->notes[i].start - start_beat) / (end_beat - start_beat);
        if (adjust_volume)
          {
            double n = bb_interpolate_double (frac, g_value_get_double (&start_value), g_value_get_double (&end_value), interpolation_mode);
            part->notes[i].volume = bb_adjust_double (part->notes[i].volume, n, adjust_mode);
          }
        else
          {
            bb_interpolate_value (&tmp, frac, &start_value, &end_value, interpolation_mode);
            bb_adjust_value (part->notes[i].values + param_index, &tmp, adjust_mode);
          }
      }
  g_value_unset (&tmp);
  g_value_unset (&start_value);
  g_value_unset (&end_value);

  /* ends right after '}' */
  *p_end = str + 1;
  return TRUE;
}

static void
convert_to_grace_notes (BbScorePart   *part,
                        guint          first_note,
                        gdouble        at_beat,
                        gdouble        fake_end_beat,
                        gdouble        time_scale,
                        gdouble        duration_scale,
                        gdouble        volume_scale)
{
  guint n = part->n_notes - first_note;
  guint i;
  if (n == 0)
    g_warning ("empty grace note clause");
  for (i = 0; i < n; i++)
    {
      BbScoreNote *note = part->notes + first_note + i;
      int p;
      note->start = (note->start - fake_end_beat) * time_scale + at_beat;
      p = part->instrument->volume_param;
      if (p >= 0)
        g_value_set_double (note->values + p, g_value_get_double (note->values + p) * volume_scale);
      else
        note->volume *= volume_scale;
      p = part->instrument->duration_param;
      if (p >= 0)
        bb_value_scale_duration (note->values + p, duration_scale);
   }
}

BbScorePart * bb_score_add_part_as_string (BbScore      *score,
                                           BbInstrument *instrument,
				           double        beat,
				           const char   *str,
				           GError      **error)
{
  BbScorePart *part = bb_score_add_part (score, instrument, beat);
  gboolean in_grace = FALSE;
  gdouble grace_start_note=0,grace_start_beat=0;
  gdouble grace_volume_scale = 0.6;
  gdouble grace_duration_scale = 1.0 / 3.0;
  gdouble grace_time_scale = grace_duration_scale;
  BbRenderInfo *render_info = NULL;             /* placeholder */

  while (*str)
    {
      if (*str == 0)
        break;

      switch (*str)
        {
        case ' ': case '\t': case '\n':
          str++;
          break;
        case 'M':		/* set mark */
          {
            const char *mark_name_start;
            char *name;
            str++;
            GSK_SKIP_WHITESPACE (str);
            mark_name_start = str;
            GSK_SKIP_CHAR_TYPE (str, IS_IDENTIFIER_CHAR);
            if (mark_name_start == str)
              g_error ("no identifier found after 'M'");
            name = g_strndup (mark_name_start, str - mark_name_start);
            bb_score_set_mark (score, name, part->cur_beat);
            g_free (name);
            break;
          }
        case 'J':
          {
            const char *mark_name_start;
            char *mark_name;
            str++;
            GSK_SKIP_WHITESPACE (str);
            mark_name_start = str;
            GSK_SKIP_CHAR_TYPE (str, IS_IDENTIFIER_CHAR);
            if (mark_name_start == str)
              g_error ("no identifier found after 'J'");
            mark_name = g_strndup (mark_name_start, str - mark_name_start);
            if (!bb_score_get_mark (score, mark_name, &part->cur_beat))
              g_error ("no mark %s found", mark_name);
            g_free (mark_name);
            break;
          }
        case 'O':
          if (memcmp (str, "ON", 2) == 0)
            {
              bb_score_part_note_on (part);
              str += 2;
            }
          else if (memcmp (str, "OFF", 3) == 0)
            {
              bb_score_part_note_off (part);
              str += 3;
            }
          break;
        case 'V':
          {
            gdouble old_volume = instrument->volume_param >= 0
                               ? g_value_get_double (part->latest_values + instrument->volume_param)
                               : part->cur_volume;
            gdouble volume=1;
            if (str[1] == '+')
              {
                volume = old_volume * part->cur_volume_factor;
                str += 2;
              }
            else if (str[1] == '-')
              {
                volume = old_volume / part->cur_volume_factor;
                str += 2;
              }
            else if (g_ascii_isdigit (str[1]))
              {
                char *end;
                volume = strtod (str + 1, &end);
                if (str + 1 == end)
                  g_error ("error parsing number from volume");
                str = end;
              }
            else if (str[1] == 0)
              g_error ("unexpected EOF after 'V'");
            else
              g_error ("unexpected char '%c' after 'V'", str[1]);
            
            if (instrument->volume_param >= 0)
              {
                GValue v = BB_VALUE_INIT;
                g_value_init (&v, G_TYPE_DOUBLE);
                g_value_set_double (&v, volume);
                bb_score_part_set_param (part, instrument->volume_param, &v);
              }
            else
              part->cur_volume = volume;
            break;
          }
        case 'F':
          {
            if (memcmp (str, "FADE{", 5) == 0)
              {
                const char *end;
                if (!handle_fade (str + 5, &end, part, error))
                  return FALSE;
                str = end;
              }
            else
              {
                g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                             "got F without FADE");
                return FALSE;
              }
            break;
          }
        case 'G':
          {
            if (memcmp (str, "GRACE{", 6) == 0)
              {
                in_grace = TRUE;
                grace_start_note = part->n_notes;
                grace_start_beat = part->cur_beat;
                str += 6;
              }
            else
              {
                g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                             "got G without GRACE");
                return FALSE;
              }
            break;
          }
    
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case 'g':
          {
            gint note_id = get_note_id (part, str, &str);
            bb_score_part_set_note (part, note_id);

            if (part->complex_note_events == NULL)
              bb_score_part_add_note (part);
            else
              part->cur_beat += part->cur_note_step;
          }
	  break;

        case 'x':
          {
            str++;
            if (part->complex_note_events != NULL)
              g_error ("'x' does not make sense in note ON context");
            bb_score_part_add_note (part);
          }
          break;

        case '.':
          {
            /* extend last note */
            BbScoreNote *last_note;
            GValue *last_dur;
	    /* setting <dotbeats=0> is a convenient way to
	     * ignore errors if '.' is used in a macro
	     * that is interpreted by a 'complex' instrument. */
	    if (part->cur_dot_beats == 0)
	      {
		str++;
		break;
	      }
            if (part->n_notes == 0)
	      g_error ("'.' encountered before first note");
            str++;
            if (instrument->duration_param < 0)
              g_error ("'.' encountered but no duration parameter");
            last_note = part->notes + part->n_notes - 1;
            last_dur = last_note->values + instrument->duration_param;
            bb_value_set_duration (last_dur,
                                   BB_DURATION_UNITS_BEATS,
                                   bb_value_get_duration_as_beats (last_dur, render_info) + part->cur_dot_beats);
            break;
          }
        case '_':
          bb_score_part_adjust_octave (part, -1);
          str++;
          break;
        case '^':
          bb_score_part_adjust_octave (part, 1);
          str++;
          break;

        case '{':
          {
            const char *name_start;
            const char *name_end;
            const char *program_start;
            char *name;
            char type_char;
            guint brace_count;
            char *program_text;
            BbProgram *program;
            str++;
            GSK_SKIP_WHITESPACE (str);
            name_start = str;
            GSK_SKIP_CHAR_TYPE (str, IS_IDENTIFIER_CHAR);
            name_end = str;
            if (name_start == name_end)
              g_error ("error parsing identifier after '"LBRACE_STR"'");
            GSK_SKIP_WHITESPACE (str);
            type_char = *str++;
            if (type_char == 0)
              g_error ("unexpected eof after {%.*s",
                       (guint)(name_end-name_start), name_start);
            else if (type_char != ':' && type_char != '~' && type_char != '=')
              g_error ("unexpected char '%c' after {%.*s",
                       type_char, (guint)(name_end-name_start), name_start);

            program_start = str;
            brace_count = 0;
            while (*str)
              {
                if (*str == '{')
                  brace_count++;
                else if (*str == '}')
                  {
                    if (brace_count == 0)
                      break;
                    else
                      brace_count--;
                  }
                str++;
              }
            if (*str == 0)
              g_error ("missing } after {%.*s",
                       (guint)(name_end-name_start), name_start);
            program_text = g_strndup (program_start, str - program_start);
            program = bb_program_parse (program_text, error);
            if (program == NULL)
              {
                if (type_char != ':')
                  {
                    gsk_g_error_add_prefix (error,
                                            "error parsing program for %.*s",
                                            (guint)(name_end-name_start), name_start);
                    return FALSE;
                  }
                g_clear_error (error);
              }

            /* ensure that no new params were made */
            g_free (program_text);

            str++;              /* skip right-brace */
            name = g_strndup (name_start, name_end - name_start);
            if (!handle_curly_clause (part, name, type_char, program, error))
              {
                if (program)
                  bb_program_unref (program);
                g_free (name);
                return NULL;
              }
            g_free (name);
            if (program)
              bb_program_unref (program);
          }
        break;
        case '}':
          if (!in_grace)
            {
              g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                           "unexpected '}': maybe mismatched"); 
              return NULL;
            }
          str++;
          convert_to_grace_notes (part, grace_start_note, grace_start_beat, part->cur_beat,
                                  grace_time_scale, grace_duration_scale, grace_volume_scale);
          part->cur_beat = grace_start_beat;	/* XXX: max_beat is not adjusted, but who cares? */
          break;

        case '<':
          if (str[1] == '<')
            {
              str += 2;
              part->cur_beat -= part->cur_note_step;
              break;
            }
          if (str[1] == ',')
            {
              str += 2;
              part->cur_beat -= part->cur_comma_step;
              break;
            }
          {
            /* processing directive */
            const char *end = strchr (str, '>');
            if (end == NULL)
              {
                g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                             "missing '>' in score-part");
                return NULL;
              }
            str++;
            GSK_SKIP_WHITESPACE (str);
            if (g_str_has_prefix (str, "notestep="))
              {
                part->cur_note_step = strtod (strchr (str, '=') + 1, NULL);
              }
            else if (g_str_has_prefix (str, "commastep="))
              {
                part->cur_comma_step = strtod (strchr (str, '=') + 1, NULL);
              }
            else if (g_str_has_prefix (str, "pitchshift="))
              {
                part->cur_pitchshift = strtol (strchr (str, '=') + 1, NULL, 10);
              }
            else if (g_str_has_prefix (str, "pitchshift+="))
              {
                part->cur_pitchshift += strtol (strchr (str, '=') + 1, NULL, 10);
              }
            else if (g_str_has_prefix (str, "pitchshift-="))
              {
                part->cur_pitchshift -= strtol (strchr (str, '=') + 1, NULL, 10);
              }
            else if (g_str_has_prefix (str, "dotbeats="))
              {
                part->cur_dot_beats = strtod (strchr (str, '=') + 1, NULL);
              }
            else if (g_str_has_prefix (str, "pause="))
              {
                part->cur_beat += strtod (strchr (str, '=') + 1, NULL);
              }
            else if (g_str_has_prefix (str, "volumefactor="))
              {
                part->cur_volume_factor = strtod (strchr (str, '=') + 1, NULL);
              }
            else if (g_str_has_prefix (str, "gracevolumescale="))
              {
                grace_volume_scale = strtod (strchr (str, '=') + 1, NULL);
              }
            else if (g_str_has_prefix (str, "gracetimescale="))
              {
                grace_time_scale = strtod (strchr (str, '=') + 1, NULL);
              }
            else if (g_str_has_prefix (str, "gracedurationscale="))
              {
                grace_duration_scale = strtod (strchr (str, '=') + 1, NULL);
              }
            else if (g_str_has_prefix (str, "tuning="))
              {
                const char *name = strchr (str, '=') + 1;
                char *n = g_strndup (name, end - name);
                BbTuning *tuning;
                g_strstrip (n);
                tuning = bb_tuning_get (n);
                if (tuning == NULL)
                  g_error ("no tuning named '%s' found", n);
                g_free (n);
                part->cur_tuning = tuning;
              }
            else
              {
                g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                             "unknown processing directive %.*s",
                             (end + 1) - (str - 1),
                             str - 1);
                return NULL;
              }
            str = end + 1;
            break;
          }
        case ',':
          part->cur_beat += part->cur_comma_step;
          str++;
          break;

        default:
          g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                       "invalid character '%c' in score-string", *str);
          return NULL;
          }
      if (part->cur_beat > part->max_beat)
        part->max_beat = part->cur_beat;
      if (part->cur_beat < part->min_beat)
        part->min_beat = part->cur_beat;
    }
  return part;
}

double
bb_score_get_time_from_beat (BbScore *score,
                             double   beat)
{
  guint br;
  if (score->n_beats == 0)
    {
      g_warning ("no beats found");	/* this often happens, and it's pretty much always an error */
      return 0;
    }
  for (br = 0; br < score->n_beats; br++)
    if (beat < (double) score->beats[br].first_beat + score->beats[br].n_beats)
      break;
  if (br == score->n_beats)
    {
      BbScoreBeat *last = score->beats + score->n_beats - 1;
      gdouble last_end_time = last->first_beat_time + last->n_seconds;
      gdouble last_end_beat = last->first_beat + last->n_beats;
      return last_end_time + (beat - last_end_beat) * last->end_beat_period;
    }
  else
    {
      gdouble beat_inset = beat - score->beats[br].first_beat;
      gdouble n_beats = score->beats[br].n_beats;
      gdouble start_bp = score->beats[br].start_beat_period;
      gdouble end_bp = score->beats[br].end_beat_period;
      gdouble delta_bp = end_bp - start_bp;
      gdouble start_time = score->beats[br].first_beat_time;
      switch (score->beats[br].mode)
        {
        case BB_BEAT_CONSTANT:
          return start_time + beat_inset * start_bp;
        case BB_BEAT_LINEAR:
          return start_time
               + beat_inset * start_bp
               + 0.5 * beat_inset * beat_inset * delta_bp / n_beats;
        case BB_BEAT_GEOMETRIC:
          {
            if (start_bp * 0.9999 <= end_bp && end_bp <= start_bp * 1.0001)
              {
                gdouble bp = (start_bp + end_bp) * 0.5;
                return start_time + beat_inset * bp;
              }
            else
              {
                /* dt/dbi = sbp * (ebp/sbp)^(bi/nb);
                   let alpha = (ebp/sbp)^(1/nb).
                   then dt/dbi = sbp * alpha^bi.
                   Integral(sbp * alpha^bi, bi) = sbp * alpha^bi / log(alpha).
                   So alpha must not be near 1. */
                gdouble alpha = end_bp / start_bp;
                return start_bp * n_beats / log (alpha)
                     * (exp (beat_inset * log (alpha) / n_beats) - 1.0)
                     + start_time;
              }
          }
        case BB_BEAT_HARMONIC:
          {
            if (start_bp * 0.9999 <= end_bp && end_bp <= start_bp * 1.0001)
              {
                gdouble bp = (start_bp + end_bp) * 0.5;
                return start_time + beat_inset * bp;
              }
            else
              {
                /* dt/dbi = 1 / ((1/ebp - 1/sbp) * (bi/nb) + (1/sbp)).
                   Let alpha = (1/ebp - 1/sbp) / nb;  beta = 1/sbp;
                   dt/dbi = 1 / (alpha * bi + beta).
                   Integral(1 / (alpha * bi + beta), bi) = (1/alpha) * log (alpha * bi + beta).
                   t(bi) = (1/alpha) * (log (alpha * bi + beta) - log(beta)). */
                gdouble alpha = ((1.0 / end_bp) - (1.0 / start_bp)) / n_beats;
                gdouble beta = 1.0 / start_bp;
                double rv = (log (alpha * beat_inset + beta) - log (beta)) / alpha + start_time;
                return rv;
              }
          }
        default:
          g_error ("unexpected beat-mode %u", score->beats[br].mode);
        }
    }
  g_return_val_if_reached (0);
}

double
bb_score_get_beat_from_time (BbScore *score,
                             double   time)
{
  guint br;
  gdouble time_inset;
  gdouble start_bp, end_bp, delta_bp;
  gdouble first_beat, n_beats;
  for (br = 0; br < score->n_beats; br++)
    if (time < score->beats[br].first_beat_time + score->beats[br].n_seconds)
      break;
  if (score->n_beats == 0)
    {
      g_warning ("no beats found [in bb_score_get_beat_from_time]");
      return 0;
    }
  if (br == score->n_beats)
    {
      /* approximate it as constant */
      BbScoreBeat *last = score->beats + score->n_beats - 1;
      gdouble after = time - (last->first_beat_time + last->n_seconds);
      return last->first_beat + last->n_beats + after / last->end_beat_period;
    }
  time_inset = time - score->beats[br].first_beat_time;
  start_bp = score->beats[br].start_beat_period;
  end_bp = score->beats[br].end_beat_period;
  delta_bp = end_bp - start_bp;
  first_beat = score->beats[br].first_beat;
  n_beats = score->beats[br].n_beats;
  if (-1e-6 * start_bp <= delta_bp && delta_bp <= 1e-6 * start_bp)
    {
      /* start_bp == end_bp, approximately,
         so approximate the thing as constant */
      gdouble bp = 0.5 * (start_bp + end_bp);
      return first_beat + time_inset / bp;
    }
  else
    switch (score->beats[br].mode)
      {
      case BB_BEAT_CONSTANT:
        return time_inset / start_bp + first_beat;
      case BB_BEAT_LINEAR:
        {
          /* time_inset = beat_inset * s + 0.5*beat_inset^2 * d / n_beats.
             You can use the quadratic formula to see: */
          double nd = delta_bp / n_beats;
          double s = start_bp;
          double beat_inset = (-s + sqrt(s*s + 2.0 * time_inset * nd)) / nd;
          return beat_inset + first_beat;
        }
      case BB_BEAT_GEOMETRIC:
        {
          /* inverting the equ from bb_score_get_time_from_beat(),
             which was:
               t(bi) = sbp * n_beats / log(alpha) * (exp(bi * log(alpha) / n_beats) - 1).
             we get:
               log (t log(alpha) / (sbp * n_beats) + 1) = bi * log(alpha) / n_beats

            If t is near 0, then it's unstable to compute directly:
            use the start-beat-period and approximate.

            otherwise, bi = log(t * log(alpha) / sbp + 1) / log(alpha). */
          gdouble alpha = end_bp / start_bp;
          gdouble tmp = time_inset * log (alpha) / (start_bp * n_beats);
          gdouble beat_inset;
          if (-1e-6 <= tmp && tmp <= 1e-6)
            beat_inset = time_inset / start_bp;
          else
            beat_inset = log (tmp + 1.0) * n_beats / log (alpha);
          return beat_inset + first_beat;
        }

      case BB_BEAT_HARMONIC:
        {
          /* inverting the equ from bb_score_get_time_from_beat(),
             which was:
                t = (1/alpha) * (log (alpha * bi + beta) - log(beta));
                (t*alpha + log(beta)) = log (alpha * bi + beta);
                (exp(t*alpha + log(beta)) - beta) / alpha = bi. */
          gdouble alpha = ((1.0 / end_bp) - (1.0 / start_bp)) / n_beats;
          gdouble beta = 1.0 / start_bp;
          return (exp (time_inset * alpha + log (beta)) - beta) / alpha + score->beats[br].first_beat;
        }

      default:
        g_assert_not_reached ();
      }
  g_return_val_if_reached (0);
}

gdouble
bb_score_get_beat_period_from_beat (BbScore      *score,
                                    gdouble       beat)
{
  guint br;
  BbScoreBeat *b;
  gdouble frac;
  for (br = 0; br < score->n_beats; br++)
    if (beat < (double) score->beats[br].first_beat + score->beats[br].n_beats)
      break;
  if (score->n_beats == 0)
    {
      g_warning ("no beats found [in bb_score_get_beat_period_from_beat]");
      return 0;
    }
  if (br == score->n_beats)
    return score->beats[br-1].end_beat_period;
  b = score->beats + br;

  /* fraction of the number of beats we are through the BbScoreBeat */
  frac = (beat - b->first_beat) / b->n_beats;

  switch (b->mode)
    {
    case BB_BEAT_CONSTANT:
      return b->start_beat_period;

    case BB_BEAT_LINEAR:
      if (beat > b->first_beat + b->n_beats) 
        return b->end_beat_period;
      return frac * (b->end_beat_period - b->start_beat_period)
           + b->start_beat_period;

    case BB_BEAT_GEOMETRIC:
      return b->start_beat_period
           * pow (b->end_beat_period / b->start_beat_period, frac);

    case BB_BEAT_HARMONIC:
      {
        gdouble inv_start = 1.0 / b->start_beat_period;
        gdouble inv_end = 1.0 / b->end_beat_period;
        return 1.0 / (inv_start + frac * (inv_end - inv_start));
      }

    default:
      g_assert_not_reached ();
    }
  g_return_val_if_reached (0);
}

gdouble
bb_score_get_beat_period_from_time (BbScore      *score,
                                    gdouble       time)
{
  return bb_score_get_beat_period_from_beat (score, bb_score_get_beat_from_time (score, time));
}

static void
dump_param (GParamSpec *pspec,
            const GValue *value)
{
  if (G_VALUE_HOLDS_DOUBLE (value))
    g_printerr ("\t%s => %.6f\n", pspec->name, g_value_get_double (value));
  else if (G_VALUE_TYPE (value) == BB_TYPE_DURATION)
    {
      BbDuration dur;
      gdouble v;
      const char *str;
      bb_value_get_duration (value, &dur);
      v = dur.value;
      switch (dur.units)
        {
        case BB_DURATION_UNITS_SECONDS:
          str = "s";
          break;
        case BB_DURATION_UNITS_BEATS:
          str = "b";
          break;
        case BB_DURATION_UNITS_SAMPLES:
          str = "samps";
          break;
        case BB_DURATION_UNITS_NOTE_LENGTHS:
          v *= 100;
          str = "%";
          break;
        default:
          g_assert_not_reached ();
        }
      g_printerr ("\t%s => %.6f%s\n", pspec->name, v, str);
    }
}

void
bb_score_part_dump (BbScorePart *part)
{
  guint j,p;
  for (j = 0; j < part->n_notes; j++)
    {
      g_printerr ("Part %p, note %u, instrument %s, start_beat=%.6f\n",
		  part,j,part->instrument->name,
		  part->notes[j].start);
      g_printerr ("\tvolume => %.6f\n", part->notes[j].volume);
      for (p = 0; p < part->instrument->n_param_specs; p++)
	dump_param (part->instrument->param_specs[p],
		    part->notes[j].values + p);
    }
}

void
bb_score_dump (BbScore *score)
{
  guint i;
  for (i = 0; i < score->n_parts; i++)
    bb_score_part_dump (score->parts[i]);
}

static void
free_note_array (BbScorePart *part)
{
  guint j, p;
  for (j = 0; j < part->n_notes; j++)
    {
      for (p = 0; p < part->instrument->n_param_specs; p++)
	g_value_unset (part->notes[j].values + p);
      g_free (part->notes[j].values);
    }
  g_free (part->notes);
}

void     bb_score_free               (BbScore      *score)
{
  guint i;
  for (i = 0; i < score->n_parts; i++)
    {
      BbScorePart *part = score->parts[i];
      free_note_array (part);
      g_object_unref (part->instrument);
      g_free (part);
    }
  g_free (score->parts);
  g_free (score->beats);
  g_hash_table_destroy (score->marks);
  g_free (score);
}

/* --- Score Part Manipulation --- */
static void
bb_score_note_copy  (BbInstrument *instrument,
                     BbScoreNote  *dst,
                     BbScoreNote  *src)
{
  guint p;
  dst->values = g_new0 (GValue, instrument->n_param_specs);
  dst->volume = src->volume;
  dst->start = src->start;
  dst->type = src->type;
  for (p = 0; p < instrument->n_param_specs; p++)
    {
      GValue *sv = src->values + p;
      GValue *dv = dst->values + p;
      g_value_init (dv, G_VALUE_TYPE (sv));
      g_value_copy (sv, dv);
    }
  switch (src->type)
    {
    case BB_SCORE_NOTE_SIMPLE:
      dst->events = NULL;
      dst->n_events = 0;
      break;
    case BB_SCORE_NOTE_COMPLEX:
      {
	guint e;
	dst->events = g_new (BbInstrumentEvent, src->n_events + 1);
	dst->n_events = src->n_events;
	dst->events[dst->n_events].time = src->events[dst->n_events].time;
	for (e = 0; e < dst->n_events; e++)
	  {
	    BbInstrumentEvent *src_ev = &src->events[e];
	    BbInstrumentEvent *dst_ev = &dst->events[e];
	    dst_ev->type = src_ev->type;
	    dst_ev->time = src_ev->time;
	    switch (src->events[e].type)
	      {
	      case BB_INSTRUMENT_EVENT_PARAM:
		dst_ev->info.param.index = src_ev->info.param.index;
		memset (&dst_ev->info.param.value, 0, sizeof (GValue));
		g_value_init (&dst_ev->info.param.value, G_VALUE_TYPE (&src_ev->info.param.value));
		g_value_copy (&src_ev->info.param.value, &dst_ev->info.param.value);
		break;

	      case BB_INSTRUMENT_EVENT_ACTION:
		{
		  BbInstrumentActionSpec *aspec;
		  guint ai;
		  dst_ev->info.action.index = src_ev->info.action.index;
		  aspec = instrument->actions[src_ev->info.action.index];
		  dst_ev->info.action.args = g_new0 (GValue, aspec->n_args);
		  for (ai = 0; ai < aspec->n_args; ai++)
		    {
		      g_value_init (dst_ev->info.action.args + ai, G_VALUE_TYPE (src_ev->info.action.args + ai));
		      g_value_copy (src_ev->info.action.args + ai, dst_ev->info.action.args + ai);
		    }
		  break;
		}

	      default:
		g_assert_not_reached ();
	      }
	 }
       }
      break;
    default:
      g_assert_not_reached ();
    }
}

void     bb_score_part_repeat(BbScorePart  *part,
                              guint         reps)
{
  guint r,i;
  BbScoreNote *new_notes = g_new (BbScoreNote, part->n_notes * reps);
  gdouble n_beats = part->max_beat - part->first_beat;
  double max_beat = part->max_beat;
  for (r = 0; r < reps; r++)
    for (i = 0; i < part->n_notes; i++)
      {
        BbScoreNote *dst = new_notes + i + r * part->n_notes;
        BbScoreNote *src = part->notes + i;
        bb_score_note_copy  (part->instrument, dst, src);
	dst->start = src->start + n_beats * r;
        if (dst->start > max_beat)
          max_beat = dst->start;
      }
  free_note_array (part);
  part->n_notes *= reps;
  part->notes = new_notes;
  part->max_beat = max_beat;
  //part->part_n_beats *= reps;
}
BbScorePart * bb_score_copy_part (BbScore        *score,
                                  BbScorePart    *part,
                                  gdouble         first_beat)
{
  BbScorePart *rv = bb_score_add_part (score, part->instrument, first_beat);
  BbScoreNote *new_notes = add_notes_raw (rv, part->n_notes);
  gdouble delta_beats = first_beat - part->first_beat;
  guint i;
  rv->min_beat = part->min_beat + delta_beats;
  rv->max_beat = part->max_beat + delta_beats;
  for (i = 0; i < part->n_notes; i++)
    {
      bb_score_note_copy (part->instrument, new_notes + i, part->notes + i);
      new_notes[i].start += delta_beats;
    }
  return rv;
}
