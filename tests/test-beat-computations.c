#include "../core/bb-score.h"

static const char *get_beat_mode_name (BbBeatMode mode)
{
  switch (mode)
    {
#define WRITE_CASE(shortname) case BB_BEAT_##shortname: return #shortname
    WRITE_CASE (CONSTANT);
    WRITE_CASE (LINEAR);
    WRITE_CASE (GEOMETRIC);
    WRITE_CASE (HARMONIC);
#undef WRITE_CASE
    }
  g_return_val_if_reached ("***unknown beat mode***");
}
static void log_last (BbScore *score)
{
  BbScoreBeat *beat = score->beats + score->n_beats - 1;
  g_message ("beat %u added: %s: n_beats=%.6f, n_seconds=%.6f, start,end beat period=%.6f,%.6f",
             score->n_beats - 1,
             get_beat_mode_name (beat->mode),
             beat->n_beats,
             beat->n_seconds,
             beat->start_beat_period,
             beat->end_beat_period);
}

static void add_beats (BbScore *score,
                       gdouble  n_beats,
                       gdouble  beat_period)
{
  bb_score_add_beats (score, n_beats, beat_period);
  log_last (score);
}

static void add_beats_variable (BbScore *score,
                       gdouble  n_beats,
                       BbBeatMode mode,
                       gdouble  start_beat_period,
                       gdouble  end_beat_period)
{
  bb_score_add_beats_variable (score, n_beats, mode, start_beat_period, end_beat_period);
  log_last (score);
}



int main()
{
  BbScore *score = bb_score_new ();
  gdouble beat;
  add_beats (score, 10, 0.5);
  add_beats_variable (score, 10, BB_BEAT_LINEAR, 0.5, 1.0);
  add_beats_variable (score, 10, BB_BEAT_GEOMETRIC, 1.0, 2.0);
  add_beats_variable (score, 10, BB_BEAT_HARMONIC, 2.0, 1.0);

  for (beat = 0; beat < 45; beat += 0.1)
    {
      gdouble secs = bb_score_get_time_from_beat (score, beat);
      gdouble b = bb_score_get_beat_from_time (score, secs);
      gdouble bp = bb_score_get_beat_period_from_beat (score, beat);
      gdouble delta = 0.001;
      gdouble tprime = bb_score_get_time_from_beat (score, beat + delta);
      gdouble cbp = (tprime - secs) / delta;                     /* calculacated instantaneous beat-period */
      gdouble error_beat = ABS (b - beat);
      gdouble error_beat_period = ABS (cbp - bp);
      g_assert (error_beat < 1e-6);
      g_assert (error_beat_period < 1e-4);
      g_message ("beat %.6f: time=%.6f, beat_period=%.6f", beat, secs, bp);
    }
  bb_score_free (score);
  return 0;
}
