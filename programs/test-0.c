#include "bb-score.h"
#include "bb-instrument-builtins.h"


int main()
{
  BbScore *score;
  GError *error = NULL;
  g_type_init ();
  score = bb_score_new ();
  bb_score_add_beats (score, 100, 2.0 / 3.0);
  if (!bb_score_add_part_as_string (score, bb_instrument_adsr_sin_peek (), 0,
                                    "{duration=0.5} <notestep=0.5> cecegg _ abba\n"
                                    "{duration=1.0} _ a e a d\n"
                                   , &error))
    g_error ("error parsing part: %s", error->message);
  if (!bb_score_render_wavfile (score, 44100, "test-0.wav", &error))
    g_error ("error rendering score: %s", error->message);
  bb_score_dump (score);
  bb_score_free (score);
  return 0;
}
