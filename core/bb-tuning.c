#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "bb-error.h"
#include "bb-tuning.h"

static GHashTable *tunings_by_name;

BbTuning   *bb_tuning_get      (const char *name)
{
  if (tunings_by_name == NULL)
    _bb_tuning_register_defaults ();
  return g_hash_table_lookup (tunings_by_name, name);
}

static gint
compare_notes_by_strcmp (gconstpointer a, gconstpointer b)
{
  const BbTuningNote *na = a;
  const BbTuningNote *nb = b;
  return strcmp (na->note, nb->note);
}

void        bb_tuning_register (BbTuning *tuning)
{
  if (tunings_by_name == NULL)
    tunings_by_name = g_hash_table_new (g_str_hash, g_str_equal);
  g_return_if_fail (g_hash_table_lookup (tunings_by_name, tuning->name) == NULL);

  qsort (tuning->notes, tuning->n_notes, sizeof (BbTuningNote),
         compare_notes_by_strcmp);
  g_hash_table_insert (tunings_by_name, (gpointer)tuning->name, tuning);
}

gdouble  bb_tuning_get_freq     (BbTuning   *tuning,
                                 const char *note,
			         gint        octave,
			         GError    **error)
{
  gint key;
  if (!bb_tuning_get_key (tuning, note, octave, &key, error))
    return 0;
  return bb_tuning_key_to_freq (tuning, key);
}

gboolean bb_tuning_get_key      (BbTuning   *tuning,
                                 const char *note,
			         gint        octave,
			         gint       *key_out,
			         GError    **error)
{
  BbTuningNote dummy = { note, 0 };
  BbTuningNote *tnote = bsearch (&dummy,
                                 tuning->notes, tuning->n_notes, sizeof (BbTuningNote),
                                 compare_notes_by_strcmp);
  if (tnote == NULL)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                   "unknown note '%s' in tuning '%s'", note, tuning->name);
      return FALSE;
    }
  *key_out = tnote->key + octave * tuning->n_keys;
  return TRUE;
}
gboolean bb_tuning_get_key_len  (BbTuning   *tuning,
                                 const char *note,
                                 guint       note_len,
			         gint        octave,
			         gint       *key_out,
			         GError    **error)
{
  char *n = g_alloca (note_len + 1);
  memcpy (n, note, note_len);
  n[note_len] = 0;
  return bb_tuning_get_key (tuning, n, octave, key_out, error);
}

gdouble  bb_tuning_key_to_freq  (BbTuning   *tuning,
                                 gint        key)
{
  gint mid_rel_key = key - tuning->start_key;
  gint scale_offset = mid_rel_key % (gint)tuning->n_keys;
  gint octave_offset;
  gdouble freq;
  if (scale_offset < 0)
    scale_offset += tuning->n_keys;
  octave_offset = (mid_rel_key - scale_offset) / (gint)tuning->n_keys;
  freq = tuning->key_freqs[scale_offset];
  if (octave_offset)
    freq *= pow (tuning->octave_freq_factor, octave_offset);
  return freq;
}
