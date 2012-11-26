#include <stdlib.h>
#include <math.h>
#include "bb-filter1.h"
#include "bb-delay-line.h"
#include "bb-tapped-delay-line.h"
#include "bb-init.h"
#include "bb-vm.h"
#include "bb-utils.h"

GType 
bb_filter1_get_type (void)
{
  static GType rv = 0;
  if (rv == 0)
    rv = g_boxed_type_register_static ("BbFilter1",
                                       (GBoxedCopyFunc) bb_filter1_copy,
                                       (GBoxedFreeFunc) bb_filter1_free);
  return rv;
}

/* --- Generic Filters --- */
BbFilter1 *bb_filter1_alloc (gsize      size,
                             BbFilter1Vtable *vtable)
{
  BbFilter1 *rv;
  g_assert (size >= sizeof (BbFilter1));
  rv = g_malloc (size);
  rv->filter = vtable->filter;
  rv->vtable = vtable;
  return rv;
}

BbFilter1 *bb_filter1_copy  (BbFilter1 *filter)
{
  return filter->vtable->copy (filter);
}

void       bb_filter1_free  (BbFilter1 *filter)
{
  if (filter->vtable->destruct)
    filter->vtable->destruct (filter);
  g_free (filter);
}

void
bb_filter1_run_buffer (BbFilter1 *filter,
                       const double *in,
                       double *out,
                       guint n)
{
  if (filter->vtable->run_buffer != NULL)
    filter->vtable->run_buffer (filter, in, out, n);
  else
    {
      guint i;
      for (i = 0; i < n; i++)
        out[i] = bb_filter1_run (filter, in[i]);
    }
}

/* --- 3/2 Filters --- */
typedef struct _BbFilter1_32 BbFilter1_32;
struct _BbFilter1_32
{
  BbFilter1 base;
  double x[3], y[2];
  double xbuf[2], ybuf[2];
};

static BbFilter1 *
copy_3_2 (BbFilter1 *filter1)
{
  BbFilter1_32 *in = (BbFilter1_32 *) filter1;
  return bb_filter1_new_3_2 (in->x[0], in->x[1], in->x[2], in->y[0], in->y[1]);
}

static double
filter_3_2 (BbFilter1 *filter1,
            double     in)
{
  BbFilter1_32 *f = (BbFilter1_32 *) filter1;
  double out = in * f->x[0]
             + f->xbuf[0] * f->x[1]
             + f->xbuf[1] * f->x[2]
             + f->ybuf[0] * f->y[0]
             + f->ybuf[1] * f->y[1];
  f->xbuf[1] = f->xbuf[0];
  f->xbuf[0] = in;
  f->ybuf[1] = f->ybuf[0];
  f->ybuf[0] = out;
  return out;
}

static BbFilter1Vtable *
bb_filter1_3_2_vtable (void)
{
  static BbFilter1Vtable rv;
  if (rv.filter == NULL)
    {
      rv.filter = filter_3_2;
      rv.copy = copy_3_2;
    }
  return &rv;
}

BbFilter1 *
bb_filter1_new_3_2 (double x0, double x1, double x2,
                    double y1, double y2)
{
  BbFilter1 *rv = bb_filter1_alloc (sizeof (BbFilter1_32),
                                    bb_filter1_3_2_vtable ());
  BbFilter1_32 *rv_3_2 = (BbFilter1_32 *) rv;
  rv_3_2->x[0] = x0;
  rv_3_2->x[1] = x1;
  rv_3_2->x[2] = x2;
  rv_3_2->y[0] = y1;
  rv_3_2->y[1] = y2;
  rv_3_2->xbuf[0] = rv_3_2->xbuf[1] = 0;
  rv_3_2->ybuf[0] = rv_3_2->ybuf[1] = 0;
  return rv;
}

void
bb_filter1_3_2_set_resonance (BbFilter1      *filter,
                              gdouble         gain,
                              gdouble         frequency,
                              gdouble         sampling_rate,
                              gdouble         radius,
                              gboolean        normalize)
{
  BbFilter1_32 *f32 = (BbFilter1_32 *) filter;
  g_assert (filter->vtable == bb_filter1_3_2_vtable ());
  f32->y[0] = 2.0 * radius * cos (BB_2PI * frequency / sampling_rate);
  f32->y[1] = - radius * radius;
  if (normalize)
    {
      f32->x[0] = 0.5 - 0.5 * f32->y[1];
      f32->x[1] = 0;
      f32->x[2] = -f32->x[0];
      if (gain != 1.0)
        {
          f32->x[0] *= gain;
          f32->x[1] *= gain;
          f32->x[2] *= gain;
        }
    }
  else
    {
      /* note: stk does not do this */
      f32->x[0] = gain;
      f32->x[1] = 0;
      f32->x[2] = 0;
    }
}

BbFilter1 *
bb_filter1_new_biquad (gdouble         gain,
                       gdouble         frequency,
                       gdouble         sampling_rate,
                       gdouble         radius)
{
  BbFilter1 *rv = bb_filter1_new_3_2 (0,0,0,0,0);
  bb_filter1_3_2_set_resonance (rv, gain, frequency, sampling_rate, radius, TRUE);
  return rv;
}

/* --- Butterworth Filters (a kind of 3/2 filter) --- */
GType
bb_butterworth_mode_get_type (void)
{
  static GType rv = 0;
  if (rv == 0)
    {
      static const GEnumValue values[] = {
#define CASE(shortname, nick) { BB_BUTTERWORTH_##shortname, "BB_BUTTERWORTH_" #shortname, nick }
       CASE (LOW_PASS, "low-pass"),
       CASE (HIGH_PASS, "high-pass"),
       CASE (BAND_PASS, "band-pass"),
       CASE (NOTCH, "notch"),
       CASE (PEAKING_EQ, "peaking-eq"),
       CASE (LOW_SHELF, "low-shelf"),
       CASE (HIGH_SHELF, "high-shelf"),
       { 0, NULL, NULL }
#undef CASE
      };
      rv = g_enum_register_static ("BbButterworthMode", values);
    }
  return rv;
}
  
/* these filter coefficient calculations come from:
 *     http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
 */
static void
bb_filter1_butterworth_coeffs (BbButterworthMode        mode,
                               guint                    sampling_rate,
                               double                   frequency,
			       double                   bandwidth,
			       double                   db_gain,
                               gdouble                 *coeffs_out)
{
  gdouble A = 0.0, omega, sn, cs, alpha, beta = 0.0;
  gdouble a0, a1, a2, b0, b1, b2;
  gdouble  S = 1.0;		/* shelving parameter */

  omega = G_PI * 2.0 * frequency / sampling_rate;
  sn = sin (omega);
  cs = cos (omega);
  alpha = sn * sinh (M_LN2 / 2.0 * bandwidth * omega / sn);

  if (mode == BB_BUTTERWORTH_PEAKING_EQ
   || mode == BB_BUTTERWORTH_LOW_SHELF
   || mode == BB_BUTTERWORTH_HIGH_SHELF)
    {
      A = exp (db_gain * M_LN10 / 40.0);
      beta = sqrt ((A * A + 1.0) / S - ((A - 1.0) * (A - 1.0)));
    }

  switch (mode)
    {
    case BB_BUTTERWORTH_LOW_PASS:
      b0 = (1.0 - cs) / 2.0;
      b1 = 1.0 - cs;
      b2 = (1.0 - cs) / 2.0;
      a0 = 1.0 + alpha;
      a1 = -2.0 * cs;
      a2 = 1.0 - alpha;
      break;
    case BB_BUTTERWORTH_HIGH_PASS:
      b0 = (1.0 + cs) / 2.0;
      b1 = -(1.0 + cs);
      b2 = (1.0 + cs) / 2.0;
      a0 = 1.0 + alpha;
      a1 = -2.0 * cs;
      a2 = 1.0 - alpha;
      break;
    case BB_BUTTERWORTH_BAND_PASS:
      b0 = alpha;
      b1 = 0;
      b2 = -alpha;
      a0 = 1.0 + alpha;
      a1 = -2.0 * cs;
      a2 = 1.0 - alpha;
      break;
    case BB_BUTTERWORTH_NOTCH:
      b0 = 1.0;
      b1 = -2.0 * cs;
      b2 = 1.0;
      a0 = 1.0 + alpha;
      a1 = -2.0 * cs;
      a2 = 1.0 - alpha;
      break;
    case BB_BUTTERWORTH_PEAKING_EQ:
      b0 = 1.0 + alpha * A;
      b1 = -2.0 * cs;
      b2 = 1.0 - alpha * A;
      a0 = 1.0 + alpha / A;
      a1 = -2.0 * cs;
      a2 = 1.0 - alpha / A;
      break;
    case BB_BUTTERWORTH_LOW_SHELF:
      b0 = A * ((A + 1.0) - (A - 1.0) * cs + beta * sn);
      b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cs);
      b2 = A * ((A + 1.0) - (A - 1.0) * cs - beta * sn);
      a0 = (A + 1.0) + (A - 1.0) * cs + beta * sn;
      a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cs);
      a2 = (A + 1.0) + (A - 1.0) * cs - beta * sn;
      break;
    case BB_BUTTERWORTH_HIGH_SHELF:
      b0 = A * ((A + 1.0) + (A - 1.0) * cs + beta * sn);
      b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cs);
      b2 = A * ((A + 1.0) + (A - 1.0) * cs - beta * sn);
      a0 = (A + 1.0) - (A - 1.0) * cs + beta * sn;
      a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cs);
      a2 = (A + 1.0) - (A - 1.0) * cs - beta * sn;
      break;
    default:
      a0=a1=a2=b0=b1=b2=0;
      g_error ("bad butterworth-filter mode: %d", mode);
    }

  coeffs_out[0] = +b0 / a0;
  coeffs_out[1] = +b1 / a0;
  coeffs_out[2] = +b2 / a0;
  coeffs_out[3] = -a1 / a0;
  coeffs_out[4] = -a2 / a0;
}

BbFilter1 *
bb_filter1_butterworth (BbButterworthMode        mode,
                        guint                    sampling_rate,
                        double                   frequency,
			double                   bandwidth,
			double                   db_gain)
{
  gdouble c[5];
  bb_filter1_butterworth_coeffs (mode, sampling_rate, frequency, bandwidth, db_gain, c);
  return bb_filter1_new_3_2 (c[0], c[1], c[2], c[3], c[4]);
}
void
bb_filter1_3_2_set_butterworth (BbFilter1               *filter,
                                BbButterworthMode        mode,
                                guint                    sampling_rate,
                                double                   frequency,
			        double                   bandwidth,
			        double                   db_gain)
{
  BbFilter1_32 *f32 = (BbFilter1_32 *) filter;
  gdouble c[5];
  g_assert (filter->vtable == bb_filter1_3_2_vtable ());
  bb_filter1_butterworth_coeffs (mode, sampling_rate, frequency, bandwidth, db_gain, c);
  f32->x[0] = c[0];
  f32->x[1] = c[1];
  f32->x[2] = c[2];
  f32->y[0] = c[3];
  f32->y[1] = c[4];
}

/* --- Square Filters --- */
typedef struct _BbFilter1Square BbFilter1Square;
struct _BbFilter1Square
{
  BbFilter1 base;
  guint n;
  gdouble one_over_n;
  guint write_pos;
  gdouble *values;
  gdouble total;
  guint recompute_left;
};

static BbFilter1 *
bb_filter1_square_copy (BbFilter1 *filter1)
{
  return bb_filter1_new_square (((BbFilter1Square *)filter1)->n);
}

static double
bb_filter1_square_filter (BbFilter1 *filter1,
                          double     in)
{
  BbFilter1Square *sq = (BbFilter1Square *) filter1;
  sq->total -= sq->values[sq->write_pos];
  sq->total += in;
  sq->values[sq->write_pos] = in;
  if (++(sq->write_pos) == sq->n)
    {
      sq->write_pos = 0;
      if (--(sq->recompute_left) == 0)
        {
          guint i;
          gdouble total = 0;
          for (i = 0; i < sq->n; i++)
            total += sq->values[i];
          sq->total = total;
          sq->recompute_left = 3;
        }
    }
  return sq->total * sq->one_over_n;
}

static void
bb_filter1_square_destruct (BbFilter1 *filter1)
{
  g_free (((BbFilter1Square*)filter1)->values);
}
static BbFilter1Vtable *
bb_filter1_square_vtable ()
{
  static BbFilter1Vtable vtable;
  if (vtable.filter == NULL)
    {
      vtable.filter = bb_filter1_square_filter;
      vtable.copy = bb_filter1_square_copy;
      vtable.destruct = bb_filter1_square_destruct;
    }
  return &vtable;
}

BbFilter1 *
bb_filter1_new_square (guint count)
{
  BbFilter1 *rv = bb_filter1_alloc (sizeof (BbFilter1Square), bb_filter1_square_vtable ());
  BbFilter1Square *sq = (BbFilter1Square *) rv;
  sq->n = count;
  sq->values = g_new0 (double, count);
  sq->write_pos = 0;
  sq->total = 0;
  sq->one_over_n = 1.0 / (double) count;
  sq->recompute_left = 3;
  return rv;
}

/* --- plucked string filter --- */
typedef struct _BbFilter1PluckedString BbFilter1PluckedString;
struct _BbFilter1PluckedString
{
  BbFilter1 base;
  BbDelayLine *delay_line;
  gdouble feedback_pos;
  gdouble last_delayed;
  gdouble falloff;
  gdouble b;            /* random noise param */
};

static gdouble
bb_filter1_plucked_string_filter (BbFilter1 *filter1,
                                  double     in)
{
  BbFilter1PluckedString *ps = (BbFilter1PluckedString *) filter1;
  gdouble delayed, rv;
  delayed = bb_delay_line_get_lin (ps->delay_line, ps->feedback_pos - 1.0);
  rv = (ps->last_delayed + delayed) * 0.5 * ps->falloff + in;
  if (ps->b != 0 && g_random_double () < ps->b)
    rv = -rv;
  bb_delay_line_add (ps->delay_line, rv);
  ps->last_delayed = delayed;
  return rv;
}

static BbFilter1Vtable * bb_filter1_plucked_string_vtable (void);
static BbFilter1 *
bb_filter1_plucked_string_copy (BbFilter1 *filter)
{
  BbFilter1PluckedString *in_ps = (BbFilter1PluckedString *) filter;
  BbFilter1 *rv = bb_filter1_alloc (sizeof (BbFilter1PluckedString), bb_filter1_plucked_string_vtable ());
  BbFilter1PluckedString *rv_ps = (BbFilter1PluckedString *) rv;
  rv_ps->delay_line = bb_delay_line_new (in_ps->delay_line->n);
  rv_ps->feedback_pos = in_ps->feedback_pos;
  rv_ps->last_delayed = in_ps->last_delayed;
  rv_ps->falloff = in_ps->falloff;
  rv_ps->b = in_ps->b;
  return rv;
}

static void
bb_filter1_plucked_string_destruct (BbFilter1 *filter)
{
  BbFilter1PluckedString *ps = (BbFilter1PluckedString *) filter;
  bb_delay_line_free (ps->delay_line);
}

static BbFilter1Vtable *
bb_filter1_plucked_string_vtable (void)
{
  static BbFilter1Vtable rv;
  if (rv.filter == NULL)
    {
      rv.filter = bb_filter1_plucked_string_filter;
      rv.copy = bb_filter1_plucked_string_copy;
      rv.destruct = bb_filter1_plucked_string_destruct;
    }
  return &rv;
}

BbFilter1 *bb_filter1_new_plucked_string (guint      sampling_rate,
                                          gdouble    frequency,
                                          gdouble    falloff_per_second,
                                          gdouble    b)
{
  BbFilter1 *rv = bb_filter1_alloc (sizeof (BbFilter1PluckedString), bb_filter1_plucked_string_vtable ());
  BbFilter1PluckedString *rv_ps = (BbFilter1PluckedString *) rv;
  gdouble period_in_samples = (gdouble) sampling_rate / frequency;

  /* output will be the average of two samples:
     one with delay last_raw_output
     and one with delay (last_raw_output+1).
     They should be centered around the period of the frequency. */
  rv_ps->feedback_pos = period_in_samples - 0.5;
  rv_ps->delay_line = bb_delay_line_new ((int) ceil (rv_ps->feedback_pos + 3));
  rv_ps->falloff = pow (falloff_per_second, 1.0 / frequency);
  rv_ps->last_delayed = 0;
  rv_ps->b = b;
  return rv;
}

/* --- some stateless, no-argument filters  --- */
#define IMPLEMENT_STATELESS_NOARG_FILTER(our_name, c_function)                         \
static gdouble                                                                         \
bb_filter1_##our_name##_filter (BbFilter1 *filter, gdouble in)                         \
{                                                                                      \
  return c_function (in);                                                              \
}                                                                                      \
static BbFilter1 *                                                                     \
bb_filter1_##our_name##_copy (BbFilter1 *filter)                                       \
{                                                                                      \
  return bb_filter1_new_##our_name ();                                                 \
}                                                                                      \
static void                                                                            \
bb_filter1_##our_name##_run_buffer (BbFilter1 *filter,                                 \
                                    const double *in,                                  \
                                    gdouble *out,                                      \
                                    guint n)                                           \
{                                                                                      \
  guint i;                                                                             \
  for (i = 0; i < n; i++)                                                              \
    out[i] = c_function (in[i]);                                                       \
}                                                                                      \
                                                                                       \
static BbFilter1Vtable bb_filter1_##our_name##_vtable =                                \
{                                                                                      \
  .filter = bb_filter1_##our_name##_filter,                                            \
  .copy = bb_filter1_##our_name##_copy,                                                \
  .run_buffer = bb_filter1_##our_name##_run_buffer                                     \
};                                                                                     \
                                                                                       \
BbFilter1 *bb_filter1_new_##our_name (void)                                            \
{                                                                                      \
  return bb_filter1_alloc (sizeof (BbFilter1), &bb_filter1_##our_name##_vtable);       \
}

IMPLEMENT_STATELESS_NOARG_FILTER(arctan, atan)   /* bb_filter1_new_arctan() */
IMPLEMENT_STATELESS_NOARG_FILTER(sin, sin)       /* bb_filter1_new_sin() */
IMPLEMENT_STATELESS_NOARG_FILTER(cos, cos)       /* bb_filter1_new_cos() */
IMPLEMENT_STATELESS_NOARG_FILTER(tan, tan)       /* bb_filter1_new_tan() */
IMPLEMENT_STATELESS_NOARG_FILTER(ln, log)        /* bb_filter1_new_ln() */
IMPLEMENT_STATELESS_NOARG_FILTER(exp, exp)       /* bb_filter1_new_exp() */
//IMPLEMENT_STATELESS_NOARG_FILTER(log2, log2)     /* bb_filter1_new_log2() */
IMPLEMENT_STATELESS_NOARG_FILTER(log10, log10)   /* bb_filter1_new_log10() */

/* stateless 1-argument filters */
#define IMPLEMENT_STATELESS_1ARG_FILTER(our_name, c_function, type)                    \
typedef struct _BbFilter1_##our_name BbFilter1_##our_name;                             \
struct _BbFilter1_##our_name                                                           \
{                                                                                      \
  BbFilter1 base_filter;                                                               \
  type arg;                                                                            \
};                                                                                     \
static gdouble                                                                         \
bb_filter1_##our_name##_filter (BbFilter1 *filter, gdouble in)                         \
{                                                                                      \
  BbFilter1_##our_name *f = (BbFilter1_##our_name *) filter;                           \
  return c_function (in, f->arg);                                                      \
}                                                                                      \
static BbFilter1 *                                                                     \
bb_filter1_##our_name##_copy (BbFilter1 *filter)                                       \
{                                                                                      \
  BbFilter1_##our_name *f = (BbFilter1_##our_name *) filter;                           \
  return bb_filter1_new_##our_name (f->arg);                                           \
}                                                                                      \
static void                                                                            \
bb_filter1_##our_name##_run_buffer (BbFilter1 *filter,                                 \
                                    const double *in,                                  \
                                    gdouble *out,                                      \
                                    guint n)                                           \
{                                                                                      \
  guint i;                                                                             \
  gdouble arg = ((BbFilter1_##our_name*)filter)->arg;                                  \
  for (i = 0; i < n; i++)                                                              \
    out[i] = c_function (in[i], arg);                                                  \
}                                                                                      \
                                                                                       \
static BbFilter1Vtable bb_filter1_##our_name##_vtable =                                \
{                                                                                      \
  .filter = bb_filter1_##our_name##_filter,                                            \
  .copy = bb_filter1_##our_name##_copy,                                                \
  .run_buffer = bb_filter1_##our_name##_run_buffer                                     \
};                                                                                     \
                                                                                       \
BbFilter1 *bb_filter1_new_##our_name (type arg)                                        \
{                                                                                      \
  BbFilter1 *rv;                                                                       \
  rv = bb_filter1_alloc (sizeof (BbFilter1_##our_name), &bb_filter1_##our_name##_vtable);\
  ((BbFilter1_##our_name*)rv)->arg = arg;                                              \
  return rv;                                                                           \
}
static inline gdouble filter_add_const_func (gdouble a, gdouble b) { return a + b; }
static inline gdouble filter_mul_const_func (gdouble a, gdouble b) { return a * b; }
IMPLEMENT_STATELESS_1ARG_FILTER(add_dc, filter_add_const_func, gdouble)
IMPLEMENT_STATELESS_1ARG_FILTER(gain, filter_mul_const_func, gdouble)
IMPLEMENT_STATELESS_1ARG_FILTER(pow_const, pow, gdouble)

/* --- one-pole --- */
typedef struct _BbFilter1OnePole BbFilter1OnePole;
struct _BbFilter1OnePole
{
  BbFilter1 base;
  gdouble b0, a1;
  gdouble gain;

  double input;
  double output;
};

static gdouble
bb_filter1_one_pole_run (BbFilter1   *filter1,
                         gdouble      input)
{
  BbFilter1OnePole *op = (BbFilter1OnePole *) filter1;
  op->input = op->gain * input;
  op->output = op->b0 * op->input - op->a1 * op->output;
  return op->output;
}

static BbFilter1 * bb_filter1_one_pole_copy (BbFilter1 *in);
static void
bb_filter1_one_pole_run_buffer (BbFilter1   *filter1,
                                const double *in,
                                double       *out,
                                guint         N)
{
  BbFilter1OnePole *op = (BbFilter1OnePole *) filter1;
  double output = op->output;
  double b0gain = op->gain * op->b0;
  double a1 = op->a1;
  while (N--)
    {
      output = b0gain * (*in++) + a1 * output;
      *out++ = output;
    }
  op->output = output;
}

static BbFilter1Vtable bb_filter1_one_pole_vtable =
{
  .filter = bb_filter1_one_pole_run,
  .copy = bb_filter1_one_pole_copy,
  .run_buffer = bb_filter1_one_pole_run_buffer
};

static BbFilter1 * bb_filter1_one_pole_copy (BbFilter1 *in)
{
  BbFilter1OnePole *in_op = (BbFilter1OnePole *) in;
  BbFilter1 *rv = bb_filter1_alloc (sizeof (BbFilter1OnePole),
                                    &bb_filter1_one_pole_vtable);
  BbFilter1OnePole *rv_op = (BbFilter1OnePole *) rv;
  rv_op->a1 = in_op->a1;
  rv_op->b0 = in_op->b0;
  rv_op->gain = in_op->gain;
  rv_op->input = 0;
  rv_op->output = 0;
  return rv;
}

BbFilter1 * bb_filter1_one_pole (gdouble the_pole,
                                 gdouble gain)
{
  BbFilter1 *rv = bb_filter1_alloc (sizeof (BbFilter1OnePole),
                                    &bb_filter1_one_pole_vtable);
  BbFilter1OnePole *rv_op = (BbFilter1OnePole *) rv;
  rv_op->a1 = -gain * the_pole;
  rv_op->b0 = gain - ABS (the_pole);
  rv_op->gain = gain;
  rv_op->input = 0;
  rv_op->output = 0;
  return rv;
}

/* --- pole-zero filters --- */
typedef struct _BbFilter1PoleZero BbFilter1PoleZero;
struct _BbFilter1PoleZero
{
  BbFilter1 base;
  gdouble x0,x1,y0;
  gdouble last_input, last_output;
};

static gdouble
bb_filter1_pole_zero_run (BbFilter1   *filter1,
                          gdouble      input)
{
  BbFilter1PoleZero *op = (BbFilter1PoleZero *) filter1;
  op->last_output = op->x1 * op->last_input
                  + op->x0 * input
                  + op->y0 * op->last_output;
  op->last_input = input;
  return op->last_output;
}

static BbFilter1 * bb_filter1_pole_zero_copy (BbFilter1 *in);
static void
bb_filter1_pole_zero_run_buffer (BbFilter1   *filter1,
                                 const double *in,
                                 double       *out,
                                 guint         N)
{
  BbFilter1PoleZero *op = (BbFilter1PoleZero *) filter1;
  gdouble x0 = op->x0;
  gdouble x1 = op->x1;
  gdouble y0 = op->y0;
  gdouble last_input = op->last_input;
  gdouble last_output = op->last_output;
  while (N--)
    {
      gdouble inp = *in++;
      last_output = last_input * x1 
                  + inp * x0
                  + last_output * y0;
      last_input = inp;
      *out++ = last_output;
    }
  op->last_output = last_output;
  op->last_input = last_input;
}

static BbFilter1Vtable bb_filter1_pole_zero_vtable =
{
  .filter = bb_filter1_pole_zero_run,
  .copy = bb_filter1_pole_zero_copy,
  .run_buffer = bb_filter1_pole_zero_run_buffer
};


BbFilter1 * bb_filter1_new_pole_zero (double x0,
                                      double x1,
                                      double y0)
{
  BbFilter1 *rv = bb_filter1_alloc (sizeof (BbFilter1OnePole),
                                    &bb_filter1_pole_zero_vtable);
  BbFilter1PoleZero *rv_pz = (BbFilter1PoleZero *) rv;
  rv_pz->last_input = 0;
  rv_pz->last_output = 0;
  rv_pz->x0 = x0;
  rv_pz->x1 = x1;
  rv_pz->y0 = y0;
  return rv;
}

static BbFilter1 *
bb_filter1_pole_zero_copy (BbFilter1 *filter)
{
  BbFilter1PoleZero *op = (BbFilter1PoleZero *) filter;
  g_assert (filter->vtable == &bb_filter1_pole_zero_vtable);
  return bb_filter1_new_pole_zero (op->x0, op->x1, op->y0);
}

BbFilter1 * bb_filter1_new_dc_block  (void)
{
  return bb_filter1_new_pole_zero (1.0, -1.0, 0.99);
}

void        bb_filter1_pole_zero_set_allpass(BbFilter1 *filter,
                                             double     gain,
                                             double     coeff)
{
  BbFilter1PoleZero *pz = (BbFilter1PoleZero *) filter;
  g_assert (filter->vtable == &bb_filter1_pole_zero_vtable);
  pz->x0 = coeff * gain;
  pz->x1 = gain;
  pz->y0 = -coeff;
}

void        bb_filter1_pole_zero_set_dcblock(BbFilter1 *filter,
                                             double     coeff)
{
  BbFilter1PoleZero *pz = (BbFilter1PoleZero *) filter;
  g_assert (filter->vtable == &bb_filter1_pole_zero_vtable);
  pz->x0 = 1;
  pz->x1 = -1;
  pz->y0 = coeff;
}


/* --- Two-Zero Filters --- */
typedef struct _BbFilter1TwoZero BbFilter1TwoZero;
struct _BbFilter1TwoZero
{
  BbFilter1 base_filter;
  gdouble x0,x1,x2;
  gdouble input1, input2;
};

static gdouble
bb_filter1_two_zero_run (BbFilter1   *filter1,
                          gdouble      input)
{
  BbFilter1TwoZero *op = (BbFilter1TwoZero *) filter1;
  gdouble rv = input * op->x0 + op->input1 * op->x1 + op->input2 * op->x2;
  op->input2 = op->input1;
  op->input1 = input;
  return rv;
}

static BbFilter1 * bb_filter1_two_zero_copy (BbFilter1 *in)
{
  BbFilter1TwoZero *tz = (BbFilter1TwoZero *) in;
  return bb_filter1_new_two_zero (tz->x0, tz->x1, tz->x2);
}

static void
bb_filter1_two_zero_run_buffer (BbFilter1   *filter1,
                                 const double *in,
                                 double       *out,
                                 guint         N)
{
  BbFilter1TwoZero *tz = (BbFilter1TwoZero *) filter1;
  gdouble x0 = tz->x0;
  gdouble x1 = tz->x1;
  gdouble x2 = tz->x1;
  gdouble input1 = tz->input1;
  gdouble input2 = tz->input2;
  while (N--)
    {
      gdouble inp = *in++;
      *out++ = x0 * inp + x1 * input1 + x2 * input2;
      input2 = input1;
      input1 = inp;
    }
  tz->input1 = input1;
  tz->input2 = input2;
}

static BbFilter1Vtable bb_filter1_two_zero_vtable =
{
  .filter = bb_filter1_two_zero_run,
  .copy = bb_filter1_two_zero_copy,
  .run_buffer = bb_filter1_two_zero_run_buffer
};

BbFilter1 * bb_filter1_new_two_zero  (double x0,
                                      double x1,
                                      double x2)
{
  BbFilter1 *rv = bb_filter1_alloc (sizeof (BbFilter1TwoZero),
                                    &bb_filter1_two_zero_vtable);
  BbFilter1TwoZero *rv_tz = (BbFilter1TwoZero *) rv;
  rv_tz->x0 = x0;
  rv_tz->x1 = x1;
  rv_tz->x2 = x2;
  rv_tz->input1 = 0;
  rv_tz->input2 = 0;
  return rv;
}
void        bb_filter1_two_zero_set_params (BbFilter1   *filter,
                                            double       x0,
                                            double       x1,
                                            double       x2)
{
  BbFilter1TwoZero *tz = (BbFilter1TwoZero *) filter;
  g_assert (filter->vtable == &bb_filter1_two_zero_vtable);
  tz->x0 = x0;
  tz->x1 = x1;
  tz->x2 = x2;
}

void        bb_filter1_two_zero_set_notch  (BbFilter1   *filter,
                                            double       sampling_rate,
                                            double       frequency,
                                            double       radius)
{
  BbFilter1TwoZero *tz = (BbFilter1TwoZero *) filter;
  gdouble x2 = radius * radius;
  gdouble x1 = -2.0 * radius * cos (BB_2PI * frequency / sampling_rate);
  gdouble x0 = 1.0 / ((x1 > 0.0) ? (1.0 + x1 + x2) : (1.0 - x1 + x2));
  g_assert (filter->vtable == &bb_filter1_two_zero_vtable);
  tz->x0 = x0;
  tz->x1 = x0 * x1;
  tz->x2 = x0 * x2;
}

/* --- FIR Filters --- */
typedef struct _BbFilter1Fir BbFilter1Fir;
struct _BbFilter1Fir
{
  BbFilter1 base;
  BbTappedDelayLine *delay_line;
  gdouble *delay_coeff_pairs;
};

static gdouble
bb_filter1_fir_run (BbFilter1   *filter1,
                    gdouble      input)
{
  BbFilter1Fir *fir = (BbFilter1Fir *) filter1;
  guint n_taps = fir->delay_line->n_taps;
  guint t;
  gdouble rv;
  gdouble *coeff_at = fir->delay_coeff_pairs + 1;       /* the odd entries are coefficients */
  bb_tapped_delay_line_tick (fir->delay_line, input);
  rv = fir->delay_line->taps[0].last_output * *coeff_at;
  coeff_at += 2;
  for (t = 1; t < n_taps; t++)
    {
      rv += fir->delay_line->taps[t].last_output * *coeff_at;
      coeff_at += 2;
    }
  return rv;
}

static BbFilter1 * bb_filter1_fir_copy (BbFilter1 *in)
{
  BbFilter1Fir *fir = (BbFilter1Fir *) in;
  return bb_filter1_new_linear (1.0,
                                fir->delay_line->n_taps,
                                fir->delay_coeff_pairs,
                                0, NULL);
}

static void
bb_filter1_fir_run_buffer (BbFilter1   *filter1,
                           const double *in,
                           double       *out,
                           guint         N)
{
  BbFilter1Fir *fir = (BbFilter1Fir *) filter1;
  guint n_taps = fir->delay_line->n_taps;
  guint i, t;
  for (i = 0; i < N; i++)
    {
      gdouble *coeff_at = fir->delay_coeff_pairs + 1;       /* the odd entries are coefficients */
      gdouble rv;
      bb_tapped_delay_line_tick (fir->delay_line, in[i]);
      rv = fir->delay_line->taps[0].last_output * *coeff_at;
      coeff_at += 2;
      for (t = 1; t < n_taps; t++)
        {
          rv += fir->delay_line->taps[t].last_output * *coeff_at;
          coeff_at += 2;
        }
      out[i] = rv;
    }
}

static void
bb_filter1_fir_destruct (BbFilter1 *filter1)
{
  BbFilter1Fir *fir = (BbFilter1Fir *) filter1;
  bb_tapped_delay_line_free (fir->delay_line);
  g_free (fir->delay_coeff_pairs);
}

static BbFilter1Vtable bb_filter1_fir_vtable =
{
  .filter = bb_filter1_fir_run,
  .copy = bb_filter1_fir_copy,
  .run_buffer = bb_filter1_fir_run_buffer,
  .destruct = bb_filter1_fir_destruct
};


/* --- IIR Filters --- */
typedef struct _BbFilter1Iir BbFilter1Iir;
struct _BbFilter1Iir
{
  BbFilter1 base;
  BbTappedDelayLine *input_delay_line;
  BbTappedDelayLine *output_delay_line;
  gdouble *input_delay_coeff_pairs;
  gdouble *output_delay_coeff_pairs;
};

static gdouble
bb_filter1_iir_run (BbFilter1   *filter1,
                    gdouble      input)
{
  BbFilter1Iir *iir = (BbFilter1Iir *) filter1;
  guint input_n_taps = iir->input_delay_line->n_taps;
  guint output_n_taps = iir->output_delay_line->n_taps;
  guint t;
  gdouble rv;
  gdouble *coeff_at = iir->input_delay_coeff_pairs + 1;       /* the odd entries are coefficients */
  bb_tapped_delay_line_tick (iir->input_delay_line, input);
  rv = iir->input_delay_line->taps[0].last_output * *coeff_at;
  coeff_at += 2;
  for (t = 1; t < input_n_taps; t++)
    {
      rv += iir->input_delay_line->taps[t].last_output * *coeff_at;
      coeff_at += 2;
    }
  coeff_at = iir->output_delay_coeff_pairs + 1;
  for (t = 0; t < output_n_taps; t++)
    {
      rv += iir->output_delay_line->taps[t].last_output * *coeff_at;
      coeff_at += 2;
    }
  bb_tapped_delay_line_tick (iir->output_delay_line, rv);
  return rv;
}

static BbFilter1 * bb_filter1_iir_copy (BbFilter1 *in)
{
  BbFilter1Iir *iir = (BbFilter1Iir *) in;
  return bb_filter1_new_linear (1.0,
                                iir->input_delay_line->n_taps,
                                iir->input_delay_coeff_pairs,
                                iir->output_delay_line->n_taps,
                                iir->output_delay_coeff_pairs);
}

static void
bb_filter1_iir_run_buffer (BbFilter1   *filter1,
                           const double *in,
                           double       *out,
                           guint         N)
{
  BbFilter1Iir *iir = (BbFilter1Iir *) filter1;
  guint input_n_taps = iir->input_delay_line->n_taps;
  guint output_n_taps = iir->output_delay_line->n_taps;
  guint i, t;
  for (i = 0; i < N; i++)
    {
      gdouble *coeff_at = iir->input_delay_coeff_pairs + 1;       /* the odd entries are coefficients */
      gdouble rv;
      bb_tapped_delay_line_tick (iir->input_delay_line, in[i]);
      rv = iir->input_delay_line->taps[0].last_output * *coeff_at;
      coeff_at += 2;
      for (t = 1; t < input_n_taps; t++)
        {
          rv += iir->input_delay_line->taps[t].last_output * *coeff_at;
          coeff_at += 2;
        }
      coeff_at = iir->output_delay_coeff_pairs + 1;
      for (t = 0; t < output_n_taps; t++)
        {
          rv += iir->output_delay_line->taps[t].last_output * *coeff_at;
          coeff_at += 2;
        }
      bb_tapped_delay_line_tick (iir->output_delay_line, rv);
      out[i] = rv;
    }
}

static void
bb_filter1_iir_destruct (BbFilter1 *filter1)
{
  BbFilter1Iir *iir = (BbFilter1Iir *) filter1;
  bb_tapped_delay_line_free (iir->input_delay_line);
  bb_tapped_delay_line_free (iir->output_delay_line);
  g_free (iir->input_delay_coeff_pairs);
  g_free (iir->output_delay_coeff_pairs);
}

static BbFilter1Vtable bb_filter1_iir_vtable =
{
  .filter = bb_filter1_iir_run,
  .copy = bb_filter1_iir_copy,
  .run_buffer = bb_filter1_iir_run_buffer,
  .destruct = bb_filter1_iir_destruct
};

BbFilter1 *
bb_filter1_new_linear (gdouble        sampling_rate,
                       guint          n_input_delay_coeffs,
                       const gdouble *input_delay_coeffs,
                       guint          n_output_delay_coeffs,
                       const gdouble *output_delay_coeffs)
{
  if (n_output_delay_coeffs == 0)
    {
      BbFilter1 *rv = bb_filter1_alloc (sizeof (BbFilter1Fir),
                                        &bb_filter1_fir_vtable);
      BbFilter1Fir *fir = (BbFilter1Fir *)rv;
      gdouble *delays;
      guint i;
      fir->delay_coeff_pairs = g_memdup (input_delay_coeffs, n_input_delay_coeffs * sizeof (gdouble) * 2);
      qsort (fir->delay_coeff_pairs, n_input_delay_coeffs, sizeof (gdouble) * 2, bb_compare_doubles);
      delays = g_new (double, n_input_delay_coeffs);
      for (i = 0; i < n_input_delay_coeffs; i++)
        {
          fir->delay_coeff_pairs[2*i+0] *= sampling_rate;
          delays[i] = fir->delay_coeff_pairs[2*i+0];
        }
      fir->delay_line = bb_tapped_delay_line_new (n_input_delay_coeffs, delays);
      g_free (delays);
      return rv;
    }
  else
    {
      BbFilter1 *rv = bb_filter1_alloc (sizeof (BbFilter1Iir),
                                        &bb_filter1_iir_vtable);
      BbFilter1Iir *iir = (BbFilter1Iir *)rv;
      gdouble *delays;
      guint i;
      iir->input_delay_coeff_pairs = g_memdup (input_delay_coeffs, n_input_delay_coeffs * sizeof (gdouble) * 2);
      qsort (iir->input_delay_coeff_pairs, n_input_delay_coeffs, sizeof (gdouble) * 2, bb_compare_doubles);
      iir->output_delay_coeff_pairs = g_memdup (output_delay_coeffs, n_output_delay_coeffs * sizeof (gdouble) * 2);
      qsort (iir->output_delay_coeff_pairs, n_output_delay_coeffs, sizeof (gdouble) * 2, bb_compare_doubles);
      delays = g_new (double, n_input_delay_coeffs + n_output_delay_coeffs);
      for (i = 0; i < n_input_delay_coeffs; i++)
        {
          iir->input_delay_coeff_pairs[2*i+0] *= sampling_rate;
          delays[i] = iir->input_delay_coeff_pairs[2*i+0];
        }
      for (i = 0; i < n_output_delay_coeffs; i++)
        {
          iir->output_delay_coeff_pairs[2*i+0] *= sampling_rate;
          delays[i + n_input_delay_coeffs] = iir->output_delay_coeff_pairs[2*i+0];
        }
      iir->input_delay_line = bb_tapped_delay_line_new (n_input_delay_coeffs, delays);
      iir->output_delay_line = bb_tapped_delay_line_new (n_output_delay_coeffs, delays + n_input_delay_coeffs);
      g_free (delays);
      return rv;
    }
}

