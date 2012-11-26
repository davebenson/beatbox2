#ifndef __BB_TYPES_H_
#define __BB_TYPES_H_

#include <glib-object.h>

/* --- value-arrays --- */
typedef struct _BbValueArray BbValueArray;
struct _BbValueArray
{
  guint ref_count;
  guint n_values;
  GValue *values;
};
#define BB_TYPE_VALUE_ARRAY	(bb_value_array_get_type())
GType bb_value_array_get_type (void) G_GNUC_CONST;
BbValueArray *bb_value_array_new_take (guint         n,
                                       GValue       *arr);
BbValueArray *bb_value_array_ref      (BbValueArray *);
void          bb_value_array_unref    (BbValueArray *);

/* --- double-arrays --- */
typedef struct _BbDoubleArray BbDoubleArray;
struct _BbDoubleArray
{
  guint n_values;
  double *values;
};

#define BB_TYPE_DOUBLE_ARRAY	(bb_double_array_get_type())
GType bb_double_array_get_type(void) G_GNUC_CONST;

BbDoubleArray *bb_double_array_new (guint          n_values,
                                    const double  *values);
void           bb_double_array_free(BbDoubleArray *array);
BbDoubleArray *bb_double_array_copy(const BbDoubleArray *array);

/* --- complex numbers --- */
typedef struct _BbComplex BbComplex;
struct _BbComplex
{
  gdouble re, im;
};

#define BB_COMPLEX_MUL(out, a,b)                        \
  G_STMT_START{                                         \
    gdouble _bb_re = ((a).re*(b).re) - ((a).im*(b).im); \
    gdouble _bb_im = ((a).re*(b).im) + ((a).im*(b).re); \
    (out).re = _bb_re;                                  \
    (out).im = _bb_im;                                  \
  }G_STMT_END
#define BB_COMPLEX_ADD(out, a,b)                        \
  G_STMT_START{                                         \
    (out).re = (a).re + (b).re;                         \
    (out).im = (a).im + (b).im;                         \
  }G_STMT_END
#define BB_COMPLEX_SUB(out, a,b)                        \
  G_STMT_START{                                         \
    (out).re = (a).re - (b).re;                         \
    (out).im = (a).im - (b).im;                         \
  }G_STMT_END

/* placeholder for a GValue */
/* only for very limited use within the VM and for
   the code-generator */
#define BB_TYPE_VALUE   (bb_value_get_type())
GType bb_value_get_type (void);

#endif
