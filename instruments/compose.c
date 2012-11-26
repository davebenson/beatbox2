#include "instrument-includes.h"
#include <gsk/gskghelpers.h>
#include "../core/bb-vm.h"

typedef struct _ComposeInput ComposeInput;
typedef struct _Compose Compose;

struct _ComposeInput
{
  BbProgram *start, *duration;		/* these return BbDurations */
  BbInstrument *instrument;
};

struct _Compose
{
  guint n_inputs;
  ComposeInput *inputs;
};

static double *
compose_synth (BbInstrument *instrument,
               BbRenderInfo *render_info,
               GValue       *params,
               guint        *n_samples_out)
{
  guint n = render_info->note_duration_samples;
  guint n_params = instrument->n_param_specs;
  guint in;
  Compose *compose = instrument->data;
  gdouble *rv = g_new0 (gdouble, n);
  BbRenderInfo sub_render_info = *render_info;
  *n_samples_out = n;
  for (in = 0; in < compose->n_inputs; in++)
    {
      /* evaluate start/duration programs */
      ComposeInput *input = compose->inputs + in;
      GValue substart_value = BB_VALUE_INIT;
      GValue subduration_value = BB_VALUE_INIT;
      GValue *subvalues = g_new (GValue, input->instrument->n_param_specs);
      double *subrv;
      guint n_subrv;
      gint end_sample;
      gint start_sample;
      guint ip;

      bb_vm_run_get_value_or_die (input->start, render_info, n_params, params, &substart_value);
      bb_vm_run_get_value_or_die (input->duration, render_info, n_params, params, &subduration_value);

      sub_render_info.note_duration_secs = bb_value_get_duration_as_seconds (&subduration_value, render_info);
      sub_render_info.note_duration_beats = bb_value_get_duration_as_beats (&subduration_value, render_info);
                  

      /* lookup values; treat duration specially */
      for (ip = 0; ip < input->instrument->n_param_specs; ip++)
        if (bb_param_spec_names_equal (input->instrument->param_specs[ip]->name, "duration"))
          {
            memset (&subvalues[ip], 0, sizeof (GValue));
            g_value_init (&subvalues[ip], BB_TYPE_DURATION);
            bb_value_set_duration (&subvalues[ip],
                                   BB_DURATION_UNITS_SECONDS,
                                   sub_render_info.note_duration_secs);
          }
        else
          {
            guint oip;
            for (oip = 0; oip < instrument->n_param_specs; oip++)
              if (bb_param_spec_names_equal (input->instrument->param_specs[ip]->name, instrument->param_specs[oip]->name))
                break;
            g_assert (oip < instrument->n_param_specs);
            subvalues[ip] = params[oip];                /* kinda cheating with the gvalue api */
          }
      start_sample = bb_value_get_duration_as_samples (&substart_value, render_info);
      if (start_sample < 0)
        start_sample = 0;
      else if ((guint)start_sample >= n)
        {
          g_free (subvalues);
          continue;
        }

      end_sample = (int) (render_info->sampling_rate
                           * (bb_value_get_duration_as_seconds (&substart_value, render_info)
                              + sub_render_info.note_duration_secs));
      if (end_sample < start_sample)
        end_sample = start_sample;
      sub_render_info.note_duration_samples = end_sample - start_sample;

      /* do subsynthesis */
      subrv = bb_instrument_synth (input->instrument, &sub_render_info, subvalues, &n_subrv);

      /* truncate the returned samples, if necessary */
      if (n_subrv + start_sample > n)
        n_subrv = n - start_sample;

      /* do mixing (TODO: add subsample interpolation) */
      {
        const double *in_sample;
        double *mod_sample;
        guint sample;
        in_sample = subrv;
        mod_sample = rv + start_sample;
        for (sample = 0; sample < n_subrv; sample++)
          *mod_sample++ += *in_sample++;
      }

      g_free (subrv);
      g_free (subvalues);
    }
  return rv;
}

static void
free_compose (gpointer data)
{
  Compose *compose = data;
  guint in;
  for (in = 0; in < compose->n_inputs; in++)
    {
      g_object_unref (compose->inputs[in].instrument);
      bb_program_unref (compose->inputs[in].start);
      bb_program_unref (compose->inputs[in].duration);
    }
  g_free (compose->inputs);
  g_free (compose);
}

/* each argument should be a triple:
 *   [instrument, start_time, duration]
 */
static BbInstrument *
construct_compose (guint            n_args,
                   char           **args,
                   GError         **error)
{
  ComposeInput *inputs = g_new (ComposeInput, n_args);
  guint i;
  GPtrArray *pspecs = g_ptr_array_new ();
  Compose *ci;
  BbInstrument *rv;

  /* duration must be arg 0 */
  g_ptr_array_add (pspecs, param_spec_duration ());

  for (i = 0; i < n_args; i++)
    {
      const char *at = args[i];
      const char *end;
      char *tmp;
      guint j, p;
      BbInstrument *instrument;
      BbProgram *start_program, *duration_program;
      GSK_SKIP_WHITESPACE (at);
      if (*at != '[')
	{
	  g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
	               "expected '[' as arg to compose<>");
	  return FALSE;
	}
      at++;
      end = bb_instrument_string_scan (at);
      tmp = g_strndup (at, end - at);
      instrument = bb_instrument_from_string (tmp);
      if (instrument == NULL)
	g_error ("errorr making instrument from %s", tmp);
      g_free (tmp);

      /* accumulate parameters from instrument */
      for (p = 0; p < instrument->n_param_specs; p++)
        {
          for (j = 0; j < pspecs->len; j++)
            if (bb_param_spec_names_equal (((GParamSpec*)(pspecs->pdata[j]))->name, instrument->param_specs[p]->name))
              break;
          if (j == pspecs->len)
            g_ptr_array_add (pspecs, instrument->param_specs[p]);
        }

      /* parse start/duration programs */
      at = end;
      GSK_SKIP_WHITESPACE (at);
      if (*at != ',')
        g_error ("missing ',' after instrument in 'compose' (str is '%s')",at);
      at++;
      GSK_SKIP_WHITESPACE (at);
      end = bb_program_string_scan (at);
      tmp = g_strndup (at, end - at);
      start_program = bb_program_parse_create_params (tmp, pspecs, instrument->n_param_specs, instrument->param_specs, error);
      g_free (tmp);
      if (start_program == NULL)
        {
          gsk_g_error_add_prefix (error, "in start program of arg %u to compose", i);
          return FALSE;
        }
      at = end;
      GSK_SKIP_WHITESPACE (at);
      if (*at != ',')
        g_error ("missing ',' after start-program in 'compose'");
      at++;
      GSK_SKIP_WHITESPACE (at);
      end = bb_program_string_scan (at);
      tmp = g_strndup (at, end - at);
      duration_program = bb_program_parse_create_params (tmp, pspecs, instrument->n_param_specs, instrument->param_specs, error);
      g_free (tmp);
      if (duration_program == NULL)
        {
          gsk_g_error_add_prefix (error, "in duration_program program of arg %u to compose", i);
          return FALSE;
        }

      inputs[i].start = start_program;
      inputs[i].duration = duration_program;
      inputs[i].instrument = instrument;
    }
  ci = g_new (Compose, 1);
  ci->n_inputs = n_args;
  ci->inputs = inputs;
  rv = bb_instrument_new_generic_v (NULL, pspecs->len, (GParamSpec**)pspecs->pdata);
  rv->synth_func = compose_synth;
  rv->data = ci;
  rv->free_data = free_compose;
  g_ptr_array_free (pspecs, TRUE);
  return rv;
}

BB_INIT_DEFINE(_bb_compose_init)
{
  bb_instrument_template_constructor_register ("compose", construct_compose);
}
