
/* was Envelope from Stk */

#include <glib.h>

typedef struct _BbRateLimiter BbRateLimiter;
struct _BbRateLimiter
{
  double value, rate, target;
  gboolean at_target;
};

              BbRateLimiter * bb_rate_limiter_new ();
G_INLINE_FUNC void            bb_rate_limiter_set_value  (BbRateLimiter *rate_limiter,
                                                          double         value);
G_INLINE_FUNC void            bb_rate_limiter_set_target (BbRateLimiter *rate_limiter,
                                                          double         target);

/* "rate" is the max change in value per sample. */
G_INLINE_FUNC void            bb_rate_limiter_set_rate   (BbRateLimiter *rate_limiter,
                                                          double         target);
              void            bb_rate_limiter_dump       (BbRateLimiter *rate_limiter);
G_INLINE_FUNC double          bb_rate_limiter_tick       (BbRateLimiter *rate_limiter);
              void            bb_rate_limiter_free       (BbRateLimiter *rate_limiter);



#if defined (G_CAN_INLINE) || defined (__BB_DEFINE_INLINES__)
G_INLINE_FUNC void            bb_rate_limiter_set_value  (BbRateLimiter *rate_limiter,
                                                          double         value)
{
  rate_limiter->at_target = TRUE;
  rate_limiter->value = value;
  rate_limiter->target = value;
}

G_INLINE_FUNC void            bb_rate_limiter_set_target (BbRateLimiter *rate_limiter,
                                                          double         target)
{
  rate_limiter->target = target;
  rate_limiter->at_target = FALSE;
}

/* "rate" is the max change in value per sample. */
G_INLINE_FUNC void            bb_rate_limiter_set_rate   (BbRateLimiter *rate_limiter,
                                                          double         rate)
{
  rate_limiter->rate = rate;
}

G_INLINE_FUNC double          bb_rate_limiter_tick       (BbRateLimiter *rate_limiter)
{
  if (!rate_limiter->at_target)
    {
      if (rate_limiter->value < rate_limiter->target)
        {
          double n = rate_limiter->value + rate_limiter->rate;
          if (n > rate_limiter->target)
            {
              rate_limiter->value = rate_limiter->target;
              rate_limiter->at_target = TRUE;
            }
          else
            {
              rate_limiter->value = n;
            }
        }
      else if (rate_limiter->value > rate_limiter->target)
        {
          double n = rate_limiter->value - rate_limiter->rate;
          if (n < rate_limiter->target)
            {
              rate_limiter->value = rate_limiter->target;
              rate_limiter->at_target = TRUE;
            }
          else
            {
              rate_limiter->value = n;
            }
        }
      else if (rate_limiter->value == rate_limiter->target)
        rate_limiter->at_target = TRUE;
    }
  return rate_limiter->value;
}
#endif
