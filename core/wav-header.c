#include <glib.h>
#include "wav-header.h"

guint8 *get_wav_header (guint n_samples,
                        guint sampling_rate,
			guint *header_size_out)
{
  guint header_size = 8		/* "RIFF" plus size */
                    + 4		/* "WAVE" */
                    + 8		/* "fmt " plus size */
		    + 16	/* fmt header */
		    + 8;	/* "data" plus size */
  guint file_size = header_size + 2 * n_samples;
  guint8 *buf = g_malloc (header_size);
  buf[0]  = 'R';
  buf[1]  = 'I';
  buf[2]  = 'F';
  buf[3]  = 'F';
  buf[4]  = (file_size - 8)>>0;
  buf[5]  = (file_size - 8)>>8;
  buf[6]  = (file_size - 8)>>16;
  buf[7]  = (file_size - 8)>>24;
  buf[8]  = 'W';
  buf[9]  = 'A';
  buf[10] = 'V';
  buf[11] = 'E';
  buf[12]  = 'f';
  buf[13]  = 'm';
  buf[14] = 't';
  buf[15] = ' ';
  buf[16] = 16;		/* 12-15: size of WAVE header */
  buf[17] = 0;
  buf[18] = 0;
  buf[19] = 0;
  buf[20] = 1;		/* compression code */
  buf[21] = 0;
  buf[22] = 1;		/* n_channels */
  buf[23] = 0;
  buf[24] = sampling_rate >> 0;
  buf[25] = sampling_rate >> 8;
  buf[26] = sampling_rate >> 16;
  buf[27] = sampling_rate >> 24;
  buf[28] = (sampling_rate*2) >> 0;
  buf[29] = (sampling_rate*2) >> 8;
  buf[30] = (sampling_rate*2) >> 16;
  buf[31] = (sampling_rate*2) >> 24;
  buf[32] = 2;		/* block_align */
  buf[33] = 0;
  buf[34] = 16;		/* sig-bits */
  buf[35] = 0;
  buf[36] = 'd';
  buf[37] = 'a';
  buf[38] = 't';
  buf[39] = 'a';
  buf[40] = (n_samples*2) >> 0;
  buf[41] = (n_samples*2) >> 8;
  buf[42] = (n_samples*2) >> 16;
  buf[43] = (n_samples*2) >> 24;
  g_assert (44 == header_size);

  *header_size_out = header_size;
  return buf;
}
