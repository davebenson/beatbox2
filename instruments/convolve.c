#include "instrument-includes.h"
#include "../core/bb-fft.h"
#include "../core/bb-vm.h"
#include <gsk/gskghelpers.h>


typedef struct _ConvolveInfoSub ConvolveInfoSub;
typedef struct _ConvolveInfo ConvolveInfo;

struct _ConvolveInfoSub
{
  BbInstrument *instrument;
  guint *instrument_params;
  BbProgram *duration_program;
};
struct _ConvolveInfo
{
  guint n_subs;
  ConvolveInfoSub *subs;
};

static gdouble *
synth_one (ConvolveInfoSub       *sub,
           BbRenderInfo          *render_info,
           GValue                *params,
           guint                 *n_samples_out)
{
  GValue *remapped = g_new (GValue, sub->instrument->n_param_specs);
  BbRenderInfo sub_render_info = *render_info;
  guint i;
  gdouble *rv;
  for (i = 0; i < sub->instrument->n_param_specs; i++)
    remapped[i] = params[sub->instrument_params[i]];
  if (sub->duration_program)
    {
      GValue v = BB_VALUE_INIT;
      bb_vm_run_get_value_or_die (sub->duration_program, &sub_render_info,
                                  sub->instrument->n_param_specs, remapped,
                                  &v);
      g_assert (G_VALUE_TYPE (&v) == BB_TYPE_DURATION);
      if (sub->instrument->duration_param >= 0)
        {
          BbDuration d;
          bb_value_get_duration (&v, &d);
          if (d.units == BB_DURATION_UNITS_NOTE_LENGTHS)
            {
              gdouble beats = render_info->note_duration_beats * d.value;
              bb_value_set_duration (remapped + sub->instrument->duration_param, BB_DURATION_UNITS_BEATS, beats);
            }
          else
            remapped[sub->instrument->duration_param] = v;
        }

      /* adjust sub_render_info */
      sub_render_info.note_duration_secs = bb_value_get_duration_as_seconds (&v, render_info);
      sub_render_info.note_duration_beats = bb_value_get_duration_as_beats (&v, render_info);
      sub_render_info.note_duration_samples = bb_value_get_duration_as_samples (&v, render_info);
    }
  rv = bb_instrument_synth (sub->instrument, &sub_render_info, remapped, n_samples_out);
  g_free (remapped);
  return rv;
}

static double *
convolve_synth    (BbInstrument *instrument,
                   BbRenderInfo *render_info,
                   GValue       *params,
                   guint        *n_samples_out)
{
  ConvolveInfo *ci = instrument->data;
  guint n_cur;
  gdouble *cur = synth_one (ci->subs + 0, render_info, params, &n_cur);
  guint i;
  for (i = 1; i < ci->n_subs; i++)
    {
      guint n_tmp;
      gdouble *tmp = synth_one (ci->subs + i, render_info, params, &n_tmp);
      gdouble *next_cur = g_new (gdouble, n_cur + n_tmp);
      bb_convolve (n_cur, cur, n_tmp, tmp, next_cur);
      g_free (cur);
      g_free (tmp);
      cur = next_cur;
      n_cur += n_tmp;
    }
  *n_samples_out = n_cur;
  return cur;
}

static void
free_convolve_info (gpointer data)
{
  ConvolveInfo *ci = data;
  guint i;
  for (i = 0; i < ci->n_subs; i++)
    {
      if (ci->subs[i].duration_program)
        bb_program_unref (ci->subs[i].duration_program);
      g_object_unref (ci->subs[i].instrument);
      g_free (ci->subs[i].instrument_params);
    }
  g_free (ci->subs);
  g_free (ci);
}

static BbInstrument *
construct_convolve(guint            n_args,
                   char           **args,
                   GError         **error)
{
  BbInstrument *rv;
  GPtrArray *pspecs = g_ptr_array_new ();
  ConvolveInfo *ci;
  guint i;
  if (n_args < 2)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                   "convolve takes at least 2 args");
      return NULL;
    }
  ci = g_new (ConvolveInfo, 1);
  ci->n_subs = n_args;
  ci->subs = g_new (ConvolveInfoSub, n_args);
  for (i = 0; i < n_args; i++)
    {
      const char *dotdot = strstr (args[i], "..");
      if (dotdot == NULL)
        {
          ci->subs[i].duration_program = NULL;
          ci->subs[i].instrument = bb_instrument_from_string (args[i]);
        }
      else
        {
          char *time_str;
          time_str = g_strndup (args[i], dotdot - args[i]);
          ci->subs[i].duration_program = bb_program_parse (time_str, error);
          if (ci->subs[i].duration_program == NULL)
            {
              gsk_g_error_add_prefix (error, "error parsing duration program (index %u) for convolve", i);
              return NULL;
            }
          ci->subs[i].instrument = bb_instrument_parse_string (dotdot + 2, error);
          g_free (time_str);
        }
      if (ci->subs[i].instrument == NULL)
        {
          gsk_g_error_add_prefix (error, "error parsing instrument %u for convolve", i);
          return NULL;
        }
      ci->subs[i].instrument_params = g_new (guint, ci->subs[i].instrument->n_param_specs);
      if (!bb_instrument_map_params (ci->subs[i].instrument,
                                     ci->subs[i].instrument_params,
                                     pspecs,
                                     error))
        {
          gsk_g_error_add_prefix (error, "error mapping params for instrument %u for convolve", i);
          return NULL;
        }
    }
  rv = bb_instrument_new_generic_v (NULL, pspecs->len, (GParamSpec **) pspecs->pdata);
  g_ptr_array_free (pspecs, TRUE);
  rv->data = ci;
  rv->free_data = free_convolve_info;
  rv->synth_func = convolve_synth;
  return rv;
}

BB_INIT_DEFINE(_bb_convolve_init)
{
  bb_instrument_template_constructor_register ("convolve", construct_convolve);
}
