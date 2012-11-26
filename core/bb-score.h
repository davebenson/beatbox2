#ifndef __BB_SCORE_H_
#define __BB_SCORE_H_

#include "bb-instrument.h"
#include "bb-vm.h"
#include "bb-tuning.h"

typedef struct _BbScorePart BbScorePart;
typedef struct _BbScoreBeat BbScoreBeat;
typedef struct _BbScore BbScore;

typedef struct _BbScoreNote BbScoreNote;
typedef enum
{
  BB_SCORE_NOTE_SIMPLE,
  BB_SCORE_NOTE_COMPLEX
} BbScoreNoteType;

struct _BbScoreNote
{
  BbScoreNoteType type;
  GValue *values;
  double volume;
  double start;                         /* in beats */

  /* for complex notes */
  guint n_events;
  BbInstrumentEvent *events;		/* note: there is a dummy event at events[n_events] whose only
					   valid member is 'time': used to provide a duration */
};

struct _BbScorePart
{
  BbScore *score;
  BbInstrument *instrument;
  guint n_notes;
  BbScoreNote *notes;
  guint notes_alloced;
  double first_beat;
  char *name;
  guint index;

  double max_beat, min_beat;

  GValue *latest_values;        /* instrument->n_param_specs+4 of them */
  BbProgram **value_programs;   /* instrument->n_param_specs of them */
  gdouble cur_beat;
  gdouble cur_pitchshift;
  gint cur_octave;
  gdouble cur_note_step;
  gdouble cur_comma_step;
  gdouble cur_volume;
  gdouble cur_volume_factor;
  gdouble cur_dot_beats;
  BbProgram *volume_program;
  BbTuning *cur_tuning;

  guint n_program_pspecs;       /* == program_pspecs->len */
  GPtrArray *program_pspecs;

  /* these describe a pending complex note.
     they are only value between 'ON' and 'OFF' directives.
     'complex_note_events!=NULL' can be used to see if you are in a
     complex-note creation section */
  GArray *complex_note_events;
  GValue *complex_note_init_values;
  double complex_note_start, complex_note_volume, complex_note_start_time;
};

void bb_score_part_set_note      (BbScorePart *part,
                                  gint         note);
void bb_score_part_add_note      (BbScorePart *part);
void bb_score_part_set_octave    (BbScorePart *part,
                                  int          octave);
void bb_score_part_adjust_octave (BbScorePart *part,
                                  int          octave);

typedef enum
{
  BB_BEAT_CONSTANT,
  BB_BEAT_LINEAR,               /* linear in beat-period as a function of beat-number */
  BB_BEAT_GEOMETRIC,            /* exponential in beat-period as a function of beat-number */
  BB_BEAT_HARMONIC,             /* linear in BPM as a function of beat-number */
} BbBeatMode;

struct _BbScoreBeat
{
  guint first_beat;
  gdouble n_beats;
  gdouble first_beat_time;
  BbBeatMode mode;

  /* starting and ending beat periods.
     start==end if mode==CONSTANT */
  gdouble start_beat_period, end_beat_period;

  gdouble n_seconds;            /* a computed value (cached for convenience) */
};

struct _BbScore
{
  guint n_parts;
  BbScorePart **parts;
  guint parts_alloced;
  guint n_beats;
  BbScoreBeat *beats;
  guint beats_alloced;
  GHashTable *marks;
  GHashTable *parts_by_name;

  BbScorePart *cur_part;
};

/* start_sample is in the same units as the 'score' */
typedef gboolean (*BbScoreRenderHandler) (guint         start_sample,
                                          guint         n_doubles,
                                          const double *doubles,
				          gpointer      handler_data,
                                          GError      **error);

BbScore *bb_score_new                (void);
void     bb_score_free               (BbScore      *score);
void     bb_score_add_beats          (BbScore      *score,
                                      gdouble       n_beats,
				      double        beat_period);
void     bb_score_add_beats_variable (BbScore      *score,
                                      gdouble       n_beats,
                                      BbBeatMode    beat_mode,
				      double        start_beat_period,
				      double        end_beat_period);
BbScorePart *bb_score_add_part       (BbScore      *score,
                                      BbInstrument *instrument,
                                      gdouble       beat);
BbScorePart *bb_score_add_part_as_string (BbScore      *score,
                                          BbInstrument *instrument,
				          double        beat,
				          const char   *str,
				          GError      **error);

void         bb_score_part_set_name   (BbScorePart  *part,
                                       const char   *name);
BbScorePart *bb_score_get_part_by_name(BbScore      *score,
                                       const char   *name);

void         bb_score_set_mark       (BbScore      *score,
                                      const char   *name,
                                      double        beat);
gboolean     bb_score_get_mark       (BbScore      *score,
                                      const char   *name,
                                      double       *beat_out);

/* --- rendering --- */
typedef enum
{
  BB_SCORE_NOTE_FILTER_REJECT,
  BB_SCORE_NOTE_FILTER_ACCEPT,
  BB_SCORE_NOTE_FILTER_CHAIN
} BbScoreNoteFilterResult;

typedef BbScoreNoteFilterResult (*BbScoreNoteFilterFunc) (BbScorePart   *part,
                                                          guint          note_index,
                                                          gpointer       data);
typedef struct _BbScoreRenderConfig BbScoreRenderConfig;
BbScoreRenderConfig *bb_score_render_config_new        (void);
void  bb_score_render_config_free            (BbScoreRenderConfig  *config);
void  bb_score_render_config_add_filter      (BbScoreRenderConfig  *config,
                                              BbScoreNoteFilterFunc func,
                                              gpointer              data,
                                              GDestroyNotify        destroy);
void  bb_score_render_config_set_sampling_rate(BbScoreRenderConfig  *config,
                                              gdouble               sampling_rate);
double bb_score_render_config_get_sampling_rate(BbScoreRenderConfig  *config);
void  bb_score_render_config_set_start_time  (BbScoreRenderConfig  *config,
                                              gdouble               start_secs);
void  bb_score_render_config_set_end_time    (BbScoreRenderConfig  *config,
                                              gdouble               end_secs);
void  bb_score_render_config_unset_start_time(BbScoreRenderConfig  *config);
void  bb_score_render_config_unset_end_time  (BbScoreRenderConfig  *config);


gboolean bb_score_render             (BbScore      *score,
                                      BbScoreRenderConfig *config,
                                      BbScoreRenderHandler handler,
                                      gpointer             handler_data,
                                      gboolean      print_status,
                                      GError      **error);
gboolean bb_score_render_wavfile     (BbScore      *score,
                                      BbScoreRenderConfig *config,
                                      const char   *wav_filename,
                                      gboolean      print_status,
                                      GError      **error);
gboolean bb_score_render_raw         (BbScore      *score,
                                      BbScoreRenderConfig *config,
                                      const char   *raw_filename,
                                      gboolean      print_status,
                                      GError      **error);

typedef struct 
{
  guint start_sample, n_samples;
  gdouble max_value, min_value;
  char *filename;
} BbScoreRenderResultPart;
typedef struct 
{
  guint n_parts;
  BbScoreRenderResultPart *parts;
  gdouble max_value, min_value;
  guint start_sample, n_samples;
  char *filename;
  char *dir;
} BbScoreRenderResult;

BbScoreRenderResult *bb_score_render_for_analysis(BbScore             *score,
                                                  BbScoreRenderConfig *config,
                                                  const char          *output_dir,
                                                  gboolean             print_status,
                                                  GError             **error);
void                 bb_score_render_result_free (BbScoreRenderResult *result,
                                                  gboolean             do_unlink);

/* --- utilities --- */
gdouble  bb_score_get_time_from_beat (BbScore      *score,
                                      gdouble       beat);
gdouble  bb_score_get_beat_period_from_beat (BbScore      *score,
                                      gdouble       beat);
gdouble  bb_score_get_beat_period_from_time (BbScore      *score,
                                      gdouble       time);
gdouble  bb_score_get_beat_from_time (BbScore      *score,
                                      gdouble       beat);

void     bb_score_dump               (BbScore      *score);

void     bb_score_part_repeat        (BbScorePart  *part,
                                      guint         total_reps);
BbScorePart *bb_score_copy_part      (BbScore      *score,
                                      BbScorePart  *part,
                                      gdouble       new_start_beat);

/* debugging */
extern gboolean bb_score_gnuplot_notes;
#endif
