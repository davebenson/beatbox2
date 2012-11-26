#ifndef __BB_RENDER_INFO_H_
#define __BB_RENDER_INFO_H_

typedef struct _BbRenderInfo BbRenderInfo;
struct _BbRenderInfo
{
  double sampling_rate;         /* in samples per second */
  double beat_period;           /* in seconds */
  double note_duration_secs;    /* in seconds */
  double note_duration_beats;   /* in beats */
  unsigned note_duration_samples;
};

#endif
