#include "bb-rate-limiter.h"

BbRateLimiter * bb_rate_limiter_new (void)
{
  BbRateLimiter *limiter = g_new (BbRateLimiter, 1);
  limiter->at_target = TRUE;
  limiter->value = limiter->target = 0;
  limiter->rate = 0.001;
  return limiter;
}

void
bb_rate_limiter_dump       (BbRateLimiter *rate_limiter)
{
  g_message ("rate_limiter: value=%.6f, target=%.6f, rate=%.6f [at_target=%d]",
             rate_limiter->value,
             rate_limiter->target,
             rate_limiter->rate,
             rate_limiter->at_target);
}

void bb_rate_limiter_free (BbRateLimiter *limiter)
{
  g_free (limiter);
}
