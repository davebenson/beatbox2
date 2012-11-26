/*
Copyright (c) 2003, Mark Borgerding

All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
    * Neither the author nor the names of any contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/* this code has been heavily modified from the kiss-fft version:
   practically totally different... */

#define _GNU_SOURCE                     /* for j0() */
#include <stdlib.h>
#include <math.h>
#include "bb-fft.h"
#include "bb-utils.h"

typedef struct
{
  BbComplex* twiddle;
  guint n_swap;
  guint * swap_indices;
} BbFftState;

/* first index is log2(N); second is inverse?1:0. */
static BbFftState *states[33][2];

static inline guint32
bit_reverse_32 (guint32 i)
{
#define SWAP(mask, shift) \
  i = (((i&mask)<<shift) | ((i>>shift)&mask))
  SWAP (0x0000ffff, 16);
  SWAP (0x00ff00ff, 8);
  SWAP (0x0f0f0f0f, 4);
  SWAP (0x33333333, 2);
  SWAP (0x55555555, 1);
#undef SWAP
  return i;
}

static inline guint
bit_reverse (guint32 i, guint N)
{
  return bit_reverse_32 (i) >> (32-N);
}

static BbFftState *
get_fft_state (guint log2_N, gboolean inverse)
{
  guint nfft = 1 << log2_N;
  guint i;
  guint *at;
  if (states[log2_N][inverse] == NULL)
    {
      BbFftState *fft_state = g_new (BbFftState, 1);
      fft_state->twiddle = g_new (BbComplex, nfft / 2.0);
      for (i = 0; i < nfft / 2; i++)
	{
	  fft_state->twiddle[i].re = cos (BB_2PI * i / nfft);
	  fft_state->twiddle[i].im = -sin (BB_2PI * i / nfft);
	  if (inverse)
	    fft_state->twiddle[i].im *= -1;
	}
      guint n_symmetric = 1 << ((log2_N + 1) / 2);
      guint n_nonsym = nfft - n_symmetric;
      fft_state->n_swap = n_nonsym / 2;
      if (states[log2_N][1-inverse] != NULL)
	fft_state->swap_indices = states[log2_N][1-inverse]->swap_indices;
      else
        {
	  fft_state->swap_indices = g_new (guint, fft_state->n_swap * 2);
	  at = fft_state->swap_indices;
	  for (i = 0; i < nfft; i++)
	    {
	      guint rev = bit_reverse (i, log2_N);
	      if (rev > i)
		{
		  *at++ = i;
		  *at++ = rev;
		}
	    }
	  g_assert (fft_state->swap_indices + fft_state->n_swap * 2 == at);
	}
      states[log2_N][inverse] = fft_state;
    }

  return states[log2_N][inverse];
}


// the heart of the fft
static void fft_work (guint            N,
                      BbComplex       *f,
                      const BbComplex *twid,
                      guint            twid_step)
{
  guint n;
  BbComplex csum, cdiff;
  N >>= 1;
  for (n = 0; n < N; ++n)
    {
      BB_COMPLEX_ADD (csum, f[n], f[N + n]);
      BB_COMPLEX_SUB (cdiff, f[n], f[N + n]);
      f[n] = csum;
      BB_COMPLEX_MUL (f[N + n], cdiff, twid[n * twid_step]);
    }
  if (N == 1)
    return;
  fft_work (N, f, twid, twid_step * 2);
  fft_work (N, f + N, twid, twid_step * 2);
}

static guint
log2_exact (guint v)
{
  /* is a power of two */
  g_assert (((v)&((v)-1))==0);

  return ((v&0xffff) ? 0 : 16)
       + ((v&0x00ff00ff) ? 0 : 8)
       + ((v&0x0f0f0f0f) ? 0 : 4)
       + ((v&0x33333333) ? 0 : 2)
       + ((v&0x55555555) ? 0 : 1);
}

// call for each buffer 
void bb_fft(guint n_fft, gboolean inverse, BbComplex *f)
{
  BbFftState *state;
  guint i;
  const guint *in_at;
  guint log2_N = log2_exact (n_fft);
  inverse = inverse ? 1 : 0;
  state = get_fft_state (log2_N, inverse);
  fft_work (n_fft, f, state->twiddle, 1);

  /* do bit reversal */
  i = state->n_swap;
  in_at = state->swap_indices;
  while (i--)
    {
      guint a = *in_at++;
      guint b = *in_at++;
      BbComplex tmp = f[a];
      f[a] = f[b];
      f[b] = tmp;
    }
}

/* ---- Window Functions ---- */
static inline gdouble square (gdouble x)
{
  return x * x;
}
void
bb_fft_make_window (BbFftWindowType type,
                    gdouble         param,
                    guint           N,
                    double         *out)
{
  guint i;
  g_assert (N > 1);
  switch (type)
    {
    case BB_FFT_WINDOW_RECTANGULAR:
      for (i = 0; i < N; i++)
        out[i] = 1;
      break;
    case BB_FFT_WINDOW_GAUSS:
      {
        gdouble sigma = param;
        gdouble n12 = (N - 1) * 0.5;
        for (i = 0; i < N; i++)
          out[i] = exp (-0.5 * square ((i - n12) / (sigma * n12)));
        break;
      }
    case BB_FFT_WINDOW_HAMMING:
      {
        gdouble factor = BB_2PI / (N-1);
        for (i = 0; i < N; i++)
          out[i] = 0.53836 - 0.46164 * cos (factor * i);
        break;
      }
    case BB_FFT_WINDOW_HANN:
      {
        gdouble factor = BB_2PI / (N-1);
        for (i = 0; i < N; i++)
          out[i] = 0.5 - 0.5 * cos (factor * i);
        break;
      }
    case BB_FFT_WINDOW_TRIANGULAR:
      {
        gdouble factor = 2.0 / (N);
        for (i = 0; i < N; i++)
          out[i] = 1.0 - factor * fabs (i - 0.5 * (N-1));
        break;
      }
    case BB_FFT_WINDOW_BARTLETT:
      {
        gdouble factor = 2.0 / (N-1);
        for (i = 0; i < N; i++)
          out[i] = 1.0 - factor * fabs (i - 0.5 * (N-1));
        break;
      }
    case BB_FFT_WINDOW_BARTLETT_HANN:
      {
        for (i = 0; i < N; i++)
          out[i] = 0.62
                 - 0.48 * fabs ((double)i / (N-1) - 0.5)
                 - 0.38 * cos (BB_2PI * i / (N-1));
        break;
      }
    case BB_FFT_WINDOW_BLACKMAN:
      {
        for (i = 0; i < N; i++)
          out[i] = 0.42
                 - 0.5 * cos (BB_2PI * i / (N-1))
                 - 0.08 * cos (BB_4PI * i / (N-1));
        break;
      }
    case BB_FFT_WINDOW_KAISER:
      {
        gdouble pi_alpha = G_PI * param;
        gdouble i0_pi_alpha = j0 (pi_alpha);
        for (i = 0; i < N; i++)
          out[i] = j0 (pi_alpha * sqrt (1.0 - square (2.0*i/(N-1) - 1)))
                 / i0_pi_alpha;
        break;
      }
    case BB_FFT_WINDOW_NUTTALL:
      {
        for (i = 0; i < N; i++)
          out[i] = 0.355768
                 - 0.487396 * cos (BB_2PI * i / (N-1))
                 + 0.144232 * cos (BB_4PI * i / (N-1))
                 - 0.012604 * cos (BB_6PI * i / (N-1));
        break;
      }
    case BB_FFT_WINDOW_BLACKMAN_HARRIS:
      {
        for (i = 0; i < N; i++)
          out[i] = 0.35875
                 - 0.48829 * cos (BB_2PI * i / (N-1))
                 + 0.14128 * cos (BB_4PI * i / (N-1))
                 - 0.01168 * cos (BB_6PI * i / (N-1));
        break;
      }
    case BB_FFT_WINDOW_BLACKMAN_NUTTALL:
      {
        for (i = 0; i < N; i++)
          out[i] = 0.3635819
                 - 0.4891775 * cos (BB_2PI * i / (N-1))
                 + 0.1365995 * cos (BB_4PI * i / (N-1))
                 - 0.0106411 * cos (BB_6PI * i / (N-1));
        break;
      }
    case BB_FFT_WINDOW_FLAT_TOP:
      {
        for (i = 0; i < N; i++)
          out[i] = 1.000
                 - 1.930 * cos (BB_2PI * i / (N-1))
                 + 1.290 * cos (BB_4PI * i / (N-1))
                 - 0.388 * cos (BB_6PI * i / (N-1))
                 + 0.032 * cos (BB_8PI * i / (N-1));
        break;
      }
    default:
      g_assert_not_reached ();
    }
}

gboolean bb_fft_window_type_has_default  (BbFftWindowType type,
                                          gdouble        *param_out)
{
  if (type == BB_FFT_WINDOW_GAUSS)
    {
      *param_out = 0.4;
      return TRUE;
    }
  else if (type == BB_FFT_WINDOW_KAISER)
    {
      *param_out = 2.0;
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

/* --- convolution --- */
void bb_convolve (guint a_len, const gdouble *a_data,
                  guint b_len, const gdouble *b_data,
                  gdouble *out)
{
  guint pot;
  guint i;
  BbComplex *ca, *cb;

  /* make 'a' the shorter sample */
  if (a_len > b_len)
    {
      guint ti;
      const gdouble *tp;
      ti = a_len; a_len = b_len; b_len = ti;
      tp = a_data; a_data = b_data; b_data = tp;
    }

  for (pot = 0; (1U<<pot) < a_len * 2; pot++)
    ;

  ca = g_new (BbComplex, 1<<pot);
  cb = g_new (BbComplex, 1<<pot);

  for (i = 0; i < a_len; i++)
    {
      ca[i].re = a_data[i];
      ca[i].im = 0;
    }
  for (     ; i < (1U<<pot); i++)
    {
      ca[i].re = 0;
      ca[i].im = 0;
    }
  bb_fft (1<<pot, FALSE, ca);

  guint last_set = 0;
  for (i = 0; i < b_len; i += (1U<<(pot-1)))
    {
      guint nnz = MIN (b_len - i, 1U<<(pot-1));
      guint j;
      for (j = 0; j < nnz; j++)
        {
          cb[j].re = b_data[j + i];
          cb[j].im = 0;
        }
      for (     ; j < (1U<<pot); j++)
        {
          cb[j].re = 0;
          cb[j].im = 0;
        }
      bb_fft (1<<pot, FALSE, cb);

      for (j = 0; j < (1U<<pot); j++)
        BB_COMPLEX_MUL (cb[j], ca[j],cb[j]);
      bb_fft (1<<pot, TRUE, cb);

      for (j = i; j < last_set; j++)
        out[j] += cb[j-i].re;
      for (j = last_set; j < i + nnz + a_len; j++)
        out[j] = cb[j-i].re;
      last_set = j;
    }
  g_assert (last_set == a_len + b_len);
  g_free (ca);
  g_free (cb);

  /* divide input and output by the window length */
  {
    GDoubleIEEE754 *hack = (GDoubleIEEE754 *) out;
    for (i = 0; i < a_len + b_len; i++)
      {
        if (hack[i].mpn.biased_exponent < pot)
          hack[i].mpn.biased_exponent = 0;
        else
          hack[i].mpn.biased_exponent -= pot;
      }
  }
}
