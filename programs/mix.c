#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <gsk/gskutils.h>
#include "mix.h"
#include "../core/bb-utils.h"

typedef struct _Sequence Sequence;

G_LOCK_DEFINE_STATIC (sequence_lock);
#define LOCK()      G_LOCK (sequence_lock)
#define UNLOCK()    G_UNLOCK (sequence_lock)

G_LOCK_DEFINE_STATIC (time);

struct _Sequence
{
  gboolean valid;
  gdouble *samples;
  gdouble volume;
  gdouble target_volume;
  guint at;
};

static guint sequences_alloced = 0;
static Sequence *sequences = NULL;

static guint loop_length, loop_at;

/* globals set-up by mix_backend_init() */
static gdouble sampling_rate;

static gdouble max_volume_delta;

/* prototypes of backend-specific functions */
static void mix_backend_init (int          *argc_inout,
			      const char ***argv_inout);

void          mix_init           (int          *argc_inout,
                                  const char ***argv_inout,
			          gdouble       loop_duration)
{
  mix_backend_init (argc_inout, argv_inout);
  loop_length = loop_duration * sampling_rate;
  mix_set_volume_rate (100.0);
}

guint         mix_add_sequence   (guint         length,
                                  gdouble      *samples)
{
  guint rv;
  for (rv = 0; rv < sequences_alloced; rv++)
    if (!sequences[rv].valid)
      break;
  if (rv == sequences_alloced)
    {
      guint alloced;
      guint i;
      if (sequences_alloced == 0)
        alloced = 16;
      else 
	alloced = sequences_alloced * 2;
      LOCK ();
      sequences = g_renew (Sequence, sequences, alloced);
      for (i = sequences_alloced; i < alloced; i++)
        sequences[i].valid = FALSE;
      sequences_alloced = alloced;
      UNLOCK ();
    }
  if (length < loop_length)
    {
      guint tmp = length;
      samples = g_renew (gdouble, samples, loop_length);
      while (tmp < loop_length)
        samples[tmp++] = 0;
    }
  LOCK ();
  sequences[rv].samples = samples;
  sequences[rv].at = loop_at;
  sequences[rv].volume = sequences[rv].target_volume = 0;
  sequences[rv].valid = TRUE;
  UNLOCK ();
  return rv;
}

void          mix_set_volume     (guint         sequence,
                                  gdouble       volume)
{
  g_assert (sequence < sequences_alloced && sequences[sequence].valid);
  LOCK ();
  sequences[sequence].target_volume = volume;
  UNLOCK ();
}

void
mix_set_volume_rate(gdouble full_changes_per_second)
{
  LOCK ();
  max_volume_delta = full_changes_per_second / sampling_rate;
  UNLOCK ();
}

void
mix_remove_sequence(guint         sequence)
{
  g_assert (sequence < sequences_alloced);
  g_assert (sequences[sequence].valid);
  LOCK ();
  sequences[sequence].valid = 0;
  UNLOCK ();
}

guint
mix_get_n_samples (void)
{
  return loop_length;
}

/* --- rendering --- */
static void
mix_render_audio   (gdouble      *out,
                    guint         n_out)
{
  guint i;
  if (n_out == 0)
    return;
  LOCK ();
  for (i = 0; i < sequences_alloced; i++)
    if (sequences[i].valid)
      {
        /* render audio */
        guint in_at = sequences[i].at;
        guint out_at = 0;
        gdouble vol = sequences[i].volume;
        gdouble target_vol = sequences[i].target_volume;
        if (sequences[i].volume != sequences[i].target_volume)
          {
            if (sequences[i].volume < sequences[i].target_volume)
              {
                do
                  {
                    out[out_at++] = sequences[i].samples[in_at++] * vol;
                    vol += max_volume_delta;
                    if (vol > target_vol)
                      vol = target_vol;
                    if (in_at == loop_length)
                      in_at = 0;
                  }
                while (vol < target_vol && out_at < n_out);
              }
            else /* if (sequences[i].volume > sequences[i].target_volume) */
              {
                do
                  {
                    out[out_at++] = sequences[i].samples[in_at++] * vol;
                    vol -= max_volume_delta;
                    if (vol < target_vol)
                      vol = target_vol;
                    if (in_at == loop_length)
                      in_at = 0;
                  }
                while (vol > target_vol && out_at < n_out);
              }
          }
        sequences[i].volume = vol;
        for (    ; out_at < n_out; out_at++)
          {
            out[out_at] = sequences[i].samples[in_at++] * vol;
            if (in_at == loop_length)
              in_at = 0;
          }
        G_LOCK (time);
        sequences[i].at = in_at;
        G_UNLOCK (time);
        break;
      }
  if (i == sequences_alloced)
    {
      /* silent */
      memset (out, 0, sizeof (double) * n_out);
      loop_at = (n_out + loop_at) % loop_length;
      UNLOCK ();
      return;
    }
  i++;
  for (    ; i < sequences_alloced; i++)
    if (sequences[i].valid)
      {
        /* render audio, adding */
        guint in_at = sequences[i].at;
        guint out_at = 0;
        gdouble vol = sequences[i].volume;
        gdouble target_vol = sequences[i].target_volume;
        if (sequences[i].volume != sequences[i].target_volume)
          {
            if (sequences[i].volume < sequences[i].target_volume)
              {
                do
                  {
                    out[out_at++] += sequences[i].samples[in_at++] * vol;
                    vol += max_volume_delta;
                    if (vol > target_vol)
                      vol = target_vol;
                    if (in_at == loop_length)
                      in_at = 0;
                  }
                while (vol < target_vol && out_at < n_out);
              }
            else /* if (sequences[i].volume > sequences[i].target_volume) */
              {
                do
                  {
                    out[out_at++] += sequences[i].samples[in_at++] * vol;
                    vol -= max_volume_delta;
                    if (vol < target_vol)
                      vol = target_vol;
                    if (in_at == loop_length)
                      in_at = 0;
                  }
                while (vol > target_vol && out_at < n_out);
              }
          }
        /* assert: volume==target_volume */
        sequences[i].volume = vol;
        for (    ; out_at < n_out; out_at++)
          {
            out[out_at] += sequences[i].samples[in_at++] * vol;
            if (in_at == loop_length)
              in_at = 0;
          }
        G_LOCK (time);
        sequences[i].at = in_at;
        G_UNLOCK (time);
      }
  G_LOCK (time);
  loop_at += n_out;
  loop_at %= loop_length;
  G_UNLOCK (time);
  UNLOCK ();
}

void
mix_sequence_get_time      (guint         sequence,
                            guint        *cur_sample,
                            guint        *offset,
                            guint        *total_samples)
{
  g_assert (sequence < sequences_alloced);
  g_assert (sequences[sequence].valid);
  G_LOCK (time);
  if (cur_sample)
    *cur_sample = sequences[sequence].at;
  if (offset)
    {
      if (sequences[sequence].at >= loop_at)
        *offset = sequences[sequence].at - loop_at;
      else
        *offset = loop_length + sequences[sequence].at - loop_at;
    }
  if (total_samples)
    *total_samples = loop_length;
  G_UNLOCK (time);
}

void
mix_get_time (guint        *cur_sample,
              guint        *total_samples)
{
  G_LOCK (time);
  if (cur_sample)
    *cur_sample = loop_at;
  if (total_samples)
    *total_samples = loop_length;
  G_UNLOCK (time);
}

void
mix_sequence_set_offset (guint      sequence,
                         guint      samples_offset)
{
  g_assert (sequence < sequences_alloced);
  g_assert (sequences[sequence].valid);
  LOCK ();
  sequences[sequence].at = (loop_at + samples_offset) % loop_length;
  UNLOCK ();
}

gdouble mix_get_sampling_rate (void)
{
  return sampling_rate;
}
static void
mix_limit_audio (gdouble *inout, guint n)
{
  guint i;
  for (i = 0; i < n; i++)
    inout[i] = atan (inout[i]) * BB_2_OVER_PI;
}

static void
mix_render_audio_b16  (gint16       *out,
                       guint         n_out)
{
  gdouble *tmp = g_new (double, n_out);
  guint i;
  mix_render_audio (tmp, n_out);
  mix_limit_audio (tmp, n_out);
  for (i = 0; i < n_out; i++)
    out[i] = tmp[i] * 32767.0;
  g_free (tmp);
}

static void
mix_render_audio_b16_stereo  (gint16       *out,
                              guint         n_out)
{
  gdouble *tmp = g_new (double, n_out);
  guint i;
  mix_render_audio (tmp, n_out);
  mix_limit_audio (tmp, n_out);
  for (i = 0; i < n_out; i++)
    out[2*i+0] = out[2*i+1] = tmp[i] * 32767.0;
  g_free (tmp);
}

static void
mix_render_audio_float  (gfloat       *out,
                         guint         n_out,
                         gboolean      limit)
{
  gdouble *tmp = g_new (gdouble, n_out);
  guint i;
  mix_render_audio (tmp, n_out);
  if (limit)
    mix_limit_audio (tmp, n_out);
  for (i = 0; i < n_out; i++)
    out[i] = tmp[i];
  g_free (tmp);
}
static void
mix_render_audio_float_stereo  (gfloat       *out,
                                guint         n_out,
                                gboolean      limit)
{
  gdouble *tmp = g_new (gdouble, n_out);
  guint i;
  mix_render_audio (tmp, n_out);
  if (limit)
    mix_limit_audio (tmp, n_out);
  for (i = 0; i < n_out; i++)
    out[2*i+0] = out[2*i+1] = tmp[i];
  g_free (tmp);
}

/* --- Backend Implementations --- */

/* OSS Style Interface */
#ifdef OSS_AUDIO
#include <linux/soundcard.h>
static int oss_fd;
static int copy_fd = -1;
static guint oss_block_size;
static gint16 *oss_block_data;
static guint oss_n_channels;
static gpointer oss_thread_func (gpointer data)
{
  for (;;)
    {
      if (oss_n_channels == 1)
        mix_render_audio_b16 (oss_block_data, oss_block_size);
      else if (oss_n_channels == 2)
        mix_render_audio_b16_stereo (oss_block_data, oss_block_size);
      else
        g_error ("n_channels bad");
      gsk_writen (oss_fd, oss_block_data,
                  sizeof (gint16) * oss_block_size * oss_n_channels);
      if (copy_fd >= 0)
        gsk_writen (copy_fd, oss_block_data,
                    sizeof (gint16) * oss_block_size * oss_n_channels);
    }
  return NULL;
}

static void mix_backend_init (int          *argc_inout,
			      const char ***argv_inout)
{
  const char *dev_name = NULL;
  gint i;
  guint log_frag_size = 8;
  guint num_fragments = 3;
  guint bits_per_sample = 16;
  guint num_channels = 2;
  gboolean stereo = TRUE;
  guint rate = 44100;
  int t;
  long r;
  for (i = 1; i < *argc_inout; )
    {
      const char *arg = (*argv_inout)[i];
      guint swallow = 0;
      if (g_str_has_prefix (arg, "--device="))
        {
          dev_name = strchr (arg, '=') + 1;
          swallow = 1;
        }
      if (g_str_has_prefix (arg, "--copy="))
        {
          const char *fname = strchr (arg, '=') + 1;
          copy_fd = open (fname, O_CREAT|O_TRUNC|O_WRONLY, 0644);
          if (copy_fd < 0)
            g_error ("could not create %s: %s", fname, g_strerror (errno));
          swallow = 1;
        }
      if (swallow)
        {
          memmove (*argv_inout + i,
                   *argv_inout + i + swallow,
                   (*argc_inout + 1 - i - swallow) * sizeof(const char*));
          *argc_inout -= swallow;
        }
      else
        i++;
    }
  if (dev_name == NULL)
    g_error ("missing --device=DEV for OSS audio devices");

  t = (log_frag_size & 0xffff)
    | ((num_fragments & 0xffff) << 16);
  oss_fd = open (dev_name, O_WRONLY);
  if (ioctl (oss_fd, SNDCTL_DSP_SETFRAGMENT, &t) < 0)
    {
      g_warning ("error going to correct amount of frag (%s)",
		 strerror (errno));
    }
  else
    {
      int total_bytes;
      int sample_size;
      sample_size = bits_per_sample / 8 * num_channels;
      total_bytes = num_fragments << log_frag_size;
      g_message ("Set soundcard fragments:  "
		 "%d frags of %d bytes; %3.3fs",
		 num_fragments,
		 (1 << log_frag_size),
		 (float) total_bytes / rate / sample_size);
    }
  t = bits_per_sample;
  if (ioctl (oss_fd, SNDCTL_DSP_SAMPLESIZE, &t) < 0)
    {
      g_error ("error going to %d bits per sample", t);
    }

  t = stereo;
  if (ioctl (oss_fd, SNDCTL_DSP_STEREO, &t) < 0)
    {
      g_error ("error setting device to stereo: %s", strerror (errno));
    }
  r = rate;
  if (ioctl (oss_fd, SNDCTL_DSP_SPEED, &r) < 0)
    {
      g_error ("error setting dev to %d samples per sec", t);
    }
  if (ioctl (oss_fd, SNDCTL_DSP_GETBLKSIZE, &r) < 0)
    {
      g_error ("error getting dev block size");
    }
  g_message ("DSP Block size is %lu", r);
/*sound_device_block_size = r;*/

  if (ioctl (oss_fd, SOUND_PCM_READ_RATE, &t) < 0)
    {
      g_warning ("error querying sapmling rate from device");
      g_warning ("you may need to manually specify the "
		 "soundcard rate if it does not support the defaults");
    }
  rate = t;
  if (ioctl (oss_fd, SOUND_PCM_READ_BITS, &t) < 0)
    g_warning ("error querying bits/channel from device");
  g_assert (t == (int)bits_per_sample);
  if (ioctl (oss_fd, SOUND_PCM_READ_CHANNELS, &t) < 0)
    g_warning ("error reading num-channels from device");
  g_message ("n_channels=%d",t);
  g_assert (t == 1 || t == 2);;
  oss_n_channels = t;

  sampling_rate = rate;
  oss_block_size = (1 << log_frag_size) / (bits_per_sample * oss_n_channels / 8);
  oss_block_data = g_new (gint16, oss_block_size * oss_n_channels);

  if (g_thread_create (oss_thread_func, NULL, FALSE, NULL) == NULL)
    g_error ("error creating audio-thread");
}
#endif

/* PortAudio Interface */
#ifdef PORTAUDIO_AUDIO
#include <portaudio.h>

static int 
audio_io_proc (void *input_buffer,
               void *output_buffer,
               unsigned long framesPerBuffer,
               PaTimestamp outTime,
               void *userData)
{
  mix_render_audio_float_stereo (output_buffer, framesPerBuffer, FALSE);
  return 0;             /* 0 == not finished */
}

static void mix_backend_init (int          *argc_inout,
			      const char ***argv_inout)
{
  const char *dev_name = NULL;
  gint i;
  //gboolean stereo = FALSE;  (stereo not supported anyways)
  guint rate = 44100;
  int err;
  PortAudioStream *stream;
  for (i = 1; i < *argc_inout; )
    {
      const char *arg = (*argv_inout)[i];
      guint swallow = 0;

      /* TODO: process arguments? */
      (void) dev_name;

      if (swallow)
        {
          memmove (*argv_inout + i,
                   *argv_inout + i + swallow,
                   (*argc_inout + 1 - i - swallow) * sizeof(const char*));
          *argc_inout -= swallow;
        }
      else
        i++;
    }

  err = Pa_OpenDefaultStream(
      &stream,        /* passes back stream pointer */
      0,              /* no input channels */
      2,              /* mono output */
      paFloat32,      /* 32 bit floating point output */
      rate,          /* sample rate */
      256,            /* frames per buffer */
      0,              /* number of buffers, if zero then use default minimum */
      audio_io_proc,  /* specify our custom callback */
      NULL );        /* no user-data needed */
  
  if (err != paNoError)
    g_error ("PortAudio OpenDefaultStream: %s (couldn't open sound device)",
             Pa_GetErrorText (err));


  /* And start it running */
  err = Pa_StartStream( stream );
  if (err != paNoError)
    g_error ("PortAudio OpenDefaultStream: %s (couldn't start sound)",
             Pa_GetErrorText (err) );

  sampling_rate = rate;
}
#endif
