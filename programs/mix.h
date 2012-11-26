#include <glib.h>

void          mix_init                (int          *argc_inout,
                                       const char ***argv_inout,
			               gdouble       loop_duration);
guint         mix_add_sequence        (guint         length,
                                       gdouble      *samples);
void          mix_set_volume          (guint         sequence,
                                       gdouble       volume);
void          mix_set_volume_rate     (gdouble       full_changes_per_second);
void          mix_remove_sequence     (guint         sequence);

gdouble       mix_get_sampling_rate   (void);
guint         mix_get_n_samples       (void);
void          mix_get_time            (guint        *cur_sample,
                                       guint        *total_samples);
void          mix_sequence_get_time   (guint         sequence,
                                       guint        *cur_sample,
                                       guint        *offset,
                                       guint        *total_samples);
void          mix_sequence_set_offset (guint         sequence,
                                       guint         samples_offset);
