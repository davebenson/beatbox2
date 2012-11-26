#ifndef __BB_INSTRUMENT_H_
#define __BB_INSTRUMENT_H_

#include <glib-object.h>
#include "bb-render-info.h"
#include "bb-utils.h"

typedef struct _BbInstrumentClass BbInstrumentClass;
typedef struct _BbInstrument BbInstrument;
typedef struct _BbInstrumentPreset BbInstrumentPreset;
typedef struct _BbInstrumentPresetParam BbInstrumentPresetParam;

GType bb_instrument_get_type(void) G_GNUC_CONST;
#define BB_TYPE_INSTRUMENT              (bb_instrument_get_type ())
#define BB_INSTRUMENT(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), BB_TYPE_INSTRUMENT, BbInstrument))
#define BB_INSTRUMENT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), BB_TYPE_INSTRUMENT, BbInstrumentClass))
#define BB_INSTRUMENT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), BB_TYPE_INSTRUMENT, BbInstrumentClass))
#define BB_IS_INSTRUMENT(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BB_TYPE_INSTRUMENT))
#define BB_IS_INSTRUMENT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), BB_TYPE_INSTRUMENT))

typedef double *(*BbInstrumentSynthFunc) (BbInstrument *instrument,
                                          BbRenderInfo *render_info,
                                          GValue       *params,
                                          guint        *n_samples_out);

typedef enum
{
  BB_INSTRUMENT_EVENT_PARAM,
  BB_INSTRUMENT_EVENT_ACTION
} BbInstrumentEventType;

typedef struct {
  gdouble time;
  BbInstrumentEventType type;
  union {
    struct {
      guint index;
      GValue value;
    } param;
    struct {
      guint index;
      GValue *args;
    } action;
  } info;
} BbInstrumentEvent;

typedef double *(*BbInstrumentComplexSynthFunc) (BbInstrument *instrument,
                                                 BbRenderInfo *render_info,
                                                 double        duration,   /* in seconds */
                                                 GValue       *init_params,
                                                 guint         n_events,
                                                 const BbInstrumentEvent *events,
                                                 guint        *n_samples_out);

typedef struct 
{
  char *name;
  guint n_args;
  GParamSpec **args;
} BbInstrumentActionSpec;

struct _BbInstrumentClass
{
  GObjectClass base_class;
};

struct _BbInstrumentPresetParam
{
  guint index;
  GValue value;
};
struct _BbInstrumentPreset
{
  char *name;
  guint n_params;
  BbInstrumentPresetParam *params;
};

struct _BbInstrument
{
  GObject base_instance;

  char *name;
  BbInstrumentSynthFunc synth_func;
  BbInstrumentComplexSynthFunc complex_synth_func;
  guint n_param_specs;
  GParamSpec **param_specs;
  gpointer data;                /* for use by synth-func */
  GDestroyNotify free_data;

  guint n_actions;
  BbInstrumentActionSpec **actions;

  guint n_presets;
  BbInstrumentPreset *presets;

  int volume_param, duration_param; /* these may be -1 if none exists */
  int frequency_param;
};

BbInstrument *bb_instrument_new           (void);
BbInstrument *bb_instrument_new_generic   (const char            *name,
                                           GParamSpec            *param_spec_0,
                                           ...);
BbInstrument *bb_instrument_new_generic_v (const char            *name,
                                           guint                  n_param_specs,
                                           GParamSpec           **param_specs);
void          bb_instrument_set_name      (BbInstrument          *instr,
                                           const char            *name);
void          bb_instrument_add_param     (BbInstrument          *instrument,
                                           guint                  index,
                                           GParamSpec            *pspec);
void          bb_instrument_add_action    (BbInstrument          *instrument,
                                           guint                  index,
                                           const char            *name,
                                           GParamSpec            *arg0,
                                           ...);
void          bb_instrument_add_action_v  (BbInstrument          *instrument,
                                           guint                  index,
                                           const char            *name,
                                           guint                  n_args,
                                           GParamSpec           **args);
void          bb_instrument_add_preset    (BbInstrument          *instrument,
                                           const char            *preset_name,
                                           const char            *first_param_name,
                                           ...);
void          bb_instrument_add_preset_v  (BbInstrument          *instrument,
                                           const char            *preset_name,
                                           guint                  n_params,
                                           GParameter            *params);
void          bb_instrument_add_preset_v2 (BbInstrument          *instrument,
                                           const char            *preset_name,
                                           guint                  n_params,
                                           BbInstrumentPresetParam *params);
void          bb_instrument_apply_preset  (BbInstrument          *instrument,
                                           const char            *name,
                                           GValue                *instrument_params);
BbInstrument *bb_instrument_from_string   (const char            *str); /* TODO: remove? */
BbInstrument *bb_instrument_parse_string  (const char            *str,
                                           GError               **error);

/* for containing one set of parameters inside another */
gboolean      bb_instrument_params_map    (guint                  n_instr_params,
                                           GParamSpec           **instr_params,
                                           guint                 *instr_param_indices,
                                           GPtrArray             *container_params,
                                           GError               **error);
gboolean      bb_instrument_map_params    (BbInstrument          *instrument,
                                           guint                 *instr_param_indices,
                                           GPtrArray             *container_params,
                                           GError               **error);

const char  * bb_instrument_string_scan   (const char            *start);

gboolean      bb_instrument_lookup_param  (BbInstrument          *instrument,
                                           const char            *name,
                                           guint                 *index_out);
gboolean      bb_instrument_lookup_event  (BbInstrument          *instrument,
                                           const char            *name,
                                           BbInstrumentEventType *type_out,
                                           guint                 *index_out);

/* does error checking, then invokes the synth_func or complex_synth_func */
double *      bb_instrument_synth        (BbInstrument *instrument,
                                          BbRenderInfo *render_info,
                                          GValue       *params,
                                          guint        *n_samples_out);
double *      bb_instrument_complex_synth(BbInstrument *instrument,
                                          BbRenderInfo *render_info,
                                          double        duration,   /* in seconds */
                                          GValue       *init_params,
                                          guint         n_events,
                                          const BbInstrumentEvent *events,
                                          guint        *n_samples_out);

/* fills rv with render_info->note_duration_samples values,
   linearly interpolating the parameter settings in events.
   (used for implementing some complex instruments) */
void          bb_instrument_param_events_interpolate (BbInstrument      *instrument,
                                                      guint              param_index,
                                                      const GValue      *init_value,
                                                      const BbRenderInfo *render_info,
                                                      guint             n_events,
                                                      const BbInstrumentEvent *events,
                                                      BbInterpolationMode interp,
                                                      gdouble           *rv);

typedef struct {
  guint enum_value;
  const char *name;
  gdouble default_value;
} BbInstrumentSimpleParam;

BbInstrument *bb_instrument_new_from_simple_param (guint        n_simple_param,
                                                   const BbInstrumentSimpleParam *simple_params);

typedef BbInstrument *(*BbInstrumentTemplateConstructor) (guint            n_args,
                                                          char           **args,
                                                          GError         **error);

void     bb_instrument_template_constructor_register (const char     *name,
                                                      BbInstrumentTemplateConstructor);

#endif
