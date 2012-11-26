#include <math.h>

#include "bb-function.h"
#include "bb-error.h"

/* for various type macros */
#include "bb-waveform.h"
#include "bb-filter1.h"
#include "bb-types.h"
#include "bb-duration.h"

static inline BbDuration value_get_duration(const GValue *value)
{
  BbDuration rv;
  bb_value_get_duration (value, &rv);
  return rv;
}
static inline void value_set_duration(GValue *value, BbDuration duration)
{
  bb_value_set_duration (value, duration.units, duration.value);
}


#include "../.generated/binding-impls.inc"

void
bb_vm_init (void)
{
  BbFunctionSignature *sig;
#include "../.generated/binding-regs.inc"
}
