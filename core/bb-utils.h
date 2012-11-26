#ifndef __BB_UTILS_H_
#define __BB_UTILS_H_

#include <glib-object.h>
void
bb_linear      (double *out,
                guint   start,
                double  start_level,
                guint   end,
                double  end_level);
void
bb_rescale (double *inout,
            guint   n,
            double  gain);

guint    bb_param_spec_name_hash       (const char *a);
gboolean bb_param_spec_names_equal     (const char *a,
                                        const char *b);
gboolean bb_param_spec_names_equal_len (const char *a, gssize a_len,
                                        const char *b, gssize b_len);
gboolean bb_enum_value_lookup          (GType      enum_type, 
                                        const char *name,
                                        guint      *out);

GParamSpec *bb_parse_g_param_spec      (const char *str,
                                        GError    **error);


typedef enum
{
  BB_INTERPOLATION_LINEAR,
  BB_INTERPOLATION_EXPONENTIAL,
  BB_INTERPOLATION_HARMONIC,		/* i.e. under reciprocal */
} BbInterpolationMode;

#define BB_TYPE_INTERPOLATION_MODE	(bb_interpolation_mode_get_type())
GType bb_interpolation_mode_get_type(void) G_GNUC_CONST;

typedef enum
{
  BB_ADJUSTMENT_REPLACE,
  BB_ADJUSTMENT_MULTIPLY,
  BB_ADJUSTMENT_ADD,
  BB_ADJUSTMENT_DIVIDE,
  BB_ADJUSTMENT_SUBTRACT,
} BbAdjustmentMode;

#define BB_TYPE_ADJUSTMENT_MODE	(bb_adjustment_mode_get_type())
GType bb_adjustment_mode_get_type(void) G_GNUC_CONST;

gdouble     bb_interpolate_double      (double        frac,
                                        double        a,
                                        double        b,
                                        BbInterpolationMode interp_mode);
void        bb_interpolate_value       (GValue       *out,
                                        gdouble       frac,
                                        const GValue *a,
                                        const GValue *b,
                                        BbInterpolationMode interp_mode);
gdouble     bb_adjust_double           (gdouble     old_value,
                                        gdouble     adjustment,
                                        BbAdjustmentMode adjust_mode);
void        bb_adjust_value            (GValue       *out,
                                        const GValue *in,
                                        BbAdjustmentMode adjust_mode);
/* qsort-usable functions */
gint bb_compare_doubles (gconstpointer a, gconstpointer b);
gint bb_compare_pstrings (gconstpointer a, gconstpointer b);

/* serialize a value, for debugging */
char    *bb_value_to_string (const GValue *value);
#define BB_VALUE_INIT	{0,{{0,}}}

#define BB_PI_2 		1.5707963267949
#define BB_2_OVER_PI            0.63661977236758
#define BB_2PI			6.28318530717959
#define BB_4PI                  12.5663706143592
#define BB_6PI                  18.8495559215388
#define BB_8PI                  25.1327412287184



#endif
