#include <string.h>
#include "bb-duration.h"
#include <gobject/gvaluecollector.h>

gboolean bb_duration_parse_units (const char *str,
                                  BbDurationUnits *units_out)
{
  switch (str[0])
    {
    case 'b':
      if (strcmp (str, "b") == 0
       || strcmp (str, "beat") == 0
       || strcmp (str, "beats") == 0)
        {
          *units_out = BB_DURATION_UNITS_BEATS;
          return TRUE;
        }
      break;
    case 's':
      if (strcmp (str, "s") == 0
       || strcmp (str, "sec") == 0
       || strcmp (str, "secs") == 0
       || strcmp (str, "second") == 0
       || strcmp (str, "seconds") == 0)
        {
          *units_out = BB_DURATION_UNITS_SECONDS;
          return TRUE;
        }
      if (strcmp (str, "samp") == 0
       || strcmp (str, "samps") == 0
       || strcmp (str, "sample") == 0
       || strcmp (str, "samples") == 0)
        {
          *units_out = BB_DURATION_UNITS_SAMPLES;
          return TRUE;
        }
      break;
    case 'n':
      if (strcmp (str, "note-length") == 0
       || strcmp (str, "note-lengths") == 0)
        {
          *units_out = BB_DURATION_UNITS_NOTE_LENGTHS;
          return TRUE;
        }
      break;
    }
  return FALSE;
}

static inline gboolean
bb_duration_units_valid (BbDurationUnits units)
{
  return units == BB_DURATION_UNITS_SECONDS
      || units == BB_DURATION_UNITS_BEATS
      || units == BB_DURATION_UNITS_NOTE_LENGTHS
      || units == BB_DURATION_UNITS_SAMPLES;
}

static void
bb_duration_value_init         (GValue       *value)
{
  value->data[0].v_int = BB_DURATION_UNITS_SECONDS;
  value->data[1].v_double = 0.0;
}

static void
bb_duration_value_copy         (const GValue *src_value,
                                GValue       *dest_value)
{
  g_assert (bb_duration_units_valid (src_value->data[0].v_int));
  dest_value->data[0].v_int = src_value->data[0].v_int;
  dest_value->data[1].v_double = src_value->data[1].v_double;
}

static char *
bb_duration_collect_value      (GValue       *value,
				guint         n_collect_values,
				GTypeCValue  *collect_values,
				guint	      collect_flags)
{
  g_assert (bb_duration_units_valid (collect_values[0].v_int));
  value->data[0].v_int = collect_values[0].v_int;
  value->data[1].v_double = collect_values[1].v_double;
  return NULL;
}

static gchar*
bb_duration_lcopy_value        (const GValue *value,
                                guint         n_collect_values,
                                GTypeCValue  *collect_values,
                                guint		collect_flags)
{
  BbDuration *dur = collect_values[0].v_pointer;
  dur->units = value->data[0].v_int;
  dur->value = value->data[1].v_double;
  return NULL;
}

static GTypeValueTable bb_duration_value_table =
{
  bb_duration_value_init,
  NULL,                         /* value_free */
  bb_duration_value_copy,
  NULL,
  "id",                         /* int (for enum) and double */
  bb_duration_collect_value,
  "p",
  bb_duration_lcopy_value
};
  
static GTypeInfo bb_duration_type_info =
{
  0,            /* class_size */
  NULL,         /* base_init */
  NULL,         /* base_finalize */
  NULL,         /* class_init */
  NULL,         /* class_finalize */
  NULL,         /* class_data */
  0,            /* instance_size */
  0,            /* n_preallocs */
  NULL,         /* instance_init */
  &bb_duration_value_table
};

static GTypeFundamentalInfo bb_duration_type_fundamental_info = 
{
  0             /* flags */
};

GTypeFundamentalFlags  type_flags;
GType
bb_duration_get_type (void)
{
  static GType rv = 0;
  if (rv == 0)
    {
      rv = g_type_fundamental_next ();
      g_type_register_fundamental (rv, "BbDuration", 
                                   &bb_duration_type_info,
                                   &bb_duration_type_fundamental_info,
                                   0);
    }
  return rv;
}

gdouble bb_duration_to_seconds                (const BbDuration   *duration,
                                               const BbRenderInfo *info)
{
  switch (duration->units)
    {
    case BB_DURATION_UNITS_SECONDS:
      return duration->value;
    case BB_DURATION_UNITS_BEATS:
      return duration->value * info->beat_period;
    case BB_DURATION_UNITS_SAMPLES:
      return duration->value * info->beat_period * info->sampling_rate;
    case BB_DURATION_UNITS_NOTE_LENGTHS:
      return duration->value * info->note_duration_secs;
    default:
      g_return_val_if_reached (0.0);
    }
}

gdouble bb_duration_to_beats                  (const BbDuration   *duration,
                                               const BbRenderInfo *info)
{
  switch (duration->units)
    {
    case BB_DURATION_UNITS_SECONDS:
      return duration->value / info->beat_period;
    case BB_DURATION_UNITS_BEATS:
      return duration->value;
    case BB_DURATION_UNITS_SAMPLES:
      return duration->value / (info->beat_period * info->sampling_rate);
    case BB_DURATION_UNITS_NOTE_LENGTHS:
      return duration->value * info->note_duration_beats;
    default:
      g_return_val_if_reached (0.0);
    }
}

gdouble bb_duration_to_note_lengths           (const BbDuration   *duration,
                                               const BbRenderInfo *info)
{
  switch (duration->units)
    {
    case BB_DURATION_UNITS_SECONDS:
      return duration->value / info->note_duration_secs;
    case BB_DURATION_UNITS_BEATS:
      return duration->value / info->note_duration_beats;
    case BB_DURATION_UNITS_SAMPLES:
      return duration->value / info->note_duration_samples;
    case BB_DURATION_UNITS_NOTE_LENGTHS:
      return duration->value;
    default:
      g_return_val_if_reached (0.0);
    }
}

gdouble bb_duration_to_samples                (const BbDuration   *duration,
                                               const BbRenderInfo *info)
{
  switch (duration->units)
    {
    case BB_DURATION_UNITS_SECONDS:
      return duration->value * info->sampling_rate;
    case BB_DURATION_UNITS_BEATS:
      return duration->value * info->beat_period * info->sampling_rate;
    case BB_DURATION_UNITS_SAMPLES:
      return duration->value;
    case BB_DURATION_UNITS_NOTE_LENGTHS:
      return info->note_duration_samples * duration->value;
    default:
      g_return_val_if_reached (0.0);
    }
}



void    bb_value_get_duration                 (const GValue       *value,
                                               BbDuration         *duration)
{
  g_assert (G_VALUE_TYPE (value) == BB_TYPE_DURATION);
  duration->units = value->data[0].v_int;
  duration->value = value->data[1].v_double;
}

void    bb_value_set_duration                 (GValue             *value,
                                               BbDurationUnits     units,
			                       gdouble             n)
{
  g_assert (G_VALUE_TYPE (value) == BB_TYPE_DURATION);
  g_assert (bb_duration_units_valid (units));
  value->data[0].v_int = units;
  value->data[1].v_double = n;
}

void    bb_value_scale_duration               (GValue             *in_out,
                                               gdouble             scale)
{
  g_assert (G_VALUE_TYPE (in_out) == BB_TYPE_DURATION);
  in_out->data[1].v_double *= scale;
}

gdouble bb_value_get_duration_as_seconds      (const GValue       *value,
                                               const BbRenderInfo *info)
{
  BbDuration dur;
  bb_value_get_duration (value, &dur);
  return bb_duration_to_seconds (&dur, info);
}

gdouble bb_value_get_duration_as_beats        (const GValue       *value,
                                               const BbRenderInfo *info)
{
  BbDuration dur;
  bb_value_get_duration (value, &dur);
  return bb_duration_to_beats (&dur, info);
}
gdouble bb_value_get_duration_as_note_lengths (const GValue       *value,
                                               const BbRenderInfo *info)
{
  BbDuration dur;
  bb_value_get_duration (value, &dur);
  return bb_duration_to_note_lengths (&dur, info);
}
gdouble bb_value_get_duration_as_samples      (const GValue       *value,
                                               const BbRenderInfo *info)
{
  BbDuration dur;
  bb_value_get_duration (value, &dur);
  return bb_duration_to_samples (&dur, info);
}



static void
bb_param_duration_value_set_default    (GParamSpec   *pspec,
					GValue       *value)
{
  BbParamSpecDuration *pspec_dur = BB_PARAM_SPEC_DURATION (pspec);
  bb_value_set_duration (value,
                         pspec_dur->default_units,
                         pspec_dur->default_value);
}
static GParamSpecTypeInfo bb_param_spec_type_info_duration =
{
  sizeof (BbParamSpecDuration),
  0,                    /* n_preallocs */
  NULL,                 /* instance_init */
  0,                    /* value_type (initialized at runtime) */
  NULL,                 /* finalize */
  bb_param_duration_value_set_default,
  NULL,                 /* value_validate */
  NULL,                 /* values_cmp (XXX: this is recommended?) */
};

GType
bb_param_spec_duration_get_type (void)
{
  static GType rv = 0;
  if (rv == 0)
    {
      bb_param_spec_type_info_duration.value_type = BB_TYPE_DURATION;
      rv = g_param_type_register_static	("BbParamSpecDuration",
                                         &bb_param_spec_type_info_duration);
    }
  return rv;
}

GParamSpec *bb_param_spec_duration (const char *name,
                                    const char *nick,
                                    const char *blurb,
                                    BbDurationUnits default_units,
                                    gdouble     default_value,
                                    GParamFlags flags)
{
  BbParamSpecDuration *pspec_dur = g_param_spec_internal (BB_TYPE_PARAM_DURATION,
                                                          name, nick, blurb, flags);
  pspec_dur->default_units = default_units;
  pspec_dur->default_value = default_value;
  return &pspec_dur->parent_instance;
}
