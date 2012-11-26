#ifndef __BB_FILTER1_H_
#define __BB_FILTER1_H_

typedef struct _BbFilter1 BbFilter1;
typedef struct _BbFilter1Vtable BbFilter1Vtable;

typedef double (*BbFilter1Func)     (BbFilter1 *filter,
                                     double     in);
typedef void   (*BbFilter1Destruct) (BbFilter1 *filter);

#include <glib-object.h>

struct _BbFilter1Vtable
{
  BbFilter1Func filter;
  BbFilter1 *(*copy)(BbFilter1 *);
  void (*run_buffer)(BbFilter1 *, const double *, double *, guint);
  BbFilter1Destruct destruct;
};
struct _BbFilter1
{
  BbFilter1Vtable *vtable;
  BbFilter1Func filter;
};

GType bb_filter1_get_type (void);
#define BB_TYPE_FILTER1 (bb_filter1_get_type())

G_INLINE_FUNC double     bb_filter1_run (BbFilter1 *filter,
                                         double     in);


BbFilter1 *bb_filter1_alloc (gsize      size,
                             BbFilter1Vtable *vtable);
BbFilter1 *bb_filter1_copy  (BbFilter1 *filter);
void       bb_filter1_free  (BbFilter1 *filter);

void       bb_filter1_run_buffer(BbFilter1    *filter,
			         const double *in,
			         double       *out,
			         guint         n);

BbFilter1 * bb_filter1_new_3_2 (double x0,
                                double x1,
                                double x2,
                                double y1,
                                double y2);
#define bb_filter1_new_3_2_defaults() bb_filter1_new_3_2(1,0,0,-1,0)

BbFilter1 *bb_filter1_new_square (guint n);
BbFilter1 *bb_filter1_new_arctan (void);
BbFilter1 *bb_filter1_new_sin    (void);
BbFilter1 *bb_filter1_new_cos    (void);
BbFilter1 *bb_filter1_new_tan    (void);
BbFilter1 *bb_filter1_new_exp    (void);
BbFilter1 *bb_filter1_new_ln     (void);
BbFilter1 *bb_filter1_new_log2   (void);
BbFilter1 *bb_filter1_new_log10  (void);
BbFilter1 *bb_filter1_new_pow_const(gdouble exponent);
BbFilter1 *bb_filter1_new_intpow (gint exponent);
BbFilter1 *bb_filter1_new_add_dc(gdouble constant);
BbFilter1 *bb_filter1_new_gain(gdouble constant);

/* a biquad is another name for a 3-2 filter */
BbFilter1 *
bb_filter1_new_biquad (gdouble         gain,
                       gdouble         frequency,
                       gdouble         sampling_rate,
                       gdouble         radius);
void        bb_filter1_3_2_set_resonance (BbFilter1      *filter,
                                          gdouble         gain,
                                          gdouble         frequency,
                                          gdouble         sampling_rate,
                                          gdouble         radius,
                                          gboolean        normalize);

BbFilter1 * bb_filter1_one_pole (gdouble the_pole,
                                gdouble gain);
#define bb_filter1_one_pole_default() bb_filter1_one_pole(0.9,1.0)

BbFilter1 * bb_filter1_new_pole_zero (double x0,
                                      double x1,
                                      double y0);
BbFilter1 * bb_filter1_new_dc_block  (void);
void        bb_filter1_pole_zero_set_allpass(BbFilter1 *filter,
                                             double     gain,
                                             double     coeff);
void        bb_filter1_pole_zero_set_dcblock(BbFilter1 *filter,
                                             double     coeff);

BbFilter1 * bb_filter1_new_two_zero  (double x0,
                                      double x1,
                                      double x2);
void        bb_filter1_two_zero_set_params (BbFilter1   *filter,
                                            double       x0,
                                            double       x1,
                                            double       x2);
void        bb_filter1_two_zero_set_notch  (BbFilter1   *filter,
                                            double       sampling_rate,
                                            double       frequency,
                                            double       radius);


typedef enum
{
  BB_BUTTERWORTH_LOW_PASS,
  BB_BUTTERWORTH_HIGH_PASS,
  BB_BUTTERWORTH_BAND_PASS,
  BB_BUTTERWORTH_NOTCH,
  BB_BUTTERWORTH_PEAKING_EQ,
  BB_BUTTERWORTH_LOW_SHELF,
  BB_BUTTERWORTH_HIGH_SHELF
} BbButterworthMode;

GType bb_butterworth_mode_get_type (void);
#define BB_TYPE_BUTTERWORTH_MODE (bb_butterworth_mode_get_type())

BbFilter1 *bb_filter1_butterworth (BbButterworthMode        mode,
                                   guint                    sampling_rate,
                                   double                   frequency,
			           double                   bandwidth,
			           double                   db_gain);
void bb_filter1_3_2_set_butterworth (BbFilter1               *filter,
                                     BbButterworthMode        mode,
                                     guint                    sampling_rate,
                                     double                   frequency,
			             double                   bandwidth,
			             double                   db_gain);

BbFilter1 *bb_filter1_new_plucked_string (guint      sampling_rate,
                                          gdouble    frequency,
                                          gdouble    falloff_per_second,
                                          gdouble    b);

BbFilter1 *bb_filter1_new_linear         (double     sampling_rate,
                                          guint      n_input_tv_pairs,
                                          const gdouble *input_tv_pairs,
                                          guint      n_output_tv_pairs,
                                          const gdouble *output_tv_pairs);

#if defined (G_CAN_INLINE) || defined (__BB_DEFINE_INLINES__)
G_INLINE_FUNC double
bb_filter1_run(BbFilter1 *filter,
               double     value)
{
  return filter->filter (filter, value);
}
#endif

#endif
