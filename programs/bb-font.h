#ifndef __BB_FONT_H_
#define __BB_FONT_H_

#include <glib.h>



unsigned bb_font_get_height     (void);
unsigned bb_font_get_text_width (const char   *text);
unsigned bb_font_get_ascent     (void);
unsigned bb_font_get_descent    (void);
void     bb_font_draw_text      (guint8       *img_data,
                                 unsigned      rowstride,
                                 const guint8 *rgb,
                                 const char   *text);

#endif
