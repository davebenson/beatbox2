#ifndef __BB_FFT_H_
#define __BB_FFT_H_

#include "bb-types.h"


/*
 * bb_fft
 *
 * Perform an in-place FFT on a complex input buffer.
 *
 * the input buffer should be real(f[0]) , imag(f[0]), ... ,real(f[nfft-1]) , imag(f[nfft-1])
 * the the output will be     real(F[0]) , imag(F[0]), ... ,real(F[nfft-1]) , imag(F[nfft-1])
 *
 */
void bb_fft(guint n_fft, gboolean inverse, BbComplex *f);

/* --- convolution --- */
void bb_convolve (guint a_len, const gdouble *a_data,
                  guint b_len, const gdouble *b_data,
                  gdouble *out);

/* --- windowing --- */
typedef enum
{
  BB_FFT_WINDOW_RECTANGULAR,
  BB_FFT_WINDOW_GAUSS,
  BB_FFT_WINDOW_HAMMING,
  BB_FFT_WINDOW_HANN,
  BB_FFT_WINDOW_TRIANGULAR,
  BB_FFT_WINDOW_BARTLETT,
  BB_FFT_WINDOW_BARTLETT_HANN,
  BB_FFT_WINDOW_BLACKMAN,
  BB_FFT_WINDOW_KAISER,
  BB_FFT_WINDOW_NUTTALL,
  BB_FFT_WINDOW_BLACKMAN_HARRIS,
  BB_FFT_WINDOW_BLACKMAN_NUTTALL,
  BB_FFT_WINDOW_FLAT_TOP,
} BbFftWindowType;

GType bb_fft_window_type_get_type (void);
#define BB_TYPE_FFT_WINDOW_TYPE (bb_fft_window_type_get_type())

/*  The param is only used for:
       gauss:  param represents sigma (default = 0.4)
       kaiser: param represents alpha (default = 2)
 */
void
bb_fft_make_window (BbFftWindowType type,
                    gdouble         param,
                    guint           N,
                    double         *out);

gboolean bb_fft_window_type_has_default  (BbFftWindowType type,
                                          gdouble        *param_out);

#endif
