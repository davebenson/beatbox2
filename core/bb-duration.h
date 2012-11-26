#ifndef __BB_DURATION_H_
#define __BB_DURATION_H_

typedef struct _BbDuration BbDuration;

#include "bb-render-info.h"
#include <glib-object.h>


#define BB_TYPE_DURATION	(bb_duration_get_type())

typedef enum
{
  BB_DURATION_UNITS_SECONDS,
  BB_DURATION_UNITS_BEATS,
  BB_DURATION_UNITS_NOTE_LENGTHS,
  BB_DURATION_UNITS_SAMPLES
} BbDurationUnits;

struct _BbDuration
{
  BbDurationUnits units;
  gdouble value;
};

G_BEGIN_DECLS
gboolean bb_duration_parse_units (const char *str,
                                  BbDurationUnits *units_out);

gdouble bb_duration_to_seconds                (const BbDuration   *duration,
                                               const BbRenderInfo *info);
gdouble bb_duration_to_beats                  (const BbDuration   *duration,
                                               const BbRenderInfo *info);
gdouble bb_duration_to_note_lengths           (const BbDuration   *duration,
                                               const BbRenderInfo *info);
gdouble bb_duration_to_samples                (const BbDuration   *duration,
                                               const BbRenderInfo *info);


void    bb_value_get_duration                 (const GValue       *value,
                                               BbDuration         *duration);
void    bb_value_set_duration                 (GValue             *value,
                                               BbDurationUnits     units,
			                       gdouble             n);
void    bb_value_scale_duration               (GValue             *in_out,
                                               gdouble             scale);
gdouble bb_value_get_duration_as_seconds      (const GValue       *value,
                                               const BbRenderInfo *info);
gdouble bb_value_get_duration_as_beats        (const GValue       *value,
                                               const BbRenderInfo *info);
gdouble bb_value_get_duration_as_note_lengths (const GValue       *value,
                                               const BbRenderInfo *info);
gdouble bb_value_get_duration_as_samples      (const GValue       *value,
                                               const BbRenderInfo *info);

#define BB_TYPE_PARAM_DURATION  (bb_param_spec_duration_get_type ())
#define BB_IS_PARAM_DURATION    (G_TYPE_CHECK_INSTANCE_TYPE ((pspec), BB_TYPE_PARAM_DURATION))
#define BB_PARAM_SPEC_DURATION(pspec) (G_TYPE_CHECK_INSTANCE_CAST ((pspec), BB_TYPE_PARAM_DURATION, BbParamSpecDuration))
typedef struct _BbParamSpecDuration BbParamSpecDuration;
struct _BbParamSpecDuration
{
  GParamSpec parent_instance;
  BbDurationUnits default_units;
  gdouble default_value;
};

GParamSpec *bb_param_spec_duration (const char *name,
                                    const char *nick,
                                    const char *blurb,
                                    BbDurationUnits default_units,
                                    gdouble     default_value,
                                    GParamFlags flags);

/*< private >*/
GType bb_duration_get_type (void);
GType bb_param_spec_duration_get_type (void);

G_END_DECLS

#endif
