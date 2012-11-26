#include "bb-waveform.h"
#include "bb-vm.h"
#include "bb-init.h"
#include "bb-utils.h"
#include <math.h>

GType bb_waveform_get_type (void)
{
  static GType rv = 0;
  if (rv == 0)
    rv = g_boxed_type_register_static ("BbWaveform",
                                       (GBoxedCopyFunc) bb_waveform_ref,
                                       (GBoxedFreeFunc) bb_waveform_unref);
  return rv;
}
BbWaveform *bb_waveform_alloc (BbWaveformVtable *vtable)
{
  BbWaveform *rv = g_malloc (vtable->size);
  g_assert (vtable->size >= sizeof (BbWaveform));
  rv->eval = vtable->eval;
  rv->vtable = vtable;
  rv->ref_count = 1;
  return rv;
}

BbWaveform *bb_waveform_ref   (BbWaveform       *waveform)
{
  g_assert (waveform->ref_count > 0);
  ++(waveform->ref_count);
  return waveform;
}

void        bb_waveform_unref (BbWaveform       *waveform)
{
  g_assert (waveform->ref_count > 0);
  if (--(waveform->ref_count) == 0)
    {
      if (waveform->vtable->destruct)
        waveform->vtable->destruct (waveform);
      g_free (waveform);
    }
}

void       bb_waveform_eval_array (BbWaveform *waveform,
                                   double     *inout,
                                   guint       n)
{
  if (waveform->vtable->eval_array != NULL)
    waveform->vtable->eval_array (waveform, inout, n);
  else
    {
      BbWaveformFunc func = waveform->eval;
      guint i;
      for (i = 0; i < n; i++)
        inout[i] = func (waveform, inout[i]);
    }
}

/* TODO: eventually optimize this to have a special virtual function */
void       bb_waveform_eval_linear(BbWaveform *waveform,
                                   double      start,
                                   double      step,
                                   double     *out,
                                   guint       n)
{
  guint i;
  gdouble at = start;
  for (i = 0; i < n; )
    {
      out[i] = at;
      i++;
      if (i % 0x10000 == 0)
        at = step * i + start;
      else
        at += step;
    }
  bb_waveform_eval_array (waveform, out, n);
}


/* --- composition --- */
typedef struct _BbWaveformCompose BbWaveformCompose;
struct _BbWaveformCompose
{
  guint n_elements;
  BbWaveformComposeElement *elements;
};

static gdouble compose_eval       (BbWaveform *waveform, double in)
{
  guint i;
  BbWaveformCompose *compose = (BbWaveformCompose *) waveform;
  BbWaveformComposeElement *el = compose->elements;
  gdouble rv = 0;
  for (i = 0; i < compose->n_elements; i++, el++)
    if (el->start <= in && in < el->start + el->length)
      rv += bb_waveform_eval (el->waveform, (in - el->start) / el->length);
  return rv;
}

/* need fast lookup table to do this right.. needs
   to take advantage if there are two adjacent lookups */
#if 0
static void    compose_eval_array (BbWaveform *waveform, double *inout, guint n)
{
  ...
}
#endif

static void    compose_destruct   (BbWaveform *waveform)
{
  BbWaveformCompose *compose = (BbWaveformCompose *) waveform;
  BbWaveformComposeElement *el = compose->elements;
  guint i;
  for (i = 0; i < compose->n_elements; i++, el++)
    bb_waveform_unref (el->waveform);
  g_free (compose->elements);
}

static BbWaveformVtable compose_vtable =
{
  .name = "compose",
  .size = sizeof (BbWaveformCompose),
  .eval = compose_eval,
  //.eval_array = compose_eval_array,
  .destruct = compose_destruct
};

BbWaveform *
bb_waveform_new_compose  (guint         n_elements,
			  BbWaveformComposeElement *elements)
{
  BbWaveform *rv = bb_waveform_alloc (&compose_vtable);
  BbWaveformCompose *rv_compose = (BbWaveformCompose *) rv;
  guint i;
  rv_compose->n_elements = n_elements;
  rv_compose->elements = g_memdup (elements, sizeof (BbWaveformComposeElement) * n_elements);
  for (i = 0; i < n_elements; i++)
    bb_waveform_ref (elements[i].waveform);

  /* TODO: build lookup treee? */

  return rv;
}


/* --- linear subwaveform --- */
typedef struct _BbWaveformLinearSubwaveform BbWaveformLinearSubwaveform;
struct _BbWaveformLinearSubwaveform
{
  BbWaveform base;
  BbWaveform *sub;
  gdouble substart, sublength, one_over_sublength;
};

static gdouble linear_subwaveform_eval (BbWaveform *waveform, gdouble in)
{
  BbWaveformLinearSubwaveform *lin = (BbWaveformLinearSubwaveform *) waveform;
  return (in - lin->substart) * lin->one_over_sublength;
}

static void linear_subwaveform_eval_array (BbWaveform *waveform, gdouble *inout, guint n)
{
  BbWaveformLinearSubwaveform *lin = (BbWaveformLinearSubwaveform *) waveform;
  gdouble substart = lin->substart;
  gdouble f = lin->one_over_sublength;
  guint i;
  for (i = 0; i < n; i++)
    inout[i] = (inout[i] - substart) * f;
  bb_waveform_eval_array (lin->sub, inout, n);
}

static void
linear_subwaveform_destruct (BbWaveform *waveform)
{
  BbWaveformLinearSubwaveform *lin = (BbWaveformLinearSubwaveform *) waveform;
  bb_waveform_unref (lin->sub);
}

static BbWaveformVtable linear_subwaveform_vtable =
{
  .name = "linear_subwaveform",
  .size = sizeof (BbWaveformLinearSubwaveform),
  .eval = linear_subwaveform_eval,
  .eval_array = linear_subwaveform_eval_array,
  .destruct = linear_subwaveform_destruct
};

BbWaveform *bb_waveform_new_linear_subwaveform(BbWaveform      *waveform,
                                               gdouble          start,
                                               gdouble          len)
{
  BbWaveform *rv = bb_waveform_alloc (&linear_subwaveform_vtable);
  BbWaveformLinearSubwaveform *lin = (BbWaveformLinearSubwaveform *) rv;
  lin->sub = bb_waveform_ref (waveform);
  lin->substart = start;
  lin->sublength = len;
  lin->one_over_sublength = 1.0 / len;
  return rv;
}

/* linear transform: perform a linear transform on the output of a waveform */
typedef struct _BbWaveformLinearTransform BbWaveformLinearTransform;
struct _BbWaveformLinearTransform
{
  BbWaveform base;
  BbWaveform *sub;
  gdouble scale, offset;
};

static gdouble linear_transform_eval (BbWaveform *waveform, gdouble in)
{
  BbWaveformLinearTransform *xform = (BbWaveformLinearTransform *) waveform;
  return in * xform->scale + xform->offset;
}

static void linear_transform_eval_array (BbWaveform *waveform, gdouble *inout, guint n)
{
  BbWaveformLinearTransform *lin = (BbWaveformLinearTransform *) waveform;
  gdouble scale = lin->scale;
  gdouble offset = lin->offset;
  guint i;
  bb_waveform_eval_array (lin->sub, inout, n);
  for (i = 0; i < n; i++)
    inout[i] = inout[i] * scale + offset;
}

static void
linear_transform_destruct (BbWaveform *waveform)
{
  BbWaveformLinearTransform *xform = (BbWaveformLinearTransform *) waveform;
  bb_waveform_unref (xform->sub);
}

static BbWaveformVtable linear_transform_vtable =
{
  .name = "linear_transform",
  .size = sizeof (BbWaveformLinearTransform),
  .eval = linear_transform_eval,
  .eval_array = linear_transform_eval_array,
  .destruct = linear_transform_destruct
};

BbWaveform *
bb_waveform_new_linear_transform (BbWaveform               *waveform,
			          gdouble                   scale,
			          gdouble                   offset)
{
  BbWaveform *rv = bb_waveform_alloc (&linear_transform_vtable);
  BbWaveformLinearTransform *xform = (BbWaveformLinearTransform *) rv;
  xform->sub = bb_waveform_ref (waveform);
  xform->scale = scale;
  xform->offset = offset;
  return rv;
}

/* --- filter waveforms --- */
typedef struct _BbWaveformFilter BbWaveformFilter;
struct _BbWaveformFilter
{
  BbWaveform base;
  BbWaveform *sub;
  BbFilter1 *filter;
};
static gdouble waveform_filter_eval (BbWaveform *waveform, gdouble in)
{
  BbWaveformFilter *wf = (BbWaveformFilter *) waveform;
  return bb_filter1_run (wf->filter, bb_waveform_eval (wf->sub, in));
}

static void waveform_filter_eval_array (BbWaveform *waveform, gdouble *inout, guint n)
{
  BbWaveformFilter *wf = (BbWaveformFilter *) waveform;
  bb_waveform_eval_array (wf->sub, inout, n);
  bb_filter1_run_buffer (wf->filter, inout, inout, n);
}

static void
waveform_filter_destruct (BbWaveform *waveform)
{
  BbWaveformFilter *wf = (BbWaveformFilter *) waveform;
  bb_waveform_unref (wf->sub);
  bb_filter1_free (wf->filter);
}

static BbWaveformVtable filter_vtable =
{
  .name = "filter",
  .size = sizeof (BbWaveformFilter),
  .eval = waveform_filter_eval,
  .eval_array = waveform_filter_eval_array,
  .destruct = waveform_filter_destruct
};

BbWaveform *bb_waveform_new_take_filter(BbWaveform               *waveform,
                                        BbFilter1                *filter)
{
  BbWaveform *rv = bb_waveform_alloc (&filter_vtable);
  BbWaveformFilter *rv_f = (BbWaveformFilter *) rv;
  rv_f->sub = bb_waveform_ref (waveform);
  rv_f->filter = filter;
  return rv;
}
BbWaveform *bb_waveform_new_filter   (BbWaveform               *waveform,
                                      BbFilter1                *filter)
{
  return bb_waveform_new_take_filter (waveform, bb_filter1_copy (filter));
}

/* --- rescaling times --- */
typedef struct _BbWaveformAdjustAttackDecay BbWaveformAdjustAttackDecay;
struct _BbWaveformAdjustAttackDecay
{
  BbWaveform base;
  BbWaveform *subwaveform;
  gdouble old_attack_time;
  gdouble new_attack_time;
  gdouble old_decay_time;
  gdouble new_decay_time;
  gdouble scales[3];            /* multiplication factors for each of the three phases */
};
static gdouble adjust_attack_decay_eval (BbWaveform *waveform, gdouble in)
{
  BbWaveformAdjustAttackDecay *aad = (BbWaveformAdjustAttackDecay *) waveform;
  if (in <= 0.0)
    in = 0;
  else if (in < aad->old_attack_time)
    in *= aad->scales[0];
  else if (in < aad->old_decay_time)
    {
      in -= aad->old_attack_time;
      in *= aad->scales[1];
      in += aad->new_attack_time;
    }
  else if (in >= 1.0)
    {
      in = 1.0;
    }
  else
    {
      in -= aad->old_decay_time;
      in *= aad->scales[2];
      in += aad->new_decay_time;
    }
  return bb_waveform_eval (aad->subwaveform, in);
}

static void adjust_attack_decay_eval_array (BbWaveform *waveform, gdouble *inout, guint n)
{
  guint i = 0;
  BbWaveformAdjustAttackDecay *aad = (BbWaveformAdjustAttackDecay *) waveform;
  while (i < n)
    {
      gdouble in = inout[i];
      if (in <= 0.0)
        {
          do
            inout[i++] = 0;
          while (i < n && inout[i] <= 0.0);
        }
      else if (in < aad->old_attack_time)
        {
          gdouble old_attack_time = aad->old_attack_time;
          gdouble scale = aad->scales[0];
          do
            inout[i++] *= scale;
          while (i < n && inout[i] < old_attack_time);
        }
      else if (in < aad->old_decay_time)
        {
          gdouble old_attack_time = aad->old_attack_time;
          gdouble new_attack_time = aad->new_attack_time;
          gdouble old_decay_time = aad->old_decay_time;
          gdouble scale = aad->scales[1];
          do
            {
              in = inout[i];
              inout[i++] = (in - old_attack_time) * scale + new_attack_time;
            }
          while (i < n && old_attack_time <= in && in < old_decay_time);
        }
      else if (in >= 1.0)
        {
          do
            inout[i++] = 1.0;
          while (i < n && inout[i] >= 1.0);
        }
      else
        {
          gdouble new_decay_time = aad->new_decay_time;
          gdouble old_decay_time = aad->old_decay_time;
          gdouble scale = aad->scales[2];
          do
            {
              in = inout[i];
              inout[i++] = (in - old_decay_time) * scale + new_decay_time;
            }
          while (i < n && old_decay_time <= inout[i] && inout[i] < 1.0);
        }
    }
  bb_waveform_eval_array (aad->subwaveform, inout, n);
}

static void
adjust_attack_decay_destruct (BbWaveform *waveform)
{
  BbWaveformAdjustAttackDecay *aad = (BbWaveformAdjustAttackDecay *) waveform;
  bb_waveform_unref (aad->subwaveform);
}

static BbWaveformVtable adjust_attack_decay_vtable =
{
  .name = "adjust_attack_decay",
  .size = sizeof (BbWaveformAdjustAttackDecay),
  .eval = adjust_attack_decay_eval,
  .eval_array = adjust_attack_decay_eval_array,
  .destruct = adjust_attack_decay_destruct
};


BbWaveform *bb_waveform_new_adjust_attack_decay (BbWaveform           *subwaveform,
                                                 gdouble               old_attack_time,
                                                 gdouble               new_attack_time,
                                                 gdouble               old_decay_time,
                                                 gdouble               new_decay_time)
{
  BbWaveform *rv = bb_waveform_alloc (&adjust_attack_decay_vtable);
  BbWaveformAdjustAttackDecay *rv_aad = (BbWaveformAdjustAttackDecay *) rv;
  rv_aad->old_attack_time = old_attack_time;
  rv_aad->new_attack_time = new_attack_time;
  rv_aad->old_decay_time = old_decay_time;
  rv_aad->new_decay_time = new_decay_time;
  rv_aad->scales[0] = new_attack_time / old_attack_time;
  rv_aad->scales[1] = (new_decay_time - new_attack_time) / (old_decay_time - old_attack_time);
  rv_aad->scales[2] = (1.0 - new_decay_time) / (1.0 - old_decay_time);
  rv_aad->subwaveform = bb_waveform_ref (subwaveform);
  return rv;
}

/* --- abssinblank --- */
static gdouble abssinblank_eval (BbWaveform *waveform, gdouble in)
{
  if (in >= 0.5)
    return 0;
  else if (in >= 0.25)
    return sin ((in-0.25) * BB_4PI);
  else
    return sin (in * BB_4PI);
}

static void abssinblank_eval_array (BbWaveform *waveform, gdouble *inout, guint n)
{
  guint i = 0;
  while (i < n)
    {
      if (inout[i] >= 0.5)
        {
          inout[i++] = 0;
          while (i < n && inout[i] >= 0.5)
            inout[i++] = 0;
        }
      else if (inout[i] >= 0.25)
        {
          inout[i] = sin ((inout[i]-0.25) * BB_4PI);
          i++;
          while (i < n && 0.25 <= inout[i] && inout[i] <= 0.5)
            {
              inout[i] = sin ((inout[i]-0.25) * BB_4PI);
              i++;
            }
        }
      else
        {
          inout[i] = sin (inout[i] * BB_4PI);
          i++;
          while (i < n && inout[i] <= 0.5)
            {
              inout[i] = sin (inout[i] * BB_4PI);
              i++;
            }
        }
    }
}

static BbWaveformVtable abssinblank_vtable =
{
  .name = "abssinblank",
  .size = sizeof (BbWaveform),
  .eval = abssinblank_eval,
  .eval_array = abssinblank_eval_array,
};

BbWaveform *bb_waveform_new_abssinblank      (void)
{
  static BbWaveform *the_abssinblank = NULL;
  if (the_abssinblank == NULL)
    the_abssinblank = bb_waveform_alloc (&abssinblank_vtable);
  return bb_waveform_ref (the_abssinblank);
}

/* --- sin --- */
static gdouble sin_eval (BbWaveform *waveform, gdouble in)
{
  return sin (in * 2.0 * M_PI);
}

static void sin_eval_array (BbWaveform *waveform, gdouble *inout, guint n)
{
  guint i;
  for (i = 0; i < n; i++)
    inout[i] = sin (inout[i] * 2.0 * M_PI);
}

static BbWaveformVtable sin_vtable =
{
  .name = "sin",
  .size = sizeof (BbWaveform),
  .eval = sin_eval,
  .eval_array = sin_eval_array,
};

BbWaveform *bb_waveform_new_sin      (void)
{
  static BbWaveform *the_sin = NULL;
  if (the_sin == NULL)
    the_sin = bb_waveform_alloc (&sin_vtable);
  return bb_waveform_ref (the_sin);
}

/* --- triangle --- */
typedef struct _BbWaveformTriangle BbWaveformTriangle;
struct _BbWaveformTriangle
{
  BbWaveform base;
  double lo_val, hi_minus_lo;
};
static gdouble triangle_eval (BbWaveform *waveform, gdouble in)
{
  BbWaveformTriangle *tri = (BbWaveformTriangle *) waveform;
  if (in < 0.5)
    return (in * 2.0) * tri->hi_minus_lo + tri->lo_val;
  else
    return (2.0 - in * 2.0) * tri->hi_minus_lo + tri->lo_val;
}

static void triangle_eval_array (BbWaveform *waveform, gdouble *inout, guint n)
{
  guint i;
  BbWaveformTriangle *tri = (BbWaveformTriangle *) waveform;
  double hi_minus_lo = tri->hi_minus_lo;
  double lo_val = tri->lo_val;
  for (i = 0; i < n; i++)
    {
      double raw;

      /* compute the value normalized to 0..1 */
      if (inout[i] < 0.5)
        raw = inout[i] * 2.0;
      else
        raw = (2.0 - inout[i] * 2.0);

      /* rescale to lo_val..hi_val */
      inout[i] = raw * hi_minus_lo + lo_val;
    }
}

static BbWaveformVtable triangle_vtable =
{
  .name = "triangle",
  .size = sizeof (BbWaveformTriangle),
  .eval = triangle_eval,
  .eval_array = triangle_eval_array,
};

BbWaveform *bb_waveform_new_triangle (double low_val, double hi_val)
{
  BbWaveform *rv = bb_waveform_alloc (&triangle_vtable);
  BbWaveformTriangle *tri = (BbWaveformTriangle *) rv;
  tri->lo_val = low_val;
  tri->hi_minus_lo = hi_val - low_val;
  return rv;
}

/* --- square --- */
typedef struct _BbWaveformSquare BbWaveformSquare;
struct _BbWaveformSquare
{
  BbWaveform base;
  gdouble duty_cycle;
  gdouble off_value;
  gdouble on_value;
};

static gdouble square_eval (BbWaveform *waveform, gdouble in)
{
  BbWaveformSquare *sq = (BbWaveformSquare *) waveform;
  if (in < sq->duty_cycle)
    return sq->on_value;
  else
    return sq->off_value;
}

static void square_eval_array (BbWaveform *waveform, gdouble *inout, guint n)
{
  BbWaveformSquare *sq = (BbWaveformSquare *) waveform;
  guint i;
  double duty_cycle = sq->duty_cycle;
  double off_value = sq->off_value;
  double on_value = sq->on_value;
  for (i = 0; i < n; i++)
    if (inout[i] < duty_cycle)
      inout[i] = on_value;
    else
      inout[i] = off_value;
}

static BbWaveformVtable square_vtable =
{
  .name = "square",
  .size = sizeof (BbWaveformSquare),
  .eval = square_eval,
  .eval_array = square_eval_array,
};

BbWaveform *bb_waveform_new_square   (gdouble                   duty_cycle,
                                      gdouble                   off_value,
                                      gdouble                   on_value)
{
  BbWaveform *rv = bb_waveform_alloc (&square_vtable);
  BbWaveformSquare *sq = (BbWaveformSquare *) rv;
  sq->duty_cycle = duty_cycle;
  sq->off_value = off_value;
  sq->on_value = on_value;
  return rv;
}

/* --- constant --- */
typedef struct _BbWaveformConstant BbWaveformConstant;
struct _BbWaveformConstant
{
  BbWaveform base;
  gdouble value;
};

static gdouble constant_eval (BbWaveform *waveform, gdouble in)
{
  BbWaveformConstant *constant = (BbWaveformConstant *) waveform;
  return constant->value;
}

static void constant_eval_array (BbWaveform *waveform, gdouble *inout, guint n)
{
  BbWaveformConstant *constant = (BbWaveformConstant *) waveform;
  guint i;
  double value = constant->value;
  for (i = 0; i < n; i++)
    inout[i] = value;
}

static BbWaveformVtable constant_vtable =
{
  .name = "constant",
  .size = sizeof (BbWaveformConstant),
  .eval = constant_eval,
  .eval_array = constant_eval_array,
};

BbWaveform *bb_waveform_new_constant (gdouble                   value)
{
  BbWaveform *rv = bb_waveform_alloc (&constant_vtable);
  BbWaveformConstant *constant = (BbWaveformConstant *) rv;
  constant->value = value;
  return rv;
}

/* --- linear --- */
typedef struct _BbWaveformLinear BbWaveformLinear;
struct _BbWaveformLinear
{
  BbWaveform base;
  guint n_tv_pairs;   /* time/volume pairs: half the number of doubles */
  double *tv_pairs;
};

static gdouble
linear_eval (BbWaveform *waveform,
             gdouble     in)
{
  BbWaveformLinear *linear = (BbWaveformLinear *) waveform;
  double t0,v0,t1,v1;
  guint i;

  /* TODO: binary search */
  for (i = 0; i + 1 < linear->n_tv_pairs; i++)
    if (linear->tv_pairs[2 * i + 2] >= in)
      break;

  /* interpolate between (i) and (i+1) times */
  t0 = linear->tv_pairs[2 * i + 0];
  v0 = linear->tv_pairs[2 * i + 1];
  t1 = linear->tv_pairs[2 * i + 2];
  v1 = linear->tv_pairs[2 * i + 3];

  /* note: this is important.  it's a tricky divide by zero guard,
     since t0 <= t1, by sorting which is done on construction.
     of course, if t0==t1==in we should probably return the average
     of v0,v1, but who cares... */
  if (t0 == in)
    return v0;
  else if (t1 == in)
    return v1;

  return (in - t0) * (v1 - v0) / (t1 - t0) + v0;
}

static void
linear_eval_array (BbWaveform *waveform,
                   gdouble *inout, guint n)
{
  BbWaveformLinear *linear = (BbWaveformLinear *) waveform;
  guint n_tv = linear->n_tv_pairs;
  const gdouble *pairs = linear->tv_pairs;
  guint i;
  guint section;

  /* TODO: binary search */
  for (section = 0; section + 1 < linear->n_tv_pairs; section++)
    if (linear->tv_pairs[2 * section + 2] >= inout[0])
      break;

  for (i = 0; i < n; i++)
    {
      gdouble t = inout[i];

      /* check that we are in the right section */
      while (section > 0 && pairs[2 * section] > t)
        section--;
      while (section + 1 < n_tv && pairs[2 * section + 2] < t)
        section++;

      {
        const double *TV = pairs + section * 2;
        double t0 = TV[0];
        double v0 = TV[1];
        double t1 = TV[2];
        double v1 = TV[3];
        if (t0 == t)
          inout[i] = v0;
        else if (t1 == t)
          inout[i] = v1;
        else
          inout[i] = (t - t0) * (v1 - v0) / (t1 - t0) + v0;
      }
    }
}

static BbWaveformVtable linear_vtable =
{
  .name = "linear",
  .size = sizeof (BbWaveformLinear),
  .eval = linear_eval,
  .eval_array = linear_eval_array,
};

BbWaveform *
bb_waveform_new_linear (guint          n_tv_pairs,
                        const gdouble *tv_pairs)
{
  BbWaveform *rv = bb_waveform_alloc (&linear_vtable);
  BbWaveformLinear *linear = (BbWaveformLinear *) rv;
  linear->n_tv_pairs = n_tv_pairs;
  linear->tv_pairs = g_memdup (tv_pairs, sizeof (gdouble) * 2 * n_tv_pairs);
  return rv;
}

/* --- exp --- */
typedef struct _BbWaveformExp BbWaveformExp;
struct _BbWaveformExp
{
  BbWaveform base;
  guint n_tv_pairs;   /* time/volume pairs: half the number of doubles */
  double *tv_pairs;
  double *transition_params;
};

static gdouble
exp_eval (BbWaveform *waveform,
             gdouble     in)
{
  BbWaveformExp *wexp = (BbWaveformExp *) waveform;
  double t0,v0,t1,v1;
  guint i;
  double param;
  gdouble rv;

  /* TODO: binary search */
  for (i = 0; i + 1 < wexp->n_tv_pairs; i++)
    if (wexp->tv_pairs[2 * i + 2] >= in)
      break;

  /* interpolate between (i) and (i+1) times */
  t0 = wexp->tv_pairs[2 * i + 0];
  v0 = wexp->tv_pairs[2 * i + 1];
  t1 = wexp->tv_pairs[2 * i + 2];
  v1 = wexp->tv_pairs[2 * i + 3];
  param = wexp->transition_params[i];

  /* note: this is important.  it's a tricky divide by zero guard,
     since t0 <= t1, by sorting which is done on construction.
     of course, if t0==t1==in we should probably return the average
     of v0,v1, but who cares... */
  if (t0 == in)
    return v0;
  else if (t1 == in)
    return v1;

  rv = (in - t0) / (t1 - t0);
  if (param != 0)
    rv = (1 - exp (rv * param)) / (1 - exp (param));
  return rv * (v1 - v0) + v0;
}

static void
exp_eval_array (BbWaveform *waveform,
                   gdouble *inout, guint n)
{
  BbWaveformExp *wexp = (BbWaveformExp *) waveform;
  guint n_tv = wexp->n_tv_pairs;
  const gdouble *pairs = wexp->tv_pairs;
  guint i;
  guint section;

  /* TODO: binary search */
  for (section = 0; section + 1 < wexp->n_tv_pairs; section++)
    if (wexp->tv_pairs[2 * section + 2] >= inout[0])
      break;

  for (i = 0; i < n; i++)
    {
      gdouble t = inout[i];

      /* check that we are in the right section */
      while (section > 0 && pairs[2 * section] > t)
        section--;
      while (section + 1 < n_tv && pairs[2 * section + 2] < t)
        section++;

      {
        const double *TV = pairs + section * 2;
        double t0 = TV[0];
        double v0 = TV[1];
        double t1 = TV[2];
        double v1 = TV[3];
        gdouble param = wexp->transition_params[section];
        if (t0 == t)
          inout[i] = v0;
        else if (t1 == t)
          inout[i] = v1;
        else
          {
            t = (t - t0) / (t1 - t0);
            if (param != 0)
              t = (1 - exp (t * param)) / (1 - exp(param));
            inout[i] = t * (v1 - v0) + v0;
          }
      }
    }
}

static BbWaveformVtable exp_vtable =
{
  .name = "exp",
  .size = sizeof (BbWaveformExp),
  .eval = exp_eval,
  .eval_array = exp_eval_array,
};

BbWaveform *
bb_waveform_new_exp (guint          n_tv_pairs,
                     const gdouble *tv_pairs,
                     const gdouble *transition_params)
{
  BbWaveform *rv = bb_waveform_alloc (&exp_vtable);
  BbWaveformExp *exp = (BbWaveformExp *) rv;
  g_assert (n_tv_pairs > 1);
  exp->n_tv_pairs = n_tv_pairs;
  exp->tv_pairs = g_memdup (tv_pairs, sizeof (gdouble) * 2 * n_tv_pairs);
  exp->transition_params = g_memdup (transition_params, sizeof (gdouble) * (n_tv_pairs-1));
  return rv;
}

/* --- Harmonic waveform --- */
/* this waveform interpolates the reciprocal linearly over the range;
   hence the range must not include 0 */
typedef struct _BbWaveformHarmonic BbWaveformHarmonic;
struct _BbWaveformHarmonic
{
  BbWaveform base;
  gdouble one_over_start;
  gdouble one_over_end;
};
static gdouble
harmonic_eval (BbWaveform *waveform,
               gdouble     in)
{
  BbWaveformHarmonic *harm = (BbWaveformHarmonic *) waveform;
  gdouble one_over_output = (1.0 - in) * harm->one_over_start
                          + in * harm->one_over_end;
  return 1.0 / one_over_output;
}
static void
harmonic_eval_array (BbWaveform *waveform,
                     gdouble *inout,
                     guint n)
{
  BbWaveformHarmonic *harm = (BbWaveformHarmonic *) waveform;
  gdouble one_over_start = harm->one_over_start;
  gdouble one_over_end = harm->one_over_end;
  guint i;
  for (i = 0; i < n; i++)
    {
      gdouble in = *inout;
      gdouble one_over_output = (1.0 - in) * one_over_start
                              + in * one_over_end;
      *inout = 1.0 / one_over_output;
      inout++;
    }
}


static BbWaveformVtable harmonic_vtable =
{
  .name = "harmonic",
  .size = sizeof (BbWaveformHarmonic),
  .eval = harmonic_eval,
  .eval_array = harmonic_eval_array,
};

BbWaveform *
bb_waveform_new_harmonic (gdouble start_val,
                          gdouble end_val)
{
  BbWaveform *rv = bb_waveform_alloc (&harmonic_vtable);
  BbWaveformHarmonic *harm = (BbWaveformHarmonic *) rv;
  g_message ("start_val,end_val=%.6f,%.6f",start_val,end_val);
  if ((start_val <= 0.0 && end_val >= 0.0)
   || (start_val >= 0.0 && end_val <= 0.0))
    g_warning ("bb_waveform_new_harmonic called with start_val/end_val containing 0: may cause infinities (leading possibly to silence)");
  harm->one_over_start = 1.0 / start_val;
  harm->one_over_end = 1.0 / end_val;
  return rv;
}

/* --- Semicircle waveform --- */
static inline gdouble semicircle (gdouble in)
{
  if (in > 0.999999999
   || in < -0.999999999)
    return 0;
  else
    return sqrt (1.0 - in * in);
}

static gdouble semicircle_eval (BbWaveform *waveform, gdouble in)
{
  return semicircle (in);
}

static void semicircle_eval_array (BbWaveform *waveform, gdouble *inout, guint n)
{
  guint i;
  for (i = 0; i < n; i++)
    inout[i] = semicircle (inout[i]);
}

static BbWaveformVtable semicircle_vtable =
{
  .name = "semicircle",
  .size = sizeof (BbWaveform),
  .eval = semicircle_eval,
  .eval_array = semicircle_eval_array,
};

BbWaveform *bb_waveform_new_semicircle      (void)
{
  return bb_waveform_alloc (&semicircle_vtable);
}


/* --- Sin-Envelope --- */
/* (1-cos(2*pi*value)) / 2 */
static gdouble sin_envelope_eval (BbWaveform *waveform, gdouble in)
{
  return (1.0 - cos (BB_2PI * in)) * 0.5;
}

static void sin_envelope_eval_array (BbWaveform *waveform, gdouble *inout, guint n)
{
  guint i;
  for (i = 0; i < n; i++)
    inout[i] = (1.0 - cos (BB_2PI * inout[i])) * 0.5;
}

static BbWaveformVtable sin_envelope_vtable =
{
  .name = "sin_envelope",
  .size = sizeof (BbWaveform),
  .eval = sin_envelope_eval,
  .eval_array = sin_envelope_eval_array,
};

BbWaveform *bb_waveform_new_sin_envelope      (void)
{
  return bb_waveform_alloc (&sin_envelope_vtable);
}
