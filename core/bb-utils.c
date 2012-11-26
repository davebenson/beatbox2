#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <gsk/gskutils.h>
#include <gsk/gskghelpers.h>
#include <gsk/gskmacros.h>
#include "bb-utils.h"
#include "bb-duration.h"
#include "bb-error.h"

void
bb_linear      (double *out,
                guint   start,
                double  start_level,
                guint   end,
                double  end_level)
{
  double slope;
  guint i;
  if (start >= end)
    return;
  slope = (end_level - start_level) / (end - start);
  for (i = start; i < end; i++)
    out[i] = (start_level + slope * (i - start));
}

void
bb_rescale (double *inout,
            guint   n,
            double  gain)
{
  guint i;
  for (i = 0; i < n; i++)
    inout[i] *= gain;
}

guint    bb_param_spec_name_hash       (const char *a)
{
  guint len = strlen (a);
  char *copy = g_alloca (len + 1);
  guint i;
  for (i = 0; i < len; i++)
    if (a[i] == '-')
      copy[i] = '_';
    else
      copy[i] = a[i];
  copy[i] = 0;
  return g_str_hash (copy);
}

gboolean bb_param_spec_names_equal     (const char *a,
                                        const char *b)
{
  return bb_param_spec_names_equal_len (a,-1,b,-1);
}

gboolean bb_param_spec_names_equal_len (const char *a, gssize a_len,
                                        const char *b, gssize b_len)
{
  guint i;
  guint len;
  if (a_len < 0)
    a_len = strlen (a);
  if (b_len < 0)
    b_len = strlen (b);
  if (a_len != b_len)
    return FALSE;
  len = a_len;
  for (i = 0; i < len; i++)
    {
      char ca = (a[i] == '-') ? '_' : a[i];
      char cb = (b[i] == '-') ? '_' : b[i];
      if (ca != cb)
        return FALSE;
    }
  return TRUE;
}

char *bb_value_to_string (const GValue *value)
{
  if (G_VALUE_HOLDS_DOUBLE (value))
    return g_strdup_printf ("%.15f", g_value_get_double (value));
  g_return_val_if_reached (NULL);
}

gboolean bb_enum_value_lookup          (GType      enum_type, 
                                        const char *name,
                                        guint      *out)
{
  GEnumClass *c = g_type_class_ref (enum_type);
  GEnumValue *v = g_enum_get_value_by_name (c, name);
  if (v == NULL)
    {
      guint i;
      for (i = 0; i < c->n_values; i++) 
        if (bb_param_spec_names_equal (c->values[i].value_nick, name))
          {
            v = c->values + i;
            break;
          }
    }
  if (v == NULL)
    return FALSE;
  *out = v->value;
  g_type_class_unref (c);
  return TRUE;
}

/* --- interpolation --- */
gdouble     bb_interpolate_double      (double        frac,
                                        double        a,
                                        double        b,
                                        BbInterpolationMode interp_mode)
{
  switch (interp_mode)
  {
  case BB_INTERPOLATION_LINEAR:
    return a * (1.0 - frac) + b * frac;
  case BB_INTERPOLATION_EXPONENTIAL:
    return exp (log (a) * (1.0 - frac) + log (b) * frac);
  case BB_INTERPOLATION_HARMONIC:
    return 1.0 / ((1.0/a) * (1.0 - frac) + (1.0/b) * frac);
  default: 
    g_return_val_if_reached (0);
  }
}

void        bb_interpolate_value       (GValue       *out,
                                        gdouble       frac,
                                        const GValue *a,
                                        const GValue *b,
                                        BbInterpolationMode interp_mode)
{
  GType type = G_VALUE_TYPE (out);
  g_assert (type == G_VALUE_TYPE (a));
  g_assert (type == G_VALUE_TYPE (b));
  switch (G_TYPE_FUNDAMENTAL (type))
    {
    case G_TYPE_DOUBLE:
      g_value_set_double (out, bb_interpolate_double (frac, g_value_get_double (a), g_value_get_double (b), interp_mode));
      break;
    default:
      if (type == BB_TYPE_DURATION)
        {
          BbDuration adur, bdur;
          bb_value_get_duration (a, &adur);
          bb_value_get_duration (b, &bdur);
          if (adur.units != bdur.units)
            g_error ("cannot interpolate between durations that are in different units");
          adur.value = bb_interpolate_double (frac, adur.value, bdur.value, interp_mode);
          bb_value_set_duration (out, adur.units, adur.value);
          break;
        }
      else
        g_error ("cannot interpolate value of type %s", g_type_name (type));
    }
}

gdouble
bb_adjust_double (gdouble     old_value,
                  gdouble     adjustment,
                  BbAdjustmentMode adjust_mode)
{
  switch (adjust_mode)
    {
    case BB_ADJUSTMENT_REPLACE: return adjustment;
    case BB_ADJUSTMENT_MULTIPLY: return old_value * adjustment;
    case BB_ADJUSTMENT_ADD: return old_value + adjustment;
    case BB_ADJUSTMENT_DIVIDE: return old_value / adjustment;
    case BB_ADJUSTMENT_SUBTRACT: return old_value - adjustment;
    default:
      g_return_val_if_reached (0);
    }
}

void        bb_adjust_value            (GValue       *out,
                                        const GValue *in,
                                        BbAdjustmentMode adjust_mode)
{
  GType type = G_VALUE_TYPE (out);
  switch (G_TYPE_FUNDAMENTAL (type))
    {
    case G_TYPE_DOUBLE:
      if (G_VALUE_TYPE (in) != G_TYPE_DOUBLE)
        g_error ("cannot adjust a double with a %s", G_VALUE_TYPE_NAME (in));
      g_value_set_double (out, bb_adjust_double (g_value_get_double (out), g_value_get_double (in), adjust_mode));
      break;
    default:
      if (type == BB_TYPE_DURATION)
        {
          BbDuration dur;
          switch (adjust_mode)
            {
            case BB_ADJUSTMENT_REPLACE:
              {
                if (G_VALUE_TYPE (in) != type)
                  g_error ("cannot replace a %s with a %s", g_type_name (type),
                           G_VALUE_TYPE_NAME (in));
                g_value_copy (in, out);
                break;
              }
            case BB_ADJUSTMENT_MULTIPLY:
              {
                if (G_VALUE_TYPE (in) != G_TYPE_DOUBLE)
                  g_error ("cannot multiply a duration by a %s",
                           G_VALUE_TYPE_NAME (in));
                bb_value_get_duration (out, &dur);
                dur.value *= g_value_get_double (in);
                bb_value_set_duration (out, dur.units, dur.value);
                break;
              }
            case BB_ADJUSTMENT_ADD:
              {
                BbDuration indur;
                if (G_VALUE_TYPE (in) != BB_TYPE_DURATION)
                  g_error ("cannot add a %s to a duration",
                           G_VALUE_TYPE_NAME (in));
                bb_value_get_duration (out, &dur);
                bb_value_get_duration (in, &indur);
                if (dur.units != indur.units)
                  g_error ("cannot add durations of differing units");
                dur.value += indur.value;
                bb_value_set_duration (out, dur.units, dur.value);
                break;
              }
            case BB_ADJUSTMENT_DIVIDE:
              {
                if (G_VALUE_TYPE (in) != G_TYPE_DOUBLE)
                  g_error ("cannot divide a duration by a %s",
                           G_VALUE_TYPE_NAME (in));
                bb_value_get_duration (out, &dur);
                dur.value *= g_value_get_double (in);
                bb_value_set_duration (out, dur.units, dur.value);
                break;
              }
            case BB_ADJUSTMENT_SUBTRACT:
              {
                BbDuration indur;
                if (G_VALUE_TYPE (in) != BB_TYPE_DURATION)
                  g_error ("cannot subtract a %s to a duration",
                           G_VALUE_TYPE_NAME (in));
                bb_value_get_duration (out, &dur);
                bb_value_get_duration (in, &indur);
                if (dur.units != indur.units)
                  g_error ("cannot subtract durations of differing units");
                dur.value -= indur.value;
                bb_value_set_duration (out, dur.units, dur.value);
                break;
              }
            default:
              g_return_if_reached ();
            }
        }
      else
        g_error ("cannot interpolate value of type %s", g_type_name (type));
    }
}

/* --- parsing parameter specs --- */
GParamSpec *bb_parse_g_param_spec      (const char *str,
                                        GError    **error)
{
  const char *at;
  char *type_name, *name;
  const char *start;
  GParamSpec *rv = NULL;
  GType type;
  at = str;
  GSK_SKIP_WHITESPACE (at);
  start = at;
  GSK_SKIP_NONWHITESPACE (at);
  if (start == at)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                   "no type-name found in param spec");
      return NULL;
    }
  type_name = g_strndup (start, at - start);
  GSK_SKIP_WHITESPACE (at);
  start = at;
  GSK_SKIP_NONWHITESPACE (at);
  if (start == at)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                   "no name found in param spec");
      return NULL;
    }
  name = g_strndup (start, at - start);
  if (strcmp (type_name, "double") == 0)
    type = G_TYPE_DOUBLE;
  else if (strcmp (type_name, "string") == 0)
    type = G_TYPE_STRING;
  else
    type = g_type_from_name (type_name);
  if (type == 0)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                   "no type '%s' known", type_name);
      return NULL;
    }
  GSK_SKIP_WHITESPACE (at);

  switch (G_TYPE_FUNDAMENTAL (type))
    {
    case G_TYPE_DOUBLE:
      {
        gdouble min,max,def;
        if (sscanf (at, "%lf %lf %lf", &min,&max,&def) != 3)
          {
            g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                         "error parsing min/max/def triple for double");
            return NULL;
          }
        rv = g_param_spec_double (name, name, NULL, min,max,def, 0);
        break;
      }
    case G_TYPE_STRING:
      {
        char *def;
        if (memcmp (at, "null", 4) == 0)
          {
            def = NULL;
          }
        else if (*at == '"')
          {
            def = gsk_unescape_memory (at, TRUE, NULL, NULL, error);
            if (def == NULL)
              {
                gsk_g_error_add_prefix (error, "error parsing quoted string");
                return NULL;
              }
          }
        else
          {
            g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                         "expected null or quoted-string");
            return NULL;
          }
        rv = g_param_spec_string (name, name, NULL, def, 0);
        g_free (def);
        break;
      }
    case G_TYPE_BOXED:
      {
        rv = g_param_spec_boxed (name, name, NULL, type, 0);
        break;
      }
    default:
      if (type == BB_TYPE_DURATION)
        {
          const char *end = at;
          char *units;
          BbDurationUnits u;
          gdouble v;
          GSK_SKIP_NONWHITESPACE (end);
          units = g_strndup (at, end - at);
          if (!bb_duration_parse_units (units, &u))
            {
              g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                           "cannot parse units for duration from %s for pspec %s", units, name);
              return FALSE;
            }
          g_free (units);
          at = end;
          GSK_SKIP_WHITESPACE (at);
          v = strtod (at, (char **) &end);
          if (at == end)
            {
              g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                           "error parsing default value for duration pspec %s", name);
              return FALSE;
            }
          rv = bb_param_spec_duration (name, name, NULL, u, v, 0);
        }
      else
        {
          g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                       "cannot parse param-spec for type %s (name %s)",
                       type_name, name);
          return FALSE;
        }
    }
  g_assert (rv != NULL);
  g_free (type_name);
  g_free (name);
  return rv;
}

int bb_compare_doubles (gconstpointer a, gconstpointer b)
{
  gdouble da = * (gdouble *) a;
  gdouble db = * (gdouble *) b;
  return (da < db) ? -1 : (da > db) ? 1 : 0;
}

