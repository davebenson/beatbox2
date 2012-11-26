#include "bb-script.h"
#include "bb-error.h"
#include "bb-input-file.h"
#include "macros.h"
#include <gsk/gskmacros.h>
#include <gsk/gskghelpers.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

typedef enum
{
  BB_SCORE_SYNC_AT_BEAT,
  BB_SCORE_SYNC_AT_PART_START,
  BB_SCORE_SYNC_AT_PART_END,
} BbScoreSyncMode;

#define File              BbInputFile
#define file_readline     bb_input_file_advance 
#define file_fpopen       bb_input_file_new_from_fp
#define file_close        bb_input_file_close

static char *
substitute_macros (const char *input,
                   GHashTable *macros)
{
  gboolean must_rerun = FALSE;
  GString *str = g_string_new ("");
  for (;;)
    {
      const char *end = strchr (input, '@');
      const char *macro_name_start;
      char *macro_name;
      const char *macro_value;

      if (end == NULL)
        {
          g_string_append (str, input);
          break;
        }
      g_string_append_len (str, input, end - input);
      input = end + 1;		/* skip '@' */
      macro_name_start = input;
      GSK_SKIP_CHAR_TYPE (input, IS_IDENTIFIER_CHAR);
      if (macro_name_start == input)
        g_error ("expected identifier macro name after '@'");
      macro_name = g_strndup (macro_name_start, input - macro_name_start);
      macro_value = g_hash_table_lookup (macros, macro_name);
      if (macro_value == NULL)
        g_error ("no macro named %s (@%s) found", macro_name, macro_name);
      must_rerun = must_rerun || (strchr (macro_value, '@') != NULL);
      g_free (macro_name);
      g_string_append (str, macro_value);
    }
  if (must_rerun)
    {
      char *rv = substitute_macros (str->str, macros);
      g_string_free (str, TRUE);
      return rv;
    }
  return g_string_free (str, FALSE);
}


BbScore *
bb_script_execute_file (File *f, BbScoreRenderConfig *config, GError **error)
{
  BbScore *score = bb_score_new ();
  GHashTable *macros = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  while (!f->eof)
    {
      const char *at;
      if (f->line[0] == '#' || f->line[0] == 0)
	{
	  file_readline (f);
	  continue;
	}
      if (f->line[0] != '=')
	{
	  g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                       "only section headers allowed (line %u)",
                       f->line_no);
          return FALSE;
        }
      at = f->line + 1;
      GSK_SKIP_WHITESPACE (at);

      if (g_str_has_prefix (at, "instrument")
       && g_ascii_isspace (at[strlen ("instrument")]))
        {
          const char *start, *end;
          char *name;
          BbInstrument *instr;
          at += strlen ("instrument");
          GSK_SKIP_WHITESPACE (at);
          start = at;
          GSK_SKIP_NONWHITESPACE (at);
          end = at;
          name = g_strndup (start, end - start);

          GSK_SKIP_WHITESPACE (at);
          if (*at == 0)
            {
              /* scarf up lines until one that starts with '=' is found */
              GString *buffer = g_string_new ("");
              file_readline (f);
              while (!f->eof)
                {
                  const char *at = f->line;
                  GSK_SKIP_WHITESPACE (at);
                  if (*at == 0 || *at == '#')
                    {
                      file_readline (f);
                      continue;
                    }
                  if (*at == '=')
                    break;
                  g_string_append (buffer, at);
                  file_readline (f);
                }
              instr = bb_instrument_from_string (buffer->str);
              g_string_free (buffer, TRUE);
            }
          else
            {
              instr = bb_instrument_from_string (at);
              file_readline(f);
            }
          if (instr->name != NULL)
            g_error ("instrument %s already named %s",
                     name, instr->name);
          bb_instrument_set_name (instr, name);
          g_free (name);
        }
      else if (strncmp (at, "part", 4) == 0
       && g_ascii_isspace (at[4]))
        {
          char *name = NULL;
          BbInstrument *instrument = NULL;
          BbScoreSyncMode mode;
          BbScorePart *part_arg=0;
          double mode_arg=0;
          //double anchor = 0;
          GString *part_str = g_string_new ("");
          char *real_str;
          BbScorePart *part;
          guint repeat = 1;
          if (score->n_parts == 0)
            {
              mode = BB_SCORE_SYNC_AT_BEAT;
              mode_arg = 0;
            }
          else
            {
              mode = BB_SCORE_SYNC_AT_PART_END;
              part_arg = score->parts[score->n_parts - 1];
            }

          /* suck up arguments to part creation */
          at += 4;
          for (;;)
            {
              GSK_SKIP_WHITESPACE (at);
              if (*at == 0 || *at == '#')
                break;
              if (g_str_has_prefix (at, "instrument"))
                {
                  const char *end;
                  char *instr;
                  at += strlen ("instrument");
                  GSK_SKIP_WHITESPACE (at);
                  end = bb_instrument_string_scan (at);
                  instr = g_strndup (at, end - at);
                  instrument = bb_instrument_from_string (instr);
                  if (instrument == NULL)
                    {
                      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                                   "error parsing instrument from string '%s'",
                                   instr);
                      return FALSE;
                    }
                  g_free (instr);
                  at = end;
                }
              else if (g_str_has_prefix (at, "named"))
                {
                  const char *start;
                  at += 5;
                  GSK_SKIP_WHITESPACE (at);
                  if (*at == 0)
                    {
                      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                                   "missing arg after named");
                      return FALSE;
                    }
                  start = at;
                  GSK_SKIP_NONWHITESPACE (at);
                  name = g_strndup (start, at - start);
                }
              else if (g_str_has_prefix (at, "repeat"))
                {
                  char *end;
                  at += 6;
                  GSK_SKIP_WHITESPACE (at);
                  repeat = strtoul (at, &end, 10);
                  if (at == end)
                    {
                      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                                   "error parsing repeat count");
                      return FALSE;
                    }
                  at = end;
                }
              else if (g_str_has_prefix (at, "at"))
                {
                  at += 2;
                  GSK_SKIP_WHITESPACE (at);
                  if (g_str_has_prefix (at, "beat"))
                    {
                      char *end;
                      at += 4;
                      GSK_SKIP_WHITESPACE (at);
                      mode_arg = strtod (at, &end);
                      if (at == end)
                        {
                          g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                                       "error parsing beat number");
                          return FALSE;
                        }
                      mode = BB_SCORE_SYNC_AT_BEAT;
                      at = end;
                    }
                  else if (g_str_has_prefix (at, "part"))
                    {
                      at += 4;
                      GSK_SKIP_WHITESPACE (at);
                      if (g_str_has_prefix (at, "start"))
                        {
                          mode = BB_SCORE_SYNC_AT_PART_START;
                          at += 5;
                        }
                      else if (g_str_has_prefix (at, "end"))
                        {
                          mode = BB_SCORE_SYNC_AT_PART_END;
                          at += 3;
                        }
                      else
                        {
                          g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                                       "part must be followed by 'start' or 'end'");
                          return FALSE;
                        }
                      GSK_SKIP_WHITESPACE (at);
                      if (g_ascii_isdigit (*at))
                        {
                          /* a part number */
                          char *end;
                          guint part_index = strtoul (at, &end, 10);
                          if (at == end)
                            {
                              g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                                           "error parsing part number");
                              return FALSE;
                            }
                          if (part_index >= score->n_parts)
                            {
                              g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                                           "score part out-of-range (part_arg=%u, score->n_parts=%u)",
                                           part_index, score->n_parts);
                              return FALSE;
                            }
                          part_arg = score->parts[part_index];
                        }
                      else
                        {
                          /* a named part */
                          const char *end = at;
                          char *n;
                          GSK_SKIP_NONWHITESPACE (end);
                          n = g_strndup (at, end - at);
                          part_arg = bb_score_get_part_by_name (score, n);
                          if (part_arg == NULL)
                            {
                              g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                                           "no part named '%s'", n);
		              g_free (n);
                              return FALSE;
                            }
                          g_free (n);
                          at = end;
                        }
                    }
                  else if (g_str_has_prefix (at, "mark") && isspace (at[4]))
                    {
                      const char *start;
                      char *mark;
                      at += 4;
                      GSK_SKIP_WHITESPACE (at);
                      start = at;
                      GSK_SKIP_NONWHITESPACE (at);
                      mark = g_strndup (start, at - start);
                      if (!bb_score_get_mark (score, mark, &mode_arg))
                        {
                          g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                                       "no mark named '%s'", mark);
                          g_free (mark);
                          return FALSE;
                        }
                      mode = BB_SCORE_SYNC_AT_BEAT;
                      g_free (mark);
                    }
                  else
                    {
                      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                                   "the only 'at' directives known are 'beat', 'mark' and 'part'");
                      return FALSE;
                    }
                }
              else
                {
                  g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                               "error parsing =part at %.6s...", at);
                  return FALSE;
                }
            }

          if (instrument == NULL && score->n_parts == 0)
            {
              g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                           "first part does not have an instrument");
              return FALSE;
            }
          if (instrument == NULL)
            instrument = g_object_ref (score->parts[score->n_parts - 1]->instrument);

          /* read note-string */
          file_readline (f);
          while (!f->eof && f->line[0] != '=')
            {
	      if (f->line[0] != '#')
                {
                  g_string_append (part_str, f->line);
                  g_string_append_c (part_str, '\n');
                }
              file_readline (f);
            }

          double beat;
          switch (mode)
            {
            case BB_SCORE_SYNC_AT_BEAT:
              beat = mode_arg;
              break;
            case BB_SCORE_SYNC_AT_PART_START:
              beat = part_arg->first_beat;
              break;
            case BB_SCORE_SYNC_AT_PART_END:
              beat = part_arg->max_beat;
              break;
            default:
              g_assert_not_reached ();
            }

          real_str = substitute_macros (part_str->str, macros);
          part = bb_score_add_part_as_string (score, instrument, beat, real_str, error);
          if (part == NULL)
            {
              return FALSE;
            }
          if (repeat > 1)
            bb_score_part_repeat (part, repeat);
          g_string_free (part_str, TRUE);
          g_free (real_str);
          g_object_unref (instrument);

          if (name != NULL)
            {
              bb_score_part_set_name (part, name);
              g_free (name);
            }

          continue;
        }
      else if (strncmp (at, "beats", 5) == 0
       && g_ascii_isspace (at[5]))
        {
          /* usage: beats NBEATS BPM */
          BbBeatMode mode = BB_BEAT_CONSTANT;
          at += 5;
          GSK_SKIP_WHITESPACE (at);
          if (strncmp (at, "linear", 6) == 0)
            {
              at += 6;
              mode = BB_BEAT_LINEAR;
            }
          else if (strncmp (at, "constant", 7) == 0)
            {
              at += 7;
              mode = BB_BEAT_CONSTANT;
            }
          else if (strncmp (at, "geometric", 9) == 0)
            {
              at += 9;
              mode = BB_BEAT_GEOMETRIC;
            }
          else if (strncmp (at, "harmonic", 8) == 0)
            {
              at += 8;
              mode = BB_BEAT_HARMONIC;
            }
          GSK_SKIP_WHITESPACE (at);
          if (mode == BB_BEAT_CONSTANT)
            {
              gdouble n_beats, bpm;
              if (sscanf (at, "%lf %lf", &n_beats, &bpm) != 2)
                {
                  g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                               "error parsing 'beats' directive (mode==constant)");
                  return NULL;
                }
              bb_score_add_beats (score, n_beats, 60.0 / bpm);
            }
          else
            {
              gdouble n_beats, start_bpm, end_bpm;
              if (sscanf (at, "%lf %lf %lf", &n_beats, &start_bpm, &end_bpm) != 3)
                {
                  g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                               "error parsing 'beats' directive");
                  return NULL;
                }
              bb_score_add_beats_variable (score, n_beats, mode, 60.0 / start_bpm, 60.0 / end_bpm);
            }

          file_readline (f);
        }
      else if (strncmp (at, "paste", 5) == 0
       && g_ascii_isspace (at[5]))
        {
          const char *start;
          char *name;
          BbScorePart *part;
          BbScorePart *new_part;
          gdouble end_beat;
          at += 5;
          GSK_SKIP_WHITESPACE (at);
          start = at;
          GSK_SKIP_NONWHITESPACE (at);
          name = g_strndup (start, at-start);
          part = bb_score_get_part_by_name (score, name);
          if (part == NULL)
            g_error ("no part '%s' found", name);
          g_free (name);
          if (score->n_parts == 0)
            end_beat = 0;
          else
            end_beat = score->parts[score->n_parts-1]->max_beat;
          new_part = bb_score_copy_part (score, part, end_beat);
          GSK_SKIP_WHITESPACE (at);
          while (*at)
            {
	      if (memcmp (at, "named", 5) == 0 && isspace (at[5]))
		{
		  at += 5;
		  GSK_SKIP_WHITESPACE (at);
		  start = at;
		  GSK_SKIP_NONWHITESPACE (at);
		  name = g_strndup (start, at-start);
		  bb_score_part_set_name (new_part, name);
		  g_free (name);
		  GSK_SKIP_WHITESPACE (at);
		}
              else
                g_error ("garbage at end of 'paste' directive (%s)", at);
            }
          file_readline (f);
        }
      else if (strncmp (at, "write", 5) == 0
       && g_ascii_isspace (at[5]))
        {
          const char *end;
          char *fname;
          at += 5;
          GSK_SKIP_WHITESPACE (at);
          end = at;
          GSK_SKIP_NONWHITESPACE (end);
          fname = g_strndup (at, end - at);
          if (!bb_score_render_wavfile (score, config, fname, FALSE, error))
            return NULL;
          g_printerr ("Wrote '%s'.\n", fname);
          g_free (fname);
          file_readline (f);
        }
      else if (strncmp (at, "macro", 5) == 0
       && g_ascii_isspace (at[5]))
        {
          const char *macro_name_start, *macro_name_end;
          at += 5;
          GSK_SKIP_WHITESPACE (at);
          macro_name_start = at;
          GSK_SKIP_CHAR_TYPE (at, IS_IDENTIFIER_CHAR);
          macro_name_end = at;
          GSK_SKIP_WHITESPACE (at);
          g_hash_table_insert (macros, g_strndup (macro_name_start, macro_name_end - macro_name_start), g_strdup (at));
          file_readline (f);
        }
      else if (strncmp (at, "long_macro", 10) == 0
       && g_ascii_isspace (at[10]))
        {
          const char *macro_name_start, *macro_name_end;
          GString *contents = g_string_new ("");
          char *macro_name;
          at += 10;
          GSK_SKIP_WHITESPACE (at);
          macro_name_start = at;
          GSK_SKIP_CHAR_TYPE (at, IS_IDENTIFIER_CHAR);
          macro_name_end = at;
          macro_name = g_strndup (macro_name_start, macro_name_end - macro_name_start);
          GSK_SKIP_WHITESPACE (at);
          if (*at != 0)
            {
              g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                           "garbage after =long_macro %s", macro_name);
              g_free (macro_name);
              return FALSE;
            }
          file_readline (f);
          while (!f->eof && f->line[0] != '=')
            {
              g_string_append (contents, f->line);
              g_string_append_c (contents, '\n');
              file_readline (f);
            }
          g_message ("defining @%s to be %s", macro_name,contents->str);
          g_hash_table_insert (macros, macro_name, g_string_free (contents, FALSE));
        }
      else if (strncmp (at, "set", 3) == 0
       && g_ascii_isspace (at[3]))
        {
          const char *start, *end;
          char *key, *value;
          at += 3;
          GSK_SKIP_WHITESPACE (at);
          start = at;
          GSK_SKIP_NONWHITESPACE (at);
          end = at;
          key = g_strndup (start, end - start);
          GSK_SKIP_WHITESPACE (at);
          if (*at == 0)
            value = NULL;
          else
            {
              start = at;
              GSK_SKIP_NONWHITESPACE (at);
              end = at;
              value = g_strndup (start, end - start);
            }
          if (strcmp (key, "gnuplot") == 0)
            {
              bb_score_gnuplot_notes = TRUE;
            }
          else
            {
              g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                           "unknown option %s", key);
              return NULL;
            }
          g_free (key);
          g_free (value);
          file_readline (f);
        }
      else
        {
          g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                       "bad line %s", f->line);
          bb_score_free (score);
          return NULL;
        }
    }
  g_hash_table_destroy (macros);
  return score;
}

BbScore *
bb_script_execute (const char *filename,
                   BbScoreRenderConfig *config,
                   GError **error)
{
  FILE *fp = fopen (filename, "r");
  File *file;
  BbScore *score;
  if (fp == NULL)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_FILE_READ,
                   "error opening %s: %s", filename, g_strerror (errno));
      return NULL;
    }
  file = file_fpopen (fp);
  score = bb_script_execute_file (file, config, error);
  if (score == NULL)
    {
      gsk_g_error_add_prefix (error, "at %s, line %u",
                              filename, file->line_no);
      file_close (file);
      return NULL;
    }
  file_close (file);
  return score;
}
