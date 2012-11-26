
#include <glib.h>

typedef struct _BbTuningNote BbTuningNote;
typedef struct _BbTuning BbTuning;

struct _BbTuningNote
{
  const char *note;
  gint key;
};

struct _BbTuning
{
  const char *name;

  /* mapping from note => key */
  guint n_notes;
  BbTuningNote *notes;

  /* mapping from key => frequency */
  guint start_key;
  guint n_keys;
  gdouble *key_freqs;
  gdouble octave_freq_factor;		/* usually 2 */
};


BbTuning   *bb_tuning_get      (const char *name);
void        bb_tuning_register (BbTuning *tuning);

gdouble  bb_tuning_get_freq     (BbTuning   *tuning,
                                 const char *note,
			         gint        octave,
			         GError    **error);
gboolean bb_tuning_get_key      (BbTuning   *tuning,
                                 const char *note,
			         gint        octave,
			         gint       *key_out,
			         GError    **error);
gboolean bb_tuning_get_key_len  (BbTuning   *tuning,
                                 const char *note,
                                 guint       note_len,
			         gint        octave,
			         gint       *key_out,
			         GError    **error);
gdouble  bb_tuning_key_to_freq  (BbTuning   *tuning,
                                 gint        key);

void _bb_tuning_register_defaults ();
