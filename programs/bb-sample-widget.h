#ifndef __BB_SAMPLE_WIDGET_H_
#define __BB_SAMPLE_WIDGET_H_

#include "../core/bb-fft.h"
#include <gtk/gtkdrawingarea.h>

typedef struct _BbSampleWidgetClass BbSampleWidgetClass;
typedef struct _BbSampleWidget BbSampleWidget;

GType bb_sample_widget_get_type(void) G_GNUC_CONST;
#define BB_TYPE_SAMPLE_WIDGET              (bb_sample_widget_get_type ())
#define BB_SAMPLE_WIDGET(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), BB_TYPE_SAMPLE_WIDGET, BbSampleWidget))
#define BB_SAMPLE_WIDGET_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), BB_TYPE_SAMPLE_WIDGET, BbSampleWidgetClass))
#define BB_SAMPLE_WIDGET_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), BB_TYPE_SAMPLE_WIDGET, BbSampleWidgetClass))
#define BB_IS_SAMPLE_WIDGET(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BB_TYPE_SAMPLE_WIDGET))
#define BB_IS_SAMPLE_WIDGET_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), BB_TYPE_SAMPLE_WIDGET))

#define BB_TYPE_SAMPLE_WIDGET_MODE         (bb_sample_widget_mode_get_type())
typedef enum
{
  BB_SAMPLE_WIDGET_MODE_AMPLITUDE,
  BB_SAMPLE_WIDGET_MODE_FREQUENCY
} BbSampleWidgetMode;

struct _BbSampleWidgetClass
{
  GtkDrawingAreaClass base_class;
};

struct _BbSampleWidget
{
  GtkDrawingArea base_instance;

  guint last_width, last_height;
  GdkDrawable *last_drawing;

  GdkGC *time_line_gc;

  gboolean has_time;
  gdouble time;

  BbSampleWidgetMode mode;

  guint n_samples;
  const gdouble *samples;
  gdouble rate;

  guint fft_window_size_log2;
  guint window_size;
  BbFftWindowType window_type;
  gdouble window_param;

  gdouble log_min_freq;
  gdouble log_max_freq;
  gdouble delta_log_freq;
};

GtkWidget *bb_sample_widget_new (guint          n_samples,
                                 const gdouble *samples,
                                 gdouble        sampling_rate);

#endif
