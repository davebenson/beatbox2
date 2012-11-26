
#include <glib.h>
#include "bb-fft.h"

void bb_render_amplitude_window (guint8      *out,
                                 guint        width,
                                 guint        n_samples,
                                 gdouble     *samples,
                                 guint        window_size);

void bb_render_fft_window  (guint          window_size,
                            BbComplex     *freq_domain_data,
                            gdouble        sampling_rate,
                            gdouble        log_min_freq,
                            gdouble        delta_log_freq,
                            guint          width,
                            guint8        *out);
