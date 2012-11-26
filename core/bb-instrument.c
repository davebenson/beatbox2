#include <string.h>
#include <math.h>
#include <gobject/gvaluecollector.h>
#include <gsk/gskghelpers.h>
#include "bb-instrument.h"
#include "bb-utils.h"
#include "bb-error.h"
#include "macros.h"

G_DEFINE_TYPE(BbInstrument, bb_instrument, G_TYPE_OBJECT);
static GHashTable *instruments = NULL;
static GHashTable *template_constructors = NULL;
static void bb_instrument_finalize (GObject *object)
{
  BbInstrument *i = BB_INSTRUMENT (object);
  if (i->free_data)
    i->free_data (i->data);
  G_OBJECT_CLASS (bb_instrument_parent_class)->finalize (object);
}
static void bb_instrument_class_init (BbInstrumentClass *class)
{
  instruments = g_hash_table_new (g_str_hash, g_str_equal);
  template_constructors = g_hash_table_new (g_str_hash, g_str_equal);
  G_OBJECT_CLASS (class)->finalize = bb_instrument_finalize;
}
static void bb_instrument_init (BbInstrument *instrument)
{
}

BbInstrument *bb_instrument_new_generic   (const char            *name,
                                           GParamSpec            *param_spec_0,
                                           ...)
{
  GPtrArray *arr = g_ptr_array_new ();
  GParamSpec *p;
  va_list args;
  BbInstrument *rv;
  va_start (args, param_spec_0);
  for (p = param_spec_0; p; p = va_arg (args, GParamSpec *))
    g_ptr_array_add (arr, p);
  va_end (args);

  rv = bb_instrument_new_generic_v (name, arr->len, (GParamSpec **) arr->pdata);
  g_ptr_array_free (arr, TRUE);
  return rv;
}

static void
check_is_special_param (BbInstrument *instrument,
                        const char   *name,
                        guint         index)
{
  if (strcmp (name, "duration") == 0)
    instrument->duration_param = index;
  if (strcmp (name, "volume") == 0)
    instrument->volume_param = index;
  if (strcmp (name, "frequency") == 0)
    instrument->frequency_param = index;
}

BbInstrument *bb_instrument_new_generic_v (const char            *name,
                                           guint                  n_param_specs,
                                           GParamSpec           **param_specs)
{
  BbInstrument *rv = g_object_new (BB_TYPE_INSTRUMENT, NULL);
  guint i;
  if (name)
    g_return_val_if_fail (g_hash_table_lookup (instruments, name) == NULL, NULL);
  rv->n_param_specs = n_param_specs;
  rv->param_specs = g_new (GParamSpec *, n_param_specs);
  rv->name = g_strdup (name);
  rv->duration_param = -1;
  rv->volume_param = -1;
  rv->frequency_param = -1;
  for (i = 0; i < n_param_specs; i++)
    {
      rv->param_specs[i] = g_param_spec_ref (param_specs[i]);
      g_param_spec_sink (param_specs[i]);

      check_is_special_param (rv, param_specs[i]->name, i);
    }
  if (name)
    g_hash_table_insert (instruments, rv->name, g_object_ref (rv));
  return rv;
}

BbInstrument *bb_instrument_new_from_simple_param (guint        n_simple_param,
                                                   const BbInstrumentSimpleParam *simple_params)
{
  GParamSpec **pspecs = g_newa (GParamSpec *, n_simple_param);
  guint i;
  memset (pspecs, 0, sizeof (GParamSpec *) * n_simple_param);
  for (i = 0; i < n_simple_param; i++)
    {
      guint p = simple_params[i].enum_value;
      if (p >= n_simple_param)
        g_error ("bb_instrument_new_from_simple_param: enum-value %u greater than number of params", p);
      if (pspecs[p] != NULL)
        g_error ("bb_instrument_new_from_simple_param: enum-value %u registered twice", p);
      pspecs[p] = g_param_spec_double (simple_params[i].name, simple_params[i].name, NULL,
                                       -1e6,1e6, simple_params[i].default_value, G_PARAM_READWRITE);
    }
  return bb_instrument_new_generic_v (NULL, n_simple_param, pspecs);
}

void
bb_instrument_set_name (BbInstrument *instr,
                        const char *name)
{
  g_return_if_fail (instr->name == NULL);
  instr->name = g_strdup (name);
  g_hash_table_insert (instruments, instr->name, g_object_ref (instr));
}

void          bb_instrument_add_action    (BbInstrument          *instrument,
                                           guint                  index,
                                           const char            *name,
                                           GParamSpec            *arg0,
                                           ...)
{
  GPtrArray *args_array = g_ptr_array_new ();
  GParamSpec *arg;
  va_list args;
  va_start (args, arg0);
  for (arg = arg0; arg; arg = va_arg (args, GParamSpec *))
    g_ptr_array_add (args_array, arg);
  va_end (args);

  bb_instrument_add_action_v (instrument, index, name, args_array->len, (GParamSpec **) args_array->pdata);
  g_ptr_array_free (args_array, TRUE);
}
  
void          bb_instrument_add_action_v  (BbInstrument          *instrument,
                                           guint                  index,
                                           const char            *name,
                                           guint                  n_args,
                                           GParamSpec           **args)
{
  BbInstrumentActionSpec *aspec;
  guint i;
  for (i = 0; i < instrument->n_param_specs; i++)
    g_assert (!bb_param_spec_names_equal (name, instrument->param_specs[i]->name));
  for (i = 0; i < instrument->n_actions; i++)
    g_assert (!bb_param_spec_names_equal (name, instrument->actions[i]->name));

  aspec = g_new (BbInstrumentActionSpec, 1);
  aspec->name = g_strdup (name);
  aspec->n_args = n_args;
  aspec->args = g_memdup (args, sizeof (GParamSpec *) * n_args);

  for (i = 0; i < n_args; i++)
    {
      g_param_spec_ref (args[i]);
      g_param_spec_sink (args[i]);
    }
  instrument->actions = g_renew (BbInstrumentActionSpec *, instrument->actions, instrument->n_actions + 1);
  instrument->actions[instrument->n_actions++] = aspec;
}

static void
bb_instrument_add_preset_valist    (BbInstrument          *instrument,
                                    const char            *preset_name,
                                    const char            *first_param_name,
                                    va_list                args)
{
  /* note that using a GArray of GValues is not really allowed:
     technically we should copy on resize like GValueArray does. */
  GArray *preset_params = g_array_new (FALSE, FALSE, sizeof (BbInstrumentPresetParam));
  const char *param_name;
  guint i;
  for (param_name = first_param_name;
       param_name != NULL;
       param_name = va_arg (args, const char *))
    {
      GType value_type;
      char *error = NULL;
      BbInstrumentPresetParam preset = {0, BB_VALUE_INIT};
      if (!bb_instrument_lookup_param (instrument, param_name, &preset.index))
        g_error ("no parameter '%s' for instrument (adding preset '%s')", param_name, preset_name);
      value_type = G_PARAM_SPEC_VALUE_TYPE (instrument->param_specs[preset.index]);
      g_value_init (&preset.value, value_type);
      G_VALUE_COLLECT (&preset.value, args, 0, &error);
      if (error != NULL)
        g_error ("error collecting value for param %s, preset %s: %s",
                 param_name, preset_name, error);
      g_array_append_val (preset_params, preset);
    }
  bb_instrument_add_preset_v2 (instrument, preset_name,
                               preset_params->len,
                               (BbInstrumentPresetParam *) (preset_params->data));
  for (i = 0; i < preset_params->len; i++)
    g_value_unset (&g_array_index (preset_params, BbInstrumentPresetParam, i).value);
  g_array_free (preset_params, TRUE);
}

void          bb_instrument_add_preset    (BbInstrument          *instrument,
                                           const char            *preset_name,
                                           const char            *first_param_name,
                                           ...)
{
  va_list args;
  va_start (args, first_param_name);
  bb_instrument_add_preset_valist (instrument, preset_name, first_param_name, args);
  va_end (args);
}

void          bb_instrument_add_preset_v  (BbInstrument          *instrument,
                                           const char            *preset_name,
                                           guint                  n_params,
                                           GParameter            *params)
{
  BbInstrumentPresetParam *pparams = g_new (BbInstrumentPresetParam, n_params);
  guint i;
  for (i = 0; i < n_params; i++)
    {
      if (!bb_instrument_lookup_param (instrument, params[i].name, &pparams[i].index))
        g_error ("no parameter named %s found for preset %s", params[i].name, preset_name);
      memcpy (&pparams[i].value, &params[i].value, sizeof (GValue));
    }
  bb_instrument_add_preset_v2 (instrument, preset_name, n_params, pparams);
  g_free (pparams);
}

void          bb_instrument_add_preset_v2 (BbInstrument          *instrument,
                                           const char            *preset_name,
                                           guint                  n_params,
                                           BbInstrumentPresetParam *params)
{
  BbInstrumentPreset *presets = g_renew (BbInstrumentPreset,
                                         instrument->presets,
                                         instrument->n_presets + 1);
  BbInstrumentPreset *new_preset = presets + instrument->n_presets++;
  guint i;

  new_preset->name = g_strdup (preset_name);
  new_preset->n_params = n_params;
  new_preset->params = g_new0 (BbInstrumentPresetParam, n_params);
  for (i = 0; i < n_params; i++)
    {
      new_preset->params[i].index = params[i].index;
      g_value_init (&new_preset->params[i].value, G_VALUE_TYPE (&params[i].value));
      g_value_copy (&params[i].value, &new_preset->params[i].value);
    }
}

void          bb_instrument_apply_preset  (BbInstrument          *instrument,
                                           const char            *name,
                                           GValue                *instrument_params)
{
  guint i;
  for (i = 0; i < instrument->n_presets; i++)
    if (strcmp (instrument->presets[i].name, name) == 0)
      {
        guint p;
        BbInstrumentPreset *preset = instrument->presets + i;
        for (p = 0; p < preset->n_params; p++)
          g_value_copy (&preset->params[p].value, instrument_params + preset->params[p].index);
        return;
      }
  g_error ("no preset named '%s' found", name);
}

void
bb_instrument_add_param (BbInstrument *instrument,
                         guint         index,
                         GParamSpec   *pspec)
{
  g_assert (index == instrument->n_param_specs);
  instrument->param_specs = g_renew (GParamSpec *, instrument->param_specs, index + 1);
  instrument->n_param_specs = index + 1;
  instrument->param_specs[index] = pspec;
  check_is_special_param (instrument, pspec->name, index);
  g_param_spec_ref (pspec);
  g_param_spec_sink (pspec);

}

gboolean
bb_instrument_lookup_param (BbInstrument *instrument,
                            const char   *name,
                            guint        *index_out)
{
  guint i;
  for (i = 0; i < instrument->n_param_specs; i++)
    if (bb_param_spec_names_equal (instrument->param_specs[i]->name, name))
      {
        *index_out = i;
        return TRUE;
      }
  return FALSE;
}

gboolean
bb_instrument_lookup_event (BbInstrument *instrument,
                            const char   *name,
                            BbInstrumentEventType *type_out,
                            guint        *index_out)
{
  guint i;
  for (i = 0; i < instrument->n_param_specs; i++)
    if (bb_param_spec_names_equal (instrument->param_specs[i]->name, name))
      {
        *type_out = BB_INSTRUMENT_EVENT_PARAM;
        *index_out = i;
        return TRUE;
      }
  for (i = 0; i < instrument->n_actions; i++)
    if (bb_param_spec_names_equal (instrument->actions[i]->name, name))
      {
        *type_out = BB_INSTRUMENT_EVENT_ACTION;
        *index_out = i;
        return TRUE;
      }
  return FALSE;
}

/* --- synthesis --- */
static void
validate_params (const char *func_name,
                 BbInstrument *instrument,
                 GValue       *params)
{
  guint i;
  for (i = 0; i < instrument->n_param_specs; i++)
    if (G_VALUE_TYPE (params + i) != G_PARAM_SPEC_VALUE_TYPE (instrument->param_specs[i]))
      {
        char instr_name_buf[128];
        const char *instr_name;
        if (instrument->name)
          instr_name = instrument->name;
        else
          {
            instr_name = instr_name_buf;
            g_snprintf (instr_name_buf, sizeof (instr_name_buf), "%p", instrument);
          }
        g_error ("bb_instrument_synth invoked on instrument %s, with param %s set to a %s (expected %s)",
                 instr_name,
                 instrument->param_specs[i]->name,
                 G_VALUE_TYPE_NAME (params + i),
                 g_type_name (G_PARAM_SPEC_VALUE_TYPE (instrument->param_specs[i])));
      }
}

static void
validate_action (BbInstrument           *instrument,
                 BbInstrumentActionSpec *aspec,
                 const GValue           *args)
{
  guint i;
  for (i = 0; i < aspec->n_args; i++)
    if (G_VALUE_TYPE (args + i) != G_PARAM_SPEC_VALUE_TYPE (aspec->args[i]))
      g_error ("action %s of instrument %s: for param %u expected type %s, got type %s",
               aspec->name, instrument->name, i,
               g_type_name (G_VALUE_TYPE (args + i)),
               g_type_name (G_PARAM_SPEC_VALUE_TYPE (aspec->args[i])));
}

double *
bb_instrument_synth        (BbInstrument *instrument,
                            BbRenderInfo *render_info,
                            GValue       *params,
                            guint        *n_samples_out)
{
  validate_params ("bb_instrument_synth", instrument, params);
  if (instrument->synth_func == NULL)
    {
      if (instrument->complex_synth_func != NULL)
        g_error ("bb_instrument_synth called on complex instrument %s", instrument->name ? instrument->name : "*unnamed*");
      else
        g_error ("bb_instrument_synth called on invalid instrument %s", instrument->name ? instrument->name : "*unnamed*");
    }
  return instrument->synth_func (instrument, render_info, params, n_samples_out);
}

double *
bb_instrument_complex_synth(BbInstrument *instrument,
                            BbRenderInfo *render_info,
                            double        duration,
                            GValue       *init_params,
                            guint         n_events,
                            const BbInstrumentEvent *events,
                            guint        *n_samples_out)
{
  guint i;
  validate_params ("bb_instrument_complex_synth", instrument, init_params);
  if (instrument->complex_synth_func == NULL)
    {
      if (instrument->synth_func != NULL)
        g_error ("bb_instrument_complex_synth called on simple instrument %s", instrument->name ? instrument->name : "*unnamed*");
      else
        g_error ("bb_instrument_complex_synth called on invalid instrument %s", instrument->name ? instrument->name : "*unnamed*");
    }
  for (i = 0; i < n_events; i++)
    switch (events[i].type)
      {
      case BB_INSTRUMENT_EVENT_PARAM:
        g_assert (events[i].info.param.index < instrument->n_param_specs);
        if (G_VALUE_TYPE (&events[i].info.param.value) != G_PARAM_SPEC_VALUE_TYPE (instrument->param_specs[events[i].info.param.index]))
          g_error ("bb_instrument_complex_synth: instrument %s: event %u: got param %s, expected type %s, got %s",
                   instrument->name ? instrument->name : "*unnamed*",
                   i,
                   instrument->param_specs[events[i].info.param.index]->name,
                   g_type_name (G_PARAM_SPEC_VALUE_TYPE (instrument->param_specs[events[i].info.param.index])),
                   g_type_name (G_VALUE_TYPE (&events[i].info.param.value)));
        break;
      case BB_INSTRUMENT_EVENT_ACTION:
        g_assert (events[i].info.action.index < instrument->n_actions);
        validate_action (instrument, instrument->actions[events[i].info.action.index], events[i].info.action.args);
        break;
      default:
        g_assert_not_reached ();
      }
  return instrument->complex_synth_func (instrument, render_info, duration,
                                         init_params, n_events, events, n_samples_out);
}

/* helper function: interpolate a particular parameter */
typedef struct _TV TV;
struct _TV
{
  gdouble time;
  gdouble value;
};
void          bb_instrument_param_events_interpolate (BbInstrument      *instrument,
                                                      guint              param_index,
                                                      const GValue      *init_value,
                                                      const BbRenderInfo *render_info,
                                                      guint             n_events,
                                                      const BbInstrumentEvent *events,
                                                      BbInterpolationMode interp,
                                                      gdouble           *rv)
{
  TV *tv = g_newa (TV, n_events + 1);
  guint n_tv = 1;
  guint i;
  guint n = render_info->note_duration_samples;
  guint tv_at;
  guint sample_at;
  gdouble time_at;
  gdouble sample_time = 1.0 / render_info->sampling_rate;
  tv[0].time = 0;
  tv[0].value = g_value_get_double (init_value);
  for (i = 0; i < n_events; i++)
    if (events[i].type == BB_INSTRUMENT_EVENT_PARAM
     && events[i].info.param.index == param_index)
      {
        tv[n_tv].time = events[i].time;
        tv[n_tv].value = g_value_get_double (&events[i].info.param.value);
        n_tv++;
      }
  tv_at = 0;
  sample_at = 0;
  time_at = 0.0;
  while (tv_at + 1 < n_tv)
    {
      /* figure out the number of samples to use from this interval (possibly 0) */
      gint tmp = (int) floor (tv[tv_at + 1].time * render_info->sampling_rate);
      guint end_sample = (tmp < 0) ? 0 : ((guint)tmp + 1);
      gdouble frac, frac_step;
      gdouble delta_t;
      if (sample_at >= end_sample)
        {
          tv_at++;
          continue;
        }
      if (end_sample > n)
        end_sample = n;

      /* do the interpolation */
      delta_t = tv[tv_at+1].time - tv[tv_at].time;
      if (delta_t <= 0.0)
        {
          g_assert (sample_at + 1 == end_sample);
          rv[sample_at++] = tv[tv_at+1].value;
          time_at += sample_time;
          tv_at++;
          continue;
        }
      frac = (time_at - tv[tv_at].time) / delta_t;
      frac_step = sample_time / delta_t;
      while (sample_at < end_sample)
        {
          rv[sample_at++] = bb_interpolate_double (frac,
                                                   tv[tv_at].value,
                                                   tv[tv_at+1].value,
                                                   interp);
          time_at += sample_time;
          frac += frac_step;
        }
      tv_at++;
    }

  {
    gdouble final = tv[tv_at].value;
    while (sample_at < n)
      {
        time_at += sample_time;
        rv[sample_at++] = final;
      }
  }
}

/* --- parsing --- */
void
bb_instrument_template_constructor_register (const char     *name,
                                             BbInstrumentTemplateConstructor c)
{
  if (template_constructors == NULL)
    template_constructors = g_type_class_ref (BB_TYPE_INSTRUMENT);
  g_return_if_fail (g_hash_table_lookup (template_constructors, name) == NULL);
  g_hash_table_insert (template_constructors,
                       g_strdup (name), (gpointer) c);
}

BbInstrument *
bb_instrument_parse_string (const char            *str,
                            GError               **error)
{
  const char *end_ident;
  BbInstrument *rv;
  const char *next;
  char *template_name;
  GPtrArray *args;
  const char *at;
  BbInstrumentTemplateConstructor constructor;
  GSK_SKIP_WHITESPACE (str);
  if (instruments == NULL)
    g_type_class_ref (BB_TYPE_INSTRUMENT);
  rv = g_hash_table_lookup (instruments, str);
  if (rv)
    return g_object_ref (rv);
  end_ident = str;
  GSK_SKIP_CHAR_TYPE (end_ident, IS_IDENTIFIER_CHAR);
  if (end_ident == str)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                   "no identifer found parsing instrument string");
      return NULL;
    }
  next = end_ident;
  GSK_SKIP_WHITESPACE (next);
  if (*next == '\0')
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                   "no instrument %s was found", str);
      return NULL;
    }
  if (*next != '<')
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                   "expected < in parameterized-instrument");
      return NULL;
    }

  /* parse arguments */
  template_name = g_strndup (str, end_ident - str);
  at = next + 1;
  args = g_ptr_array_new ();
  for (;;)
    {
      const char *start_arg, *end_arg;
      guint balance = 0;
      guint pbalance = 0, brbalance = 0, bkbalance = 0;
      GSK_SKIP_WHITESPACE (at);
      if (*at == '>')
        break;
      if (*at == '\0')
        {
          g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                       "mismatched '<'");
          return NULL;
        }
      start_arg = at;
      end_arg = NULL;
      while (end_arg == NULL)
        {
          switch (*at)
            {
            case 0:
              g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                           "mismatched '<'");
              return NULL;
            case '{': brbalance++; at++; break;
            case '}': brbalance--; at++; break;
            case '[': bkbalance++; at++; break;
            case ']': bkbalance--; at++; break;
            case '(': pbalance++; at++; break;
            case ')': pbalance--; at++; break;
            case '<':
              balance++;
              at++;
              break;
            case '>':
              if (balance == 0)
                end_arg = at;
              else
                {
                  --balance;
                  at++;
                }
              break;
            case ',':
              if (balance == 0 && pbalance == 0 && bkbalance == 0 && brbalance == 0)
                end_arg = at;
              at++;
              break;
            default:
              at++;
              break;
            }
        }
      g_ptr_array_add (args, g_strstrip (g_strndup (start_arg, end_arg - start_arg)));
    }

  /* invoke constructor */
  constructor = (BbInstrumentTemplateConstructor) g_hash_table_lookup (template_constructors, template_name);
  if (constructor == NULL)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                   "no template type %s<> known", template_name);
      return NULL;
    }
  rv = constructor (args->len, (char **)(args->pdata), error);
  if (rv == NULL)
    {
      gsk_g_error_add_prefix (error, "error constructing template %s<>", template_name);
      return NULL;
    }
  g_ptr_array_foreach (args, (GFunc) g_free, NULL);
  g_ptr_array_free (args, TRUE);
  g_free (template_name);
  return rv;
}

BbInstrument *
bb_instrument_from_string (const char            *str)
{
  GError *error = NULL;
  BbInstrument *rv = bb_instrument_parse_string (str, &error);
  if (rv == NULL)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      return NULL;
    }
  return rv;
}

const char  * bb_instrument_string_scan   (const char            *start)
{
  guint abalance = 0, pbalance = 0, bkbalance = 0, bcbalance = 0;
  const char *min_end;
  GSK_SKIP_WHITESPACE (start);
  GSK_SKIP_CHAR_TYPE (start, IS_IDENTIFIER_CHAR);
  min_end = start;
  GSK_SKIP_WHITESPACE (start);
  if (*start == '<')
    {
      abalance++;
      start++;
      while (*start && (abalance || pbalance || bkbalance || bcbalance))
        {
          switch (*start)
            {
            case '<': abalance++; break;
            case '>': abalance--; break;
            case '(': pbalance++; break;
            case ')': pbalance--; break;
            case '[': bkbalance++; break;
            case ']': bkbalance--; break;
            case '{': bcbalance++; break;
            case '}': bcbalance--; break;
            }
          start++;
        }
    }
  else
    start = min_end;
  return start;
}

/* --- parameter mapping --- */
gboolean      bb_instrument_params_map    (guint                  n_instr_params,
                                           GParamSpec           **instr_params,
                                           guint                 *instr_param_indices,
                                           GPtrArray             *container_params,
                                           GError               **error)
{
  guint i;
  for (i = 0 ; i < n_instr_params; i++)
    {
      guint j;
      for (j = 0; j < container_params->len; j++)
        {
          GParamSpec *pspec = container_params->pdata[j];
          if (bb_param_spec_names_equal (pspec->name, instr_params[i]->name))
            {
              if (G_PARAM_SPEC_VALUE_TYPE (pspec) != G_PARAM_SPEC_VALUE_TYPE (instr_params[i]))
                {
                  g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_INVALID_ARG,
                               "parameter %s was first defined as %s, then attempted to redefine as %s",
                               pspec->name, g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
                               g_type_name (G_PARAM_SPEC_VALUE_TYPE (instr_params[i])));
                  return FALSE;
                }
              break;
            }
        }
      if (j == container_params->len)
        g_ptr_array_add (container_params, instr_params[i]);
      instr_param_indices[i] = j;
    }
  return TRUE;
}

gboolean      bb_instrument_map_params    (BbInstrument          *instrument,
                                           guint                 *instr_param_indices,
                                           GPtrArray             *container_params,
                                           GError               **error)
{
  return bb_instrument_params_map (instrument->n_param_specs, instrument->param_specs,
                                   instr_param_indices, container_params, error);
}
