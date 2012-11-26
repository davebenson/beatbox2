#include <stdlib.h>
#include "bb-parse.h"
#include "bb-error.h"

gboolean
bb_parse_value (GValue *value,
                const char *str,
                GError **error)
{
  switch (G_TYPE_FUNDAMENTAL (G_VALUE_TYPE (value)))
    {
    case G_TYPE_DOUBLE:
      {
        char *end;
        double v = strtod (str, &end);
        if (str == end || *end != 0)
          {
            g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                         "bad double (garbage at %s)", end);
            return FALSE;
          }
        g_value_set_double (value, v);
        return TRUE;
      }
    }

  g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
               "no parser for %s", G_VALUE_TYPE_NAME (value));
  return FALSE;
}


