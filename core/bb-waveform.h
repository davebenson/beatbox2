#ifndef __BB_WAVEFORM_H_
#define __BB_WAVEFORM_H_

#include "bb-filter1.h"

typedef struct _BbWaveform BbWaveform;
typedef struct _BbWaveformVtable BbWaveformVtable;

typedef double (*BbWaveformFunc) (BbWaveform *waveform,
                                  double      x);

struct _BbWaveformVtable
{
  gsize size;
  const char *name;
  BbWaveformFunc eval;
  void (*eval_array) (BbWaveform *waveform, double *inout, guint n);
  void (*destruct)(BbWaveform *);
};

struct _BbWaveform
{
  guint ref_count;
  BbWaveformFunc eval;
  BbWaveformVtable *vtable;
};

GType bb_waveform_get_type (void);
#define BB_TYPE_WAVEFORM (bb_waveform_get_type())


BbWaveform *bb_waveform_alloc (BbWaveformVtable *vtable);
BbWaveform *bb_waveform_ref   (BbWaveform       *waveform);
void        bb_waveform_unref (BbWaveform       *waveform);

G_INLINE_FUNC double bb_waveform_eval  (BbWaveform       *waveform,
                                        double            x);

void       bb_waveform_eval_linear(BbWaveform *waveform,
                                   double      start,
                                   double      step,
                                   double     *out,
                                   guint       n);
void       bb_waveform_eval_array (BbWaveform *waveform,
                                   double     *inout,
                                   guint       n);

/* common types of waveforms */
typedef struct {
  BbWaveform *waveform;
  gdouble start, length;
} BbWaveformComposeElement;
BbWaveform *bb_waveform_new_compose  (guint         n_elements,
                                      BbWaveformComposeElement *elements);
BbWaveform *bb_waveform_new_linear_subwaveform(BbWaveform      *waveform,
                                               gdouble          start,
                                               gdouble          len);
BbWaveform *bb_waveform_new_linear_transform
                                     (BbWaveform               *waveform,
                                      gdouble                   scale,
                                      gdouble                   offset);
BbWaveform *bb_waveform_new_take_filter(BbWaveform               *waveform,
                                        BbFilter1                *filter); /* takes filter! */
BbWaveform *bb_waveform_new_filter   (BbWaveform               *waveform,
                                      BbFilter1                *filter); /* copies filter! */
BbWaveform *bb_waveform_new_sin      (void);

/* 'abssinblank' is fwavblnk.raw from stk.
   i reverse-engineered it: it appears to be:
     abs(sin(4*pi*x))     if x <= 0.5
     0                    if x >= 0.5         */
BbWaveform *bb_waveform_new_abssinblank (void);
BbWaveform *bb_waveform_new_triangle (gdouble                   low_value,
                                      gdouble                   high_value);
BbWaveform *bb_waveform_new_square   (gdouble                   duty_cycle,
                                      gdouble                   off_value,
                                      gdouble                   on_value);
BbWaveform *bb_waveform_new_constant (gdouble                   value);
BbWaveform *bb_waveform_new_linear   (guint                     n_points,
                                      const gdouble            *tv_pairs);
BbWaveform *bb_waveform_new_exp      (guint                     n_points,
                                      const gdouble            *tv_pairs,
                                      const double             *transition_params);

/* interpolates in 1/x space: do not specify 0 for the start or end vals */
BbWaveform *bb_waveform_new_harmonic (gdouble                   start_val,
                                      gdouble                   end_val);

/* adjusts the input to 'subwaveform' in three linear pieces,
        [0,old_attack_time]  =>  [0,new_attack_time]
        [old_attack_time,old_decay_time]  =>  [new_attack_time,new_decay_time]
        [old_decay_time,1]  =>  [new_decay_time,1]
   this is like 'stretch-envelope' in clm, etc.
 */
BbWaveform *bb_waveform_new_adjust_attack_decay (BbWaveform           *subwaveform,
                                                 gdouble               old_attack_time,
                                                 gdouble               new_attack_time,
                                                 gdouble               old_decay_time,
                                                 gdouble               new_decay_time);

/* goofy waveforms */
BbWaveform *bb_waveform_new_semicircle(void);

/* (1-cos(2*pi*value)) / 2 */
BbWaveform *bb_waveform_new_sin_envelope(void);


#if defined (G_CAN_INLINE) || defined (__BB_DEFINE_INLINES__)
G_INLINE_FUNC double
bb_waveform_eval(BbWaveform *waveform,
                 double     value)
{
  return waveform->eval (waveform, value);
}
#endif


#endif
