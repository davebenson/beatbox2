#ifndef __BB_PARSE_H_
#define __BB_PARSE_H_

#include <glib-object.h>
gboolean
bb_parse_value (GValue *value,
                const char *str,
                GError **error);


#endif
