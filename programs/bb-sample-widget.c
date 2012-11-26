#include <math.h>
#include <string.h>
#include "bb-sample-widget.h"
#include "../core/bb-render.h"

#define DEFAULT_MODE                    BB_SAMPLE_WIDGET_MODE_AMPLITUDE
#define DEFAULT_FFT_WINDOW_SIZE_LOG2    13              /* 8192 */
#define DEFAULT_FFT_WINDOW_TYPE         BB_FFT_WINDOW_HAMMING

G_DEFINE_TYPE(BbSampleWidget, bb_sample_widget, GTK_TYPE_DRAWING_AREA);


static GEnumValue bb_sample_widget_mode_enum_values[] =
{
  { BB_SAMPLE_WIDGET_MODE_AMPLITUDE,
    "BB_SAMPLE_WIDGET_MODE_AMPLITUDE",
    "amplitude" },
  { BB_SAMPLE_WIDGET_MODE_FREQUENCY,
    "BB_SAMPLE_WIDGET_MODE_FREQUENCY",
    "frequency" },
  { 0, NULL, NULL }
};

GType 
bb_sample_widget_mode_get_type (void)
{
  static GType rv = 0;
  if (rv == 0)
    rv = g_enum_register_static ("BbSampleWidgetMode", bb_sample_widget_mode_enum_values);
  return rv;
}

enum
{
  PROP_0,
  PROP_HAS_TIME,
  PROP_TIME,
  PROP_MODE,
  PROP_FFT_WINDOW_SIZE_LOG2,
  PROP_FFT_WINDOW_TYPE
};

static gboolean
bb_sample_widget_configure_event (GtkWidget      *widget,
                                  GdkEventConfigure *configure)
{
  gtk_widget_queue_draw (widget);
  return TRUE;
}

static void
black_verticle_pixel_column  (guint n_black,
                              guint out_width,
                              guint8 *out)
{
  guint i;
  guint skip = (out_width - 1) * 3;
  for (i = 0; i < n_black; i++)
    {
      *out++ = 0;
      *out++ = 0;
      *out++ = 0;
      out += skip;
    }
}

static void
render_verticle_pixel_column (guint n_copy,
                              const guint8 *in,
                              guint out_width,
                              guint8 *out)
{
  guint i;
  guint skip = (out_width - 1) * 3;
  for (i = 0; i < n_copy; i++)
    {
      *out++ = *in++;
      *out++ = *in++;
      *out++ = *in++;
      out += skip;
    }
}

static void
render_amplitude_image (BbSampleWidget *widget,
                        guint           width,
                        guint           height,
                        guint8         *out)
{
  guint8 *strip = g_malloc (height * 3);
  guint copy_len = widget->n_samples / width + 1;
  gdouble *copy = g_new (double, copy_len);
  guint last = 0;
  guint i;
  for (i = 0; i < width; i++)
    {
      guint next = (guint64) (i+1) * widget->n_samples / width;
      if (next == last)
        {
          if (last + 1 == widget->n_samples)
            {
              black_verticle_pixel_column (height, width, out + 3 * i);
              continue;
            }
          last--;
        }
      g_assert (next - last <= copy_len);
      memcpy (copy, widget->samples + last, (next - last) * sizeof (double));
      bb_render_amplitude_window (strip, height, next - last, copy, next - last);
      render_verticle_pixel_column (height, strip, width, out + 3 * i);
      last = next;
    }
  g_free (strip);
  g_free (copy);
}

static void
render_fft_image (BbSampleWidget *widget,
                  guint           width,
                  guint           height,
                  guint8         *out)
{
  guint window_size = widget->window_size;
  BbComplex *fft_input = g_new (BbComplex, window_size);
  gdouble *window = g_new (gdouble, window_size);
  guint8 *strip = g_malloc (height * 3);
  guint i;

  bb_fft_make_window (widget->window_type,
                      widget->window_param,
                      window_size, window);

  for (i = 0; i < width; i++)
    {
      guint mid_sample = (2*i+1) * width / (widget->n_samples * 2);
      gint in_at = mid_sample - window_size / 2;
      guint out_at = 0;
      while (in_at < 0 && out_at < window_size)
        {
          fft_input[out_at].re = 0;
          fft_input[out_at].im = 0;
          out_at++;
          in_at++;
        }
      while ((guint) in_at < widget->n_samples && out_at < window_size)
        {
          fft_input[out_at].re = window[out_at] * widget->samples[in_at];
          fft_input[out_at].im = 0;
          out_at++;
          in_at++;
        }
      while (out_at < window_size)
        {
          fft_input[out_at].re = 0;
          fft_input[out_at].im = 0;
          out_at++;
        }
      bb_fft (window_size, FALSE, fft_input);
      bb_render_fft_window (window_size, fft_input, widget->rate,
                            widget->log_min_freq, widget->delta_log_freq,
                            height, strip);
      render_verticle_pixel_column (height, strip, width, out + 3 * i);
    }
  g_free (fft_input);
  g_free (window);
  g_free (strip);
}

static inline gint
time_pixel (BbSampleWidget *swidget)
{
  gint wid = swidget->base_instance.widget.allocation.width;
  if (wid == 0)
    return 0;
  return (gint64) swidget->time * wid / swidget->n_samples;
}

static gboolean
bb_sample_widget_expose_event (GtkWidget      *widget,
                               GdkEventExpose *expose)
{
  BbSampleWidget *swidget = BB_SAMPLE_WIDGET (widget);
  if (swidget->last_drawing == NULL
   || widget->allocation.width != (gint) swidget->last_width
   || widget->allocation.height != (gint) swidget->last_height)
    {
      guint8 *rgb_data;
      guint width = widget->allocation.width;
      guint height = widget->allocation.height;
      guint depth = gdk_drawable_get_depth (widget->window);
      if (swidget->last_drawing)
        g_object_unref (swidget->last_drawing);
      rgb_data = g_malloc (3 * width * height);
      switch (swidget->mode)
        {
        case BB_SAMPLE_WIDGET_MODE_AMPLITUDE:
          render_amplitude_image (swidget, width, height, rgb_data);
          break;
        case BB_SAMPLE_WIDGET_MODE_FREQUENCY:
          render_fft_image (swidget, width, height, rgb_data);
          break;
        default:
          g_assert_not_reached ();
        }
      swidget->last_drawing = gdk_pixmap_new (widget->window,
                                              width, height, depth);
      gdk_draw_rgb_image (swidget->last_drawing, widget->style->fg_gc[0],
                          0, 0, width, height, GDK_RGB_DITHER_NORMAL,
                          rgb_data, width * 3);
      g_free (rgb_data);
      swidget->last_width = width;
      swidget->last_height = height;
    }

  gdk_draw_drawable (widget->window, widget->style->fg_gc[0],
                     swidget->last_drawing,
                     expose->area.x, expose->area.y,
                     expose->area.x, expose->area.y,
                     expose->area.width, expose->area.height);
  if (swidget->has_time)
    {
      gint x = time_pixel (swidget);
      if (expose->area.x <= (int) x
       && (int)x < expose->area.x + expose->area.width)
        {
          if (swidget->time_line_gc == NULL)
            {
              GdkColor col;
              swidget->time_line_gc = gdk_gc_new (widget->window);
              col.red = 65535;
              col.green = 65535;
              col.blue = 0;
              gdk_gc_set_rgb_fg_color (swidget->time_line_gc, &col);
            }
          gdk_draw_line (widget->window, swidget->time_line_gc,
                         x, expose->area.y,
                         x, expose->area.y + expose->area.height);
        }
    }
  return TRUE;
}

static inline void
invalidate (BbSampleWidget *swidget)
{
  if (swidget->last_drawing)
    {
      g_object_unref (swidget->last_drawing);
      swidget->last_drawing = NULL;
    }
  gtk_widget_queue_draw (GTK_WIDGET (swidget));
}

static void
bb_sample_widget_set_property  (GObject        *object,
                                guint           property_id,
                                const GValue   *value,
                                GParamSpec     *pspec)
{
  BbSampleWidget *swidget = BB_SAMPLE_WIDGET (object);
  GtkWidget *widget = GTK_WIDGET (object);
  switch (property_id)
    {
    case PROP_HAS_TIME:
      {
        gboolean new_value = g_value_get_boolean (value);
        if ((swidget->has_time && !new_value)
         || (!swidget->has_time && new_value))
          {
            guint pix = time_pixel (swidget);
            gtk_widget_queue_draw_area (widget, pix, 0, 1, widget->allocation.height);
          }
        swidget->has_time = new_value;
      }
      break;
    case PROP_TIME:
      {
        gint old_pixel, new_pixel;

        old_pixel = time_pixel (swidget);
        swidget->time = g_value_get_uint (value);
        new_pixel = time_pixel (swidget);

        if (swidget->has_time && old_pixel != new_pixel)
          {
            gtk_widget_queue_draw_area (widget, old_pixel, 0, 1, widget->allocation.height);
            gtk_widget_queue_draw_area (widget, new_pixel, 0, 1, widget->allocation.height);
          }
      }
      break;
    case PROP_MODE:
      {
        BbSampleWidgetMode mode = g_value_get_enum (value);
        if (mode != swidget->mode)
          {
            swidget->mode = mode;
            invalidate (swidget);
          }
        break;
      }
    case PROP_FFT_WINDOW_SIZE_LOG2:
      {
        guint v = g_value_get_uint (value);
        if (v != swidget->fft_window_size_log2)
          {
            swidget->fft_window_size_log2 = v;
            swidget->window_size = 1 << v;
            if (swidget->mode == BB_SAMPLE_WIDGET_MODE_FREQUENCY)
              invalidate (swidget);
          }
        break;
      }
    case PROP_FFT_WINDOW_TYPE:
      {
        BbFftWindowType v = g_value_get_enum (value);
        if (v != swidget->window_type)
          {
            swidget->window_type = v;
            if (swidget->mode == BB_SAMPLE_WIDGET_MODE_FREQUENCY)
              invalidate (swidget);
          }
        break;
      }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
bb_sample_widget_get_property  (GObject        *object,
                                guint           property_id,
                                GValue         *value,
                                GParamSpec     *pspec)
{
  BbSampleWidget *swidget = BB_SAMPLE_WIDGET (object);
  switch (property_id)
    {
    case PROP_HAS_TIME:
      g_value_set_boolean (value, swidget->has_time);
      break;
    case PROP_TIME:
      g_value_set_uint (value, swidget->time);
      break;
    case PROP_MODE:
      g_value_set_enum (value, swidget->mode);
      break;
    case PROP_FFT_WINDOW_SIZE_LOG2:
      g_value_set_uint (value, swidget->fft_window_size_log2);
      break;
    case PROP_FFT_WINDOW_TYPE:
      g_value_set_enum (value, swidget->window_type);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
bb_sample_widget_finalize (GObject *object)
{
  BbSampleWidget *swidget = BB_SAMPLE_WIDGET (object);
  if (swidget->last_drawing)
    g_object_unref (swidget->last_drawing);
  if (swidget->time_line_gc)
    g_object_unref (swidget->time_line_gc);
  G_OBJECT_CLASS (bb_sample_widget_parent_class)->finalize (object);
}

static void
bb_sample_widget_class_init (BbSampleWidgetClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  widget_class->expose_event = bb_sample_widget_expose_event;
  widget_class->configure_event = bb_sample_widget_configure_event;
  object_class->set_property = bb_sample_widget_set_property;
  object_class->get_property = bb_sample_widget_get_property;
  object_class->finalize = bb_sample_widget_finalize;

  g_object_class_install_property (
    object_class,
    PROP_HAS_TIME,
    g_param_spec_boolean ("has-time", "Has Time", NULL,
                          FALSE, G_PARAM_READWRITE)
  );
  g_object_class_install_property (
    object_class,
    PROP_TIME,
    g_param_spec_uint ("time", "Time (in samples)", NULL,
                        0, G_MAXUINT, 0, G_PARAM_READWRITE)
  );
  g_object_class_install_property (
    object_class,
    PROP_MODE,
    g_param_spec_enum ("mode", "Render Mode", NULL,
                       BB_TYPE_SAMPLE_WIDGET_MODE,
                       DEFAULT_MODE, G_PARAM_READWRITE)
  );
  g_object_class_install_property (
    object_class,
    PROP_FFT_WINDOW_SIZE_LOG2,
    g_param_spec_uint ("fft-window-size-log2", "FFT Window Size Log2", NULL,
                       0, 32, DEFAULT_FFT_WINDOW_SIZE_LOG2, G_PARAM_READWRITE)
  );
  g_object_class_install_property (
    object_class,
    PROP_FFT_WINDOW_TYPE,
    g_param_spec_enum ("fft-window-type", "FFT Window Type", NULL,
                       BB_TYPE_FFT_WINDOW_TYPE, DEFAULT_FFT_WINDOW_TYPE,
                       G_PARAM_READWRITE)
  );
}

static void
bb_sample_widget_init (BbSampleWidget *widget)
{
  widget->mode = DEFAULT_MODE;
  widget->fft_window_size_log2 = DEFAULT_FFT_WINDOW_SIZE_LOG2;
  widget->window_size = 1 << DEFAULT_FFT_WINDOW_SIZE_LOG2;
  widget->window_type = DEFAULT_FFT_WINDOW_TYPE;

  widget->log_min_freq = log (440.0 / 8);
  widget->log_max_freq = log (440.0 * 8);
  widget->delta_log_freq = widget->log_max_freq - widget->log_min_freq;
}

GtkWidget *
bb_sample_widget_new (guint n_samples,
                      const gdouble *samples,
                      gdouble rate)
{
  BbSampleWidget *swidget = g_object_new (BB_TYPE_SAMPLE_WIDGET, NULL);
  swidget->n_samples = n_samples;
  swidget->samples = samples;
  swidget->rate = rate;
  return GTK_WIDGET (swidget);
}
