
#include <gsk/gskutils.h>
#include <gsk/gskmacros.h>
#include <ctype.h>

#define IS_IDENTIFIER_CHAR(c) \
  (g_ascii_isalnum (c) || c == '-' || c == '_')

#define param_spec_frequency()                          \
        g_param_spec_double ("frequency",               \
                             "Frequency",               \
                             "frequency (in hertz)",    \
                             1.0, 44100.0, 440.0,       \
                             G_PARAM_READWRITE)
#define param_spec_duration()                           \
        bb_param_spec_duration ("duration",             \
                                "Duration",             \
                                "duration (in beats)",  \
                                BB_DURATION_UNITS_BEATS, 1.0, \
                                G_PARAM_READWRITE)

#define LBRACE_STR      "{"
#define RBRACE_STR      "}"
