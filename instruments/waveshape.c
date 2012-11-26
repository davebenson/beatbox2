#include "instrument-includes.h"
#include "../core/bb-vm.h"
#include "../core/bb-waveform.h"

typedef struct _WaveshapeInfo WaveshapeInfo;
struct _WaveshapeInfo
{
  BbInstrument *subinstrument;
  BbProgram *program;           /* program that constructs waveform */
};

static double *
waveshape_synth  (BbInstrument *instrument,
                  BbRenderInfo *render_info,
                  GValue       *params,
                  guint        *n_samples_out)
{
  WaveshapeInfo *info = instrument->data;
  gdouble *rv = bb_instrument_synth (info->subinstrument, render_info, params, n_samples_out);
  GValue waveform_value = BB_VALUE_INIT;
  BbWaveform *waveform;
  g_value_init (&waveform_value, BB_TYPE_WAVEFORM);
  bb_vm_run_get_value_or_die (info->program, render_info, instrument->n_param_specs, params, &waveform_value);
  waveform = g_value_get_boxed (&waveform_value);
  g_assert (waveform != NULL);
  bb_waveform_eval_array (waveform, rv, *n_samples_out);
  g_value_unset (&waveform_value);
  return rv;
}

static void
waveshape_info_free (gpointer data)
{
  WaveshapeInfo *info = data;
  bb_program_unref (info->program);
  g_object_unref (info->subinstrument);
  g_free (info);
}

static BbInstrument *
construct_waveshape (guint            n_args,
                     char           **args,
                     GError         **error)
{
  WaveshapeInfo *info;
  BbInstrument *subinstrument;
  GPtrArray *pspecs;
  guint i;
  BbProgram *program;
  BbInstrument *rv;

  if (n_args != 2)
    g_error ("construct_waveshape: expected two args (instrument and waveform)");
  subinstrument = bb_instrument_from_string (args[0]);
  pspecs = g_ptr_array_new ();
  for (i = 0; i < subinstrument->n_param_specs; i++)
    g_ptr_array_add (pspecs, subinstrument->param_specs[i]);
  program = bb_program_parse_create_params (args[1], pspecs, 0, NULL, error);
  if (program == NULL)
    g_error ("construct_waveshape: error parsing waveform program: %s", (*error)->message);
  info = g_new (WaveshapeInfo, 1);
  info->program = program;
  info->subinstrument = subinstrument;

  rv = bb_instrument_new_generic_v (NULL, pspecs->len, (GParamSpec**) (pspecs->pdata));
  g_ptr_array_free (pspecs, TRUE);
  rv->synth_func = waveshape_synth;
  rv->data = info;
  rv->free_data = waveshape_info_free;
  return rv;
}

BB_INIT_DEFINE(_bb_waveshape_init)
{
  bb_instrument_template_constructor_register ("waveshape", construct_waveshape);
}
