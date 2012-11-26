#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../core/bb-script.h"
#include "../core/bb-init.h"

void bb_vm_init ();

static void
usage (const char *prog)
{
  g_printerr ("usage: %s [-p] SCRIPT\n\n", prog);
  exit (1);
}

static BbScoreNoteFilterResult
is_from_named_part  (BbScorePart   *part,
                     guint          note_index,
                     gpointer       data)
{
  const char*name = data;
  if (part->name != NULL && strcmp (part->name, name) == 0)
    return BB_SCORE_NOTE_FILTER_ACCEPT;
  return BB_SCORE_NOTE_FILTER_REJECT;
}

int main(int argc, char **argv)
{
  const char *script = NULL;
  guint i;
  GError *error = NULL;
  gboolean do_playback = FALSE;
  BbScore *score;
  const char *wav_filename = NULL;
  gboolean do_dump = FALSE;
  gboolean do_dump_parts = FALSE;
  BbScoreRenderConfig *config;
  gboolean raw = FALSE;
  bb_init ();
  bb_vm_init ();
  config = bb_score_render_config_new ();
  for (i = 1; i < (guint) argc; i++)
    {
      if (argv[i][0] == '-')
	{
          if (strcmp (argv[i], "--g-fatal-warnings") == 0)
            {
              g_log_set_always_fatal (G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL);
            }
          else if (strcmp (argv[i], "-p") == 0
                || strcmp (argv[i], "--play") == 0)
            {
              do_playback = TRUE;
            }
          else if (g_str_has_prefix (argv[i], "--part="))
            {
              bb_score_render_config_add_filter (config,
                                                 is_from_named_part,
                                                 g_strdup (strchr (argv[i], '=') + 1),
                                                 g_free);
              bb_score_render_config_unset_start_time (config);
              bb_score_render_config_unset_end_time (config);
            }
          else if (strcmp (argv[i], "--raw") == 0)
            {
              raw = TRUE;
            }
          else if (strcmp (argv[i], "-r") == 0)
            {
              gdouble sampling_rate;
              ++i;
              g_assert (argv[i]);
              sampling_rate = g_strtod (argv[i], NULL);
              if (sampling_rate <= 0)
                g_error ("bad sampling-rate argument '%s' given", argv[i]);
              bb_score_render_config_set_sampling_rate (config, sampling_rate);
            }
          else if (strcmp (argv[i], "-o") == 0
           || strcmp (argv[i], "--output") == 0)
            {
              wav_filename = argv[++i];
              if (wav_filename == NULL)
                g_error ("%s requires filename argument",argv[i-1]);
            }
          else if (strcmp (argv[i], "-d") == 0
           || strcmp (argv[i], "--dump") == 0)
            {
              do_dump = TRUE;
            }
          else if (strcmp (argv[i], "--dump-parts") == 0)
            {
              do_dump_parts = TRUE;
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
    usage ("bb-run");
  score = bb_script_execute (script, config, &error);
  if (score == NULL)
    g_error ("error running %s: %s", script, error->message);
  if (do_dump)
    bb_score_dump (score);
  if (do_dump_parts)
    {
      guint p;
      for (p = 0; p < score->n_parts; p++)
        {
          BbScorePart *part = score->parts[p];
          const char *name = part->name ? part->name : "unnamed";
          g_message ("part %u: %s: min/maxbeats=%.6f,%.6f",
                     p, name, 
                     part->min_beat,part->max_beat);
        }
    }
  if (do_playback)
    {
      static guint seqno = 0;
      char *tmpwav = g_strdup_printf ("tmp-play-%u-%u-%u.wav",
                                      (guint)time(NULL),(guint)getpid(),
                                      seqno++);
      char *cmd;
      if (getenv ("BB_PLAYER") == NULL)
	cmd = g_strdup_printf ("mplayer %s >& /dev/null", tmpwav);
      else
	cmd = g_strdup_printf ("%s %s", getenv ("BB_PLAYER"), tmpwav);
      if (!bb_score_render_wavfile (score, config, tmpwav, TRUE, &error))
        g_error ("error writing tmp wavfile %s", error->message);
      if (system(cmd) != 0)
        g_warning ("playback failed");
      unlink (tmpwav);
      g_free (tmpwav);
    }
  if (wav_filename && raw)
    {
      if (!bb_score_render_raw (score, config, wav_filename, TRUE, &error))
        g_error ("error writing to rawfile: %s", error->message);
    }
  else if (wav_filename && !raw)
    {
      if (!bb_score_render_wavfile (score, config, wav_filename, TRUE, &error))
        g_error ("error writing to wavfile: %s", error->message);
    }
  bb_score_render_config_free (config);
  return 0;
}
