#include "bb-init.h"
#include <glib-object.h>
#include "bb-duration.h"


#include "../.generated/init-headers.inc"             /* made by 'gen-init.pl' */
void bb_init ()
{
  g_type_init ();
#include "../.generated/init-code.inc"                /* made by 'gen-init.pl' */
}
