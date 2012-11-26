#include <stdlib.h>
#include "instrument-includes.h"
#include "../core/bb-vm.h"

typedef struct _TimedEvent TimedEvent;
typedef struct _ComplexInfo ComplexInfo;

struct _TimedEvent
{
  BbProgram *time_program;
  BbInstrumentEventType event_type;
  guint index;			/* param or action index */
  BbProgram *arg_program;	/* param or argument value program */
};

/* must always by first (although others may follow) */
#define PARAM_DURATION          0

struct _ComplexInfo
{
  BbInstrument *base;

  BbProgram **initial_value_programs;
  guint n_events;
  TimedEvent *events;
};

static int
compare_instrument_events_by_time (gconstpointer a, gconstpointer b)
{
  const BbInstrumentEvent *ie_a = a;
  const BbInstrumentEvent *ie_b = b;
  return (ie_a->time < ie_b->time) ? -1
       : (ie_a->time > ie_b->time) ? +1
       : 0;
}

static double *
complex__synth (BbInstrument *instrument,
                BbRenderInfo *render_info,
                GValue       *params,
                guint        *n_samples_out)
{
  ComplexInfo *ci = instrument->data;
  GValue *initial_values = g_newa (GValue, ci->base->n_param_specs);
  BbInstrumentEvent *events = g_new (BbInstrumentEvent, ci->n_events + 1); /* +1 for end-time */
  guint i, j;
  gdouble *rv;
  memset (initial_values, 0, sizeof (GValue) * ci->base->n_param_specs);

  for (i = 0; i < ci->base->n_param_specs; i++)
    {
      if (ci->initial_value_programs[i])
        {
          bb_vm_run_get_value_or_die (ci->initial_value_programs[i],
                                      render_info,
                                      instrument->n_param_specs, params,
                                      initial_values + i);
        }
      else
        {
          g_value_init (initial_values + i, G_PARAM_SPEC_VALUE_TYPE (ci->base->param_specs[i]));
          g_param_value_set_default (ci->base->param_specs[i], initial_values + i);
        }
    }

  /* evaluate events */
  for (i = 0; i < ci->n_events; i++)
    {
      /* evaluate the time of the event */
      GValue time_value = BB_VALUE_INIT;
      bb_vm_run_get_value_or_die (ci->events[i].time_program,
                                  render_info,
                                  instrument->n_param_specs, params,
                                  &time_value);

      /* convert that to seconds */
      if (G_VALUE_TYPE (&time_value) == G_TYPE_DOUBLE)
        {
          g_warning ("complex-instrument: time-program evaluated to a double: should evaluate to a duration (assuming units==note-length)");
          events[i].time = render_info->note_duration_secs * g_value_get_double (&time_value);
        }
      else if (G_VALUE_TYPE (&time_value) == BB_TYPE_DURATION)
        {
          events[i].time = bb_value_get_duration_as_seconds (&time_value, render_info);
        }
      else
        {
          g_error ("running time-program: returned invalid type %s", g_type_name (G_VALUE_TYPE (&time_value)));
        }

      events[i].type = ci->events[i].event_type;

      /* handle different types of events */
      switch (ci->events[i].event_type)
        {
        case BB_INSTRUMENT_EVENT_PARAM:
          {
          guint pindex = ci->events[i].index;
          memset (&events[i].info.param.value, 0, sizeof (GValue));
          g_value_init (&events[i].info.param.value, G_PARAM_SPEC_VALUE_TYPE (ci->base->param_specs[pindex]));
          bb_vm_run_get_value_or_die (ci->events[i].arg_program,
                                      render_info,
                                      instrument->n_param_specs, params,
                                      &events[i].info.param.value);
          events[i].info.param.index = pindex;
          break;
          }
        case BB_INSTRUMENT_EVENT_ACTION:
          {
            GError *error = NULL;
            BbInstrumentActionSpec *aspec = ci->base->actions[ci->events[i].index];
            GValue *ie = g_new0 (GValue, aspec->n_args);
            events[i].info.action.args = ie;
            events[i].info.action.index = ci->events[i].index;
            if (ci->events[i].arg_program == NULL)
              {
                g_assert (aspec->n_args == 0);
              }
            else
              {
                BbVm *vm = bb_vm_new (instrument->n_param_specs, params, FALSE);
                if (!bb_vm_run (vm, ci->events[i].arg_program, render_info, &error))
                  g_error ("complex: error parsing action params: %s", error->message);
                g_assert (vm->n_values == aspec->n_args);
                for (j = 0; j < aspec->n_args; j++)
                  {
                    g_value_init (ie + j, G_PARAM_SPEC_VALUE_TYPE (aspec->args[j]));
                    g_value_copy (vm->values + j, ie + j);
                  }
                bb_vm_free (vm);
              }
            break;
          }
        default:
          g_assert_not_reached ();
        }
    }
  events[i].time = render_info->note_duration_secs;

  /* sort events chronologically */
  qsort (events, ci->n_events, sizeof (BbInstrumentEvent), compare_instrument_events_by_time);

  /* perform the synthesis */
  rv = bb_instrument_complex_synth (ci->base,
                                    render_info,
                                    render_info->note_duration_secs,
                                    initial_values, ci->n_events, events, n_samples_out);


  /* free initial values and events */
  for (i = 0; i < ci->base->n_param_specs; i++)
    g_value_unset (initial_values + i);
  for (i = 0; i < ci->n_events; i++)
    {
      switch (events[i].type)
        {
        case BB_INSTRUMENT_EVENT_PARAM:
          g_value_unset (&events[i].info.param.value);
          break;
        case BB_INSTRUMENT_EVENT_ACTION:
          {
            BbInstrumentActionSpec *aspec = ci->base->actions[ci->events[i].index];
            for (j = 0; j < aspec->n_args; j++)
              g_value_unset (events[i].info.action.args + j);
            g_free (events[i].info.action.args);
          }
          break;
        default:
          g_assert_not_reached ();
        }
    }
  g_free (events);
  return rv;
}

static void
free_complex_info (gpointer data)
{
  ComplexInfo *ci = data;
  guint i;
  for (i = 0; i < ci->base->n_param_specs; i++)
    if (ci->initial_value_programs[i])
      bb_program_unref (ci->initial_value_programs[i]);
  for (i = 0; i < ci->n_events; i++)
    {
      bb_program_unref (ci->events[i].time_program);
      if (ci->events[i].arg_program)
        bb_program_unref (ci->events[i].arg_program);
    }
  g_free (ci->events);
  g_object_unref (ci->base);
  g_free (ci);
}

static gboolean
parse_name_program (BbInstrument *base,
                    GPtrArray    *params,
                    const char   *str,
                    char        **name_out,
                    BbProgram   **prog_out,
                    GError      **error)
{
  const char *start;
  char *prog_str;
  char *end;
  GSK_SKIP_WHITESPACE (str);
  start = str;
  GSK_SKIP_CHAR_TYPE (str, IS_IDENTIFIER_CHAR);
  *name_out = g_strndup (start, str - start);
  GSK_SKIP_WHITESPACE (str);
  if (*str == '(')
    str++;
  else
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                   "missing '(' in name-program");
      return FALSE;
    }
  GSK_SKIP_WHITESPACE (str);
  end = strchr (str, 0);
  while (end > str && isspace (*(end-1)))
    end--;
  if (*(end-1) == ')')
    end--;
  else
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                   "name-program expr doesn't end with ')'");
      return FALSE;
      end--;
    }
  while (end > str && isspace (*(end-1)))
    end--;
  if (end == str)
    {
      /* empty string== successful NUL program */
      *prog_out = NULL;
      return TRUE;
    }
  prog_str = g_strndup (str, end - str);
  *prog_out = bb_program_parse_create_params (prog_str, params, base->n_param_specs, base->param_specs, error);
  g_free (prog_str);
  if (*prog_out == NULL)
    return FALSE;
  return TRUE;
}

static BbInstrument *
construct_complex (guint            n_args,
                   char           **args,
                   GError         **error)
{
  BbInstrument *base;
  GPtrArray *params;
  GArray *timed_events;
  BbProgram **initial_value_programs;
  guint i;
  BbInstrument *rv;
  ComplexInfo *ci;
  if (n_args < 2)
    {
      g_error ("construct_complex: needs at least two args");
    }
  base = bb_instrument_from_string (args[0]);
  if (base == NULL)
    g_error ("error making instrument from '%s'", args[0]);
  params = g_ptr_array_new ();
  g_ptr_array_add (params, param_spec_duration ());

  timed_events = g_array_new (FALSE, FALSE, sizeof (TimedEvent));
  initial_value_programs = g_new0 (BbProgram *, base->n_param_specs);
  for (i = 1; i < n_args; i++)
    {
      const char *dotdot = strstr (args[i], "..");
      if (dotdot == NULL)
        {
          /* initial value */
          char *name;
          BbProgram *prog;
          guint index;
          GError *e = NULL;
          if (!parse_name_program (base, params, args[i], &name, &prog, &e))
            g_error ("error parsing name/program from %s: %s", args[i], e->message);
          if (!bb_instrument_lookup_param  (base, name, &index))
            g_error ("no param %s", name);
          if (initial_value_programs[index] != NULL)
            g_error ("init value of parameter %s specified multiply", name);
          initial_value_programs[index] = prog;
          g_free (name);
        }
      else
        {
          /* timed event */
          TimedEvent te;
          char *str;
          char *name;
          GError *e = NULL;
          if (!parse_name_program (base, params, dotdot + 2, &name, &te.arg_program, &e))
            g_error ("error parsing name/program from %s: %s", args[i], e->message);
          if (!bb_instrument_lookup_event  (base, name, &te.event_type, &te.index))
            g_error ("no event named '%s'", name);
          if (te.arg_program == NULL
           && (te.event_type != BB_INSTRUMENT_EVENT_ACTION
            || base->actions[te.index]->n_args != 0))
            g_error ("expected value, but got empty string");

          str = g_strndup (args[i], dotdot - args[i]);
          te.time_program = bb_program_parse_create_params (str, params, base->n_param_specs, base->param_specs, &e);
          if (te.time_program == NULL)
            g_error ("error parsing time-program from %s: %s", str, e ? e->message : "empty program");
          g_free (str);
          g_array_append_val (timed_events, te);
        }
    }
  rv = bb_instrument_new_generic_v (NULL,
                                    params->len,
                                    (GParamSpec **) (params->pdata));
  ci = g_new (ComplexInfo, 1);
  ci->base = g_object_ref (base);
  ci->initial_value_programs = initial_value_programs;
  ci->n_events = timed_events->len;
  ci->events = (TimedEvent *) g_array_free (timed_events, FALSE);
  rv->data = ci;
  rv->synth_func = complex__synth;
  rv->free_data = free_complex_info;

  g_ptr_array_free (params, TRUE);
  return rv;
}


BB_INIT_DEFINE(_bb_complex_init)
{
  bb_instrument_template_constructor_register ("complex", construct_complex);
}
