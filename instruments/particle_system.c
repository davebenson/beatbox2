/* we have a set of instruments that give
 * particle parameters and a set of instrument pairs.
 *
 * the first instrument in each pair
 * gives the probability density function.
 * the second instrument gives the random
 * particle sounds.  typically, a "random" instrument is
 * employed to make these sounds.
 * the "grain" instruments are suggested as underlying sounds.
 */

/* XXX: maybe 'grain_params' should be 'distribution_params',
   since they apply to the subinstrument or something.
   in any event, it's pretty confusing.  at least doc it here. */
/* particle_system<param gdouble frequency 1 20000 440,
                   grain_param [gdouble energy 0 1 0] param_map<exp-decay, halflife_time=10>,
                   grain [param_map<exp-decay, halflife_time=10>] param_map<grain0, frequency=frequency>
                  >
 */

#include "instrument-includes.h"
#include <gsk/gskghelpers.h>
#include "../core/bb-vm.h"

enum
{
  PARAM_DURATION,

  N_FIXED_PARAMS               /* not a parameter: this is the number of fixed parameters */
};

typedef struct _ParticleSystemInfo ParticleSystemInfo;
typedef struct _SubInstrument SubInstrument;
typedef struct _GrainParameter GrainParameter;

typedef struct _SubInstrumentParam SubInstrumentParam;
struct _SubInstrumentParam
{
  /* index in GrainParameter array or the main instrument's parameters */
  guint src_param_index;

  /* index in sub_instrument::grain::param_specs array */
  guint subinstrument_param_index;
};

/* A subinstrument.  The 'pdf' is evaluated,
   and then 'grains' are added in, according
   to that probability per sample. */
struct _SubInstrument
{
  BbInstrument *pdf;		/* probability per sample */
  guint *pdf_param_indices;
  BbInstrument *grain;

  /* mapping from the array of GrainParameter to the grain->param_specs array */
  guint n_si_grain_params;
  SubInstrumentParam *si_grain_params;

  /* mapping from the array of parameters for the overall instrument
     to the subinstruments parameters */
  guint n_main_params;
  SubInstrumentParam *main_params;
};

/* parameters to the random grain constructor--
   these are fixed for each sample.
   for example, they could be an "shake energy"
   or "damping" coefficient.
   the actual randomness in the sound must be handled
   by SubInstrument::grain. */
struct _GrainParameter
{
  GParamSpecDouble *pspec;
  BbInstrument *instrument;
  guint *instrument_param_indices;
};

struct _ParticleSystemInfo
{
  guint n_grain_params;
  GrainParameter *grain_params;

  guint n_sub_instruments;
  SubInstrument *sub_instruments;

  guint max_grain_param_or_pdf_n_params;
};


static double *
particle_system_synth  (BbInstrument *instrument,
                        BbRenderInfo *render_info,
                        GValue       *params,
                        guint        *n_samples_out)
{
  ParticleSystemInfo *info = instrument->data;
  guint n = render_info->note_duration_samples;
  gdouble *rv = g_new0 (gdouble, n);
  gdouble **grain_params = g_newa (gdouble *, info->n_grain_params);
  gdouble **pdfs = g_newa (gdouble *, info->n_sub_instruments);
  guint i, k;
  BbRenderInfo sub_render_info = *render_info;
  GValue *scratch = g_new (GValue, info->max_grain_param_or_pdf_n_params);
  sub_render_info.note_duration_samples = 0;
  sub_render_info.note_duration_secs = 0;
  sub_render_info.note_duration_beats = 0;

  g_message ("n-grain-params=%u",info->n_grain_params);

  /* compute the grain parameters */
  for (i = 0; i < info->n_grain_params; i++)
    {
      BbInstrument *grain_instr = info->grain_params[i].instrument;
      guint n_grain_samples;
      for (k = 0; k < grain_instr->n_param_specs; k++)
        scratch[k] = params[info->grain_params[i].instrument_param_indices[k]];
      grain_params[i] = bb_instrument_synth (grain_instr, render_info, scratch, &n_grain_samples);
      if (n_grain_samples < n)
        {
          guint k;
          gdouble def = info->grain_params[i].pspec->default_value;
          grain_params[i] = g_renew (gdouble, grain_params[i], n);
          for (k = n_grain_samples; k < n; k++)
            grain_params[i][k] = def;
        }
    }

  /* make the pdfs */
  for (i = 0; i < info->n_sub_instruments; i++)
    {
      BbInstrument *si = info->sub_instruments[i].pdf;
      guint n_pdf_samples;
      for (k = 0; k < si->n_param_specs; k++)
        scratch[k] = params[info->sub_instruments[i].pdf_param_indices[k]];
      pdfs[i] = bb_instrument_synth (si, render_info, scratch, &n_pdf_samples);
      if (n_pdf_samples < n)
        {
          pdfs[i] = g_renew (gdouble, pdfs[i], n);
          for (k = n_pdf_samples; k < n; k++)
            pdfs[i][k] = 0;
        }
    }

  rv = g_new0 (gdouble, n);
  *n_samples_out = n;

  /* setup subinstrument grain param values */
  GValue **subinstr_params;
  subinstr_params = g_newa (GValue *, info->n_sub_instruments);
  for (i = 0; i < info->n_sub_instruments; i++)
    {
      BbInstrument *grain = info->sub_instruments[i].grain;
      subinstr_params[i] = g_newa (GValue, grain->n_param_specs);
      memset (subinstr_params[i], 0, sizeof (GValue) * grain->n_param_specs);
      for (k = 0; k < grain->n_param_specs; k++)
        {
          //guint spi = info->sub_instruments[i].si_grain_params[k].subinstrument_param_index;
          //GValue *gp = subinstr_params[i] + spi;
          GParamSpec *pspec = grain->param_specs[k];
          GValue *gp = subinstr_params[i] + k;
          g_value_init (gp, G_PARAM_SPEC_VALUE_TYPE (pspec));
          g_param_value_set_default (pspec, gp);
        }
      for (k = 0; k < info->sub_instruments[i].n_main_params; k++)
        {
          SubInstrumentParam *sip = info->sub_instruments[i].main_params + k;
          g_value_copy (params + (sip->src_param_index),
                        subinstr_params[i] + sip->subinstrument_param_index);
        }
    }

  /* spackle the return-value with sound */
  for (i = 0; i < info->n_sub_instruments; i++)
    for (k = 0; k < n; k++)
      if (g_random_double () < pdfs[i][k])
        {
          /* create sample */
          SubInstrument *si = info->sub_instruments + i;
          BbInstrument *gi = si->grain;
          gdouble *sub;
          guint n_sub;
          guint p, s;
          const gdouble *src_at;
          gdouble *dst_at;

          for (p = 0; p < si->n_si_grain_params; p++)
            {
              SubInstrumentParam *sip = si->si_grain_params + p;
              GValue *value = &subinstr_params[i][sip->subinstrument_param_index];
              //value->data[0].v_double = grain_params[sip->src_param_index][k];
              g_value_set_double (value, grain_params[sip->src_param_index][k]);
              //g_message ("initialized grain param %s with %.6f",
                         //si->grain->param_specs[sip->subinstrument_param_index]->name,grain_params[sip->src_param_index][k]);
            }

          g_assert (gi->duration_param < 0);
          sub = bb_instrument_synth (gi, &sub_render_info, subinstr_params[i], &n_sub);

          /* mix it in */
          if (n_sub + k > n)
            n_sub = n - k;
          src_at = sub;
          dst_at = rv + k;
          for (s = 0; s < n_sub; s++)
            *dst_at++ += *src_at++;

          g_free (sub);
        }

  for (i = 0; i < info->n_grain_params; i++)
    g_free (grain_params[i]);
  for (i = 0; i < info->n_sub_instruments; i++)
    g_free (pdfs[i]);
  return rv;
}

static void
free_particle_system_info (gpointer data)
{
  ParticleSystemInfo *info = data;
  guint i;
  for (i = 0; i < info->n_grain_params; i++)
    {
      g_param_spec_unref (G_PARAM_SPEC (info->grain_params[i].pspec));
      g_object_unref (info->grain_params[i].instrument);
    }
  g_free (info->grain_params);
  for (i = 0; i < info->n_sub_instruments; i++)
    {
      g_object_unref (info->sub_instruments[i].pdf);
      g_object_unref (info->sub_instruments[i].grain);
      g_free (info->sub_instruments[i].si_grain_params);
      g_free (info->sub_instruments[i].main_params);
    }
  g_free (info->sub_instruments);
  g_free (info);
}

static BbInstrument *
construct_particle_system (guint            n_args,
                           char           **args,
                           GError         **error)
{
  ParticleSystemInfo *info;
  BbInstrument *rv;
  GArray *subinstruments = g_array_new (FALSE, FALSE, sizeof (SubInstrument));
  GArray *grain_params = g_array_new (FALSE, FALSE, sizeof (GrainParameter));
  GPtrArray *pspec_array = g_ptr_array_new ();
  guint i;

  /* common parameters */
  g_assert (pspec_array->len == PARAM_DURATION);
  g_ptr_array_add (pspec_array, param_spec_duration ());

  /* pass through first time to find parameters: 'grain_param' and 'param' directives */
  for (i = 0; i < n_args; i++)
    {
      const char *at = args[i];
      GSK_SKIP_WHITESPACE (at);
      if (g_str_has_prefix (at, "param") && g_ascii_isspace (at[5]))
        {
          GParamSpec *pspec;
          at += 5;
          GSK_SKIP_WHITESPACE (at);
          pspec = bb_parse_g_param_spec (at, error);
          if (pspec == NULL)
            {
              gsk_g_error_add_prefix (error, "construct_particle_system, in 'param'");
              return NULL;
            }
          g_param_spec_ref (pspec);
          g_param_spec_sink (pspec);
          g_ptr_array_add (pspec_array, pspec);
          g_message ("adding param %s", pspec->name);
        }
      else if (g_str_has_prefix (at, "grain_param") && g_ascii_isspace (at[11]))
        {
          GrainParameter gp;
          const char *end;
          char *gpspec_str;
          GParamSpec *pspec;
          at += 11;
          GSK_SKIP_WHITESPACE (at);
          if (*at != '[')
            {
              g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                           "expected '[' after grain_param");
              return NULL;
            }
          at++;
          GSK_SKIP_WHITESPACE (at);
          end = bb_program_string_scan (at);
          if (*end != ']')
            {
              g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                           "pspec after grain_param[ didn't end with ']'");
              return NULL;
            }
          gpspec_str = g_strndup (at, end - at);
          pspec = bb_parse_g_param_spec (gpspec_str, error);
          if (pspec == NULL)
            return NULL;
          g_free (gpspec_str);
          at = end + 1;
          GSK_SKIP_WHITESPACE (at);

          if (!G_IS_PARAM_SPEC_DOUBLE (pspec))
            {
              g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                           "grain_param %s was not a double-valued parameter (type=%s, value-type=%s)",
                           pspec->name,
                           g_type_name (G_PARAM_SPEC_TYPE (pspec)), 
                           g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)));
              return NULL;
            }
          gp.pspec = G_PARAM_SPEC_DOUBLE (pspec);

          /* parse instrument */
          gp.instrument = bb_instrument_parse_string (at, error);
          if (gp.instrument == NULL)
            {
              gsk_g_error_add_prefix (error, "error parsing instrument for 'grain_param', in construct_particle_system");
              g_param_spec_sink (pspec);
              return NULL;
            }
          gp.instrument_param_indices = g_new (guint, gp.instrument->n_param_specs);
          if (!bb_instrument_map_params (gp.instrument, gp.instrument_param_indices, pspec_array, error))
            {
              gsk_g_error_add_prefix (error, "mapping grain-param instrument parameters");
              g_param_spec_sink (pspec);
              return NULL;
            }
          g_array_append_val (grain_params, gp);
          g_param_spec_ref (pspec);
          g_param_spec_sink (pspec);

          g_ptr_array_add (pspec_array, pspec);
        }
      else if (g_str_has_prefix (at, "grain") && g_ascii_isspace (at[5]))
        {
          /* handled in the next pass through the data */
        }
      else
        {
          g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                       "error parsing particle_system: got '%s' instead of 'grain', 'param' or 'grain_param'",
                       at);
          return NULL;
        }
    }

  /* second pass: parse actual grain data */
  for (i = 0; i < n_args; i++)
    {
      const char *at = args[i];
      GSK_SKIP_WHITESPACE (at);
      if (g_str_has_prefix (at, "grain") && g_ascii_isspace (at[5]))
        {
          SubInstrument si;
          GArray *main_sip_array, *grain_param_sip_array;
          const char *end;
          char *pdf_str;
          guint k;
          at += 5;
          GSK_SKIP_WHITESPACE (at);
          if (*at != '[')
            {
              g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                           "expected '[' after param");
              return NULL;
            }
          at++;
          GSK_SKIP_WHITESPACE (at);
          end = bb_program_string_scan (at);

          pdf_str = g_strndup (at, end - at);
          at = end;
          GSK_SKIP_WHITESPACE (at);
          if (*at != ']')
            {
              g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                           "missing ']' after grain's pdf in construct_particle_system");
              return NULL;
            }
          at++;
          GSK_SKIP_WHITESPACE (at);
          si.pdf = bb_instrument_parse_string (pdf_str, error);
          g_free (pdf_str);
          if (si.pdf == NULL)
            {
              gsk_g_error_add_prefix (error, "error parsing pdf instrument for 'grain', in construct_particle_system");
              return NULL;
            }
          si.pdf_param_indices = g_new (guint, si.pdf->n_param_specs);
          if (!bb_instrument_map_params (si.pdf, si.pdf_param_indices, pspec_array, error))
            {
              gsk_g_error_add_prefix (error, "error mapping PDF parameters to particle_system instrument");
              return NULL;
            }
          si.grain = bb_instrument_parse_string (at, error);
          if (si.grain == NULL)
            {
              gsk_g_error_add_prefix (error, "error parsing grain instrument for 'grain', in construct_particle_system");
              return NULL;
            }

          /* construct the subinstrument parameter mapping info:
             "main_params" and "si_grain_params" */
          main_sip_array = g_array_new (FALSE, FALSE, sizeof (SubInstrumentParam));
          grain_param_sip_array = g_array_new (FALSE, FALSE, sizeof (SubInstrumentParam));
          for (k = 0; k < si.grain->n_param_specs; k++)
            {
              guint j;
              for (j = 0; j < grain_params->len; j++)
                if (bb_param_spec_names_equal (G_PARAM_SPEC (g_array_index (grain_params, GrainParameter, j).pspec)->name,
                                               si.grain->param_specs[k]->name))
                 break;
              if (j < grain_params->len)
                {
                  SubInstrumentParam sip;
                  sip.subinstrument_param_index = k;
                  sip.src_param_index = j;
                  g_array_append_val (grain_param_sip_array, sip);
                }
              else 
                {
                  for (j = 0; j < pspec_array->len; j++)
                    if (bb_param_spec_names_equal (G_PARAM_SPEC (pspec_array->pdata[j])->name,
                                                   si.grain->param_specs[k]->name))
                       break;
                  if (j < pspec_array->len)
                    {
                      SubInstrumentParam sip;
                      sip.subinstrument_param_index = k;
                      sip.src_param_index = j;
                      g_array_append_val (main_sip_array, sip);
                    }
                }
            }
          si.n_main_params = main_sip_array->len;
          si.main_params = (SubInstrumentParam *) g_array_free (main_sip_array, FALSE);
          si.n_si_grain_params = grain_param_sip_array->len;
          si.si_grain_params = (SubInstrumentParam *) g_array_free (grain_param_sip_array, FALSE);
          
          g_array_append_val (subinstruments, si);
        }
    }


  info = g_new (ParticleSystemInfo, 1);
  info->n_grain_params = grain_params->len;
  info->grain_params = (GrainParameter *) g_array_free (grain_params, FALSE);
  info->n_sub_instruments = subinstruments->len;
  info->sub_instruments = (SubInstrument *) g_array_free (subinstruments, FALSE);

  /* find the max number of value space needed for PDF or grain-param computation */
  {
    guint max = 0;
    guint k;
    for (k = 0; k < info->n_grain_params; k++)
      max = MAX (max, info->grain_params[k].instrument->n_param_specs);
    for (k = 0; k < info->n_sub_instruments; k++)
      max = MAX (max, info->sub_instruments[k].pdf->n_param_specs);
    info->max_grain_param_or_pdf_n_params = max;
  }

  rv = bb_instrument_new_generic_v (NULL, pspec_array->len, (GParamSpec **)(pspec_array->pdata));
  rv->synth_func = particle_system_synth;
  rv->data = info;
  rv->free_data = free_particle_system_info;
  g_ptr_array_free (pspec_array, TRUE);

  return rv;
}

BB_INIT_DEFINE(_bb_particle_system_init)
{
  bb_instrument_template_constructor_register ("particle_system", construct_particle_system);
}
