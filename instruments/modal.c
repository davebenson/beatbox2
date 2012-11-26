/* XXX: lots of stuff in this file, like bb_filter1_3_2_set_resonance(),
 * is sampling_rate dependent! */
#include "instrument-includes.h"
#include "../core/bb-filter1.h"
#include "../core/bb-waveform.h"
#include "../core/bb-input-file.h"

typedef struct _ModalInstrumentMode ModalInstrumentMode;
typedef struct _ModalInstrument ModalInstrument;

struct _ModalInstrumentMode
{
  gdouble frequency_offset;
  gdouble frequency_factor;
  gdouble resonance;
  gdouble gain;
};

struct _ModalInstrument
{
  char *name;
  guint n_modes;
  ModalInstrumentMode *modes;
  gdouble stick_hardness, strike_position, direct_gain;
  gdouble vibrato_gain, vibrato_frequency;
};

typedef struct _ModalModeState ModalModeState;
typedef struct _ModalState ModalState;

struct _ModalModeState
{
  BbFilter1 *filter;
};

struct _ModalState
{
  gdouble sampling_rate;

  gboolean is_striking;
  guint strike_data_pos;
  guint strike_len;
  gdouble *strike_data;

  guint n_modes;
  ModalModeState *modes;
  ModalInstrument *instrument;

  gdouble base_frequency;
  gdouble vibrato_pos, vibrato_step;

  gdouble damp_factor;
};

enum
{
  PARAM_FREQUENCY,
  PARAM_DAMPING,		/* XXX: not sampling_rate independent */
  //PARAM_STICK_HARDNESS,
  //PARAM_STRIKE_POSITION,
  //PARAM_DIRECT_GAIN,
  //PARAM_VIBRATO_GAIN,
  //PARAM_VIBRATO_FREQUENCY,
  PARAM_INSTRUMENT
};

static const BbInstrumentSimpleParam modal_params[] =
{
  { PARAM_FREQUENCY,             "frequency",                  440 },
  { PARAM_DAMPING,               "damping",                    0.0 },
  //{ PARAM_STICK_HARDNESS,        "stick_hardness",             0.5 },
  //{ PARAM_STRIKE_POSITION,       "strike_position",            0.5 },
  //{ PARAM_DIRECT_GAIN,           "direct_gain",                0.5 },
  //{ PARAM_VIBRATO_GAIN,          "vibrato_gain",               0.0 },
  //{ PARAM_VIBRATO_FREQUENCY,     "vibrato_frequency",          0.0 },
};

enum
{
  ACTION_STRIKE
};

static gdouble *
create_strike_data (gdouble stick_hardness,
                    gdouble gain,
                    gdouble sampling_rate,
                    guint  *n_samples_out)
{
  /* these are the parameters i made up to simulate marmstk1.raw. */
  static BbWaveform *linear_envelope = NULL;
  static BbFilter1 *my_filter = NULL;
  gdouble *rv;
  guint i;

  /* from ModelBar::setStickHardness, knowing that
     sampling_rate==22050 and wave-file length=256 samples. */
  guint n = 0.0232 * sampling_rate / pow (4.0, stick_hardness);

  /* this is the parameter for the one-pole used by stk */
  BbFilter1 *onepole = NULL;

  if (linear_envelope == NULL)
    {
      static const gdouble tv_pairs[] = { 0,0.2,
                                          0.08,0.15,
                                          0.1,0.7,
                                          0.4,0.17,
                                          0.5,0.19,
                                          0.75,0.08,
			                  0.85,0.0,
                                          1.0,0 };
      linear_envelope = bb_waveform_new_linear (G_N_ELEMENTS (tv_pairs)/2,
                                                tv_pairs);
    }
  if (my_filter == NULL)
    {
      my_filter = bb_filter1_butterworth (BB_BUTTERWORTH_BAND_PASS,
                                          sampling_rate,
                                          10000,
                                          1.0,
                                          10.0);
    }

  *n_samples_out = n;
  rv = g_new (gdouble, n);

  /* create enveloped noise */
  for (i = 0; i < n; i++)
    {
      gdouble v = bb_waveform_eval (linear_envelope, (gdouble) i / n);
      rv[i] = g_random_double_range (-v, v);
    }

  /* filter noise with our filter */
  //bb_filter1_run_buffer (my_filter, rv, rv, n);

  /* filter with onepole */
  onepole = bb_filter1_one_pole (1.0 - gain, 1.0);
  bb_filter1_run_buffer (onepole, rv, rv, n);
  bb_filter1_free (onepole);

  return rv;
}

static GHashTable *instruments;

static inline gdouble
compute_damp_factor (gdouble damping)
{
  /* XXX: this is sampling-rate dependent */
  return 1.0 - 0.03 * damping;
}

static void
load_instruments (void)
{
  GError *error = NULL;
  BbInputFile *file = bb_input_file_open ("instruments/modal.instruments", &error);
  instruments = g_hash_table_new (g_str_hash, g_str_equal);

  if (file == NULL)
    g_error ("error opening instruments/modal.instruments for reading: %s", error->message);

  while (!file->eof)
    {
      const char *at;
      char *name;
      const char *start;
      if (file->line[0] == '#'
       || file->line[0] == 0)
        {
          bb_input_file_advance (file);
          continue;
        }
      if (file->line[0] != '=')
        g_error ("expected '=NAME' (got %s)", file->line);
      at = file->line + 1;
      GSK_SKIP_WHITESPACE (at);
      start = at;
      GSK_SKIP_NONWHITESPACE (at);
      name = g_strndup (start, at - start);
      if (g_hash_table_lookup (instruments, name) != NULL)
        g_error ("modal instrument %s encountered twice", name);

      /* then a sequence of modes */
      bb_input_file_advance (file);
      GArray *modes = g_array_new (FALSE, FALSE, sizeof (ModalInstrumentMode));
      ModalInstrument *instr = g_new (ModalInstrument, 1);
      instr->name = name;
      instr->stick_hardness = 0.5;
      instr->strike_position = 0.561;
      instr->direct_gain = 0.0;
      instr->vibrato_gain = 0.0;
      instr->vibrato_frequency = 0.0;
      while (!file->eof)
        {
          const char *at = file->line;
          GSK_SKIP_WHITESPACE (at);
          if (*at == '=')
            {
              break;
            }
          if (*at == '#' || *at == 0)
            {
              bb_input_file_advance (file);
              continue;
            }
          if (memcmp (at, "mode", 4) == 0
           && isspace (at[4]))
            {
              ModalInstrumentMode mode;
              at += 4;
              GSK_SKIP_WHITESPACE (at);
              if (sscanf (at, "%lf %lf %lf %lf",
                          &mode.frequency_offset,
                          &mode.frequency_factor,
                          &mode.resonance,
                          &mode.gain) != 4)
                g_error ("could not parse modeline");
              bb_input_file_advance (file);
              g_array_append_val (modes, mode);
              continue;
            }
          if (g_str_has_prefix (at, "stick_hardness"))
            {
              at += strlen ("stick_hardness");
              GSK_SKIP_WHITESPACE (at);
              if (sscanf (at, "%lf", &instr->stick_hardness) != 1)
                g_error ("could not parse stick_hardness");
              bb_input_file_advance (file);
              continue;
            }
          if (g_str_has_prefix (at, "strike_position"))
            {
              at += strlen ("strike_position");
              GSK_SKIP_WHITESPACE (at);
              if (sscanf (at, "%lf", &instr->strike_position) != 1)
                g_error ("could not parse strike_position");
              bb_input_file_advance (file);
              continue;
            }
          if (g_str_has_prefix (at, "direct_gain"))
            {
              at += strlen ("direct_gain");
              GSK_SKIP_WHITESPACE (at);
              if (sscanf (at, "%lf", &instr->direct_gain) != 1)
                g_error ("could not parse direct_gain");
              bb_input_file_advance (file);
              continue;
            }
          if (g_str_has_prefix (at, "vibrato_gain"))
            {
              at += strlen ("vibrato_gain");
              GSK_SKIP_WHITESPACE (at);
              if (sscanf (at, "%lf", &instr->vibrato_gain) != 1)
                g_error ("could not parse vibrato_gain");
              bb_input_file_advance (file);
              continue;
            }
          if (g_str_has_prefix (at, "vibrato_frequency"))
            {
              at += strlen ("vibrato_frequency");
              GSK_SKIP_WHITESPACE (at);
              if (sscanf (at, "%lf", &instr->vibrato_frequency) != 1)
                g_error ("could not parse vibrato_frequency");
              bb_input_file_advance (file);
              continue;
            }
          g_error ("unexpected line %s in modal instrument file", file->line);
        }
      instr->n_modes = modes->len;
      instr->modes = (ModalInstrumentMode *) g_array_free (modes, FALSE);
      g_hash_table_insert (instruments, name, instr);
    }
  bb_input_file_close (file);
}

static void
set_existing_filter (ModalState *state,
                     guint       mode,
                     BbFilter1  *filter)
{
  ModalInstrumentMode *imode = state->instrument->modes + mode;
  gdouble freq = imode->frequency_offset + imode->frequency_factor * state->base_frequency;
  gdouble gain = imode->gain;
  gdouble ppos = state->instrument->strike_position * G_PI;
  g_assert (mode < state->n_modes);
  switch (mode)
    {
    case 0: gain *= 0.12 * sin (ppos); break;
    case 1: gain *= -0.03 * sin (0.05 + 3.9 * ppos); break;
    case 2: gain *= 0.12 * sin (-0.05 + 11 * ppos); break;
    }

//g_message("filter params: gain=%.6f,freq=%.6f,res=%.6f",gain,freq,imode->resonance);
  bb_filter1_3_2_set_resonance (filter, gain, freq, state->sampling_rate,
                                state->damp_factor * imode->resonance, FALSE);
}
static BbFilter1 *
get_filter (ModalState *state,
            guint       mode)
{
  BbFilter1 *filter = bb_filter1_new_3_2_defaults();
  set_existing_filter (state, mode, filter);
  return filter;
}

static void
set_instrument (ModalState *state,
                const char *instr_name)
{
  ModalInstrument *instr;
  guint i;
  if (instruments == NULL)
    load_instruments ();
  instr = g_hash_table_lookup (instruments, instr_name);
  if (instr == NULL)
    g_error ("set_instrument: no modal-instrument %s found", instr_name);

  /* free old instrument data */
  for (i = 0; i < state->n_modes; i++)
    bb_filter1_free (state->modes[i].filter);
  g_free (state->modes);

  /* create new filter bank */
  state->n_modes = instr->n_modes;
  state->modes = g_new (ModalModeState, state->n_modes);
  state->instrument = instr;
  //state->direct_gain = instr->direct_gain;
  //state->vibrato_gain = instr->vibrato_gain;
  state->vibrato_step = BB_2PI * instr->vibrato_frequency / state->sampling_rate;
  for (i = 0; i < state->n_modes; i++)
    {
      state->modes[i].filter = get_filter (state, i);
    }
}

static inline void
update_filters (ModalState *state)
{
  guint i;
  for (i = 0; i < state->n_modes; i++)
    set_existing_filter (state, i, state->modes[i].filter);
}
static double *
modal_complex_synth (BbInstrument *instrument,
                     BbRenderInfo *render_info,
                     double        duration,
                     GValue       *init_params,
                     guint         n_events,
                     const BbInstrumentEvent *events,
                     guint        *n_samples_out)
{
  double rate = render_info->sampling_rate;
  guint n = render_info->note_duration_samples;
  ModalState state;
  gdouble *rv = g_new (gdouble, n);
  guint sample_at = 0, event_at = 0;

  state.is_striking = FALSE;
  state.strike_data_pos = 0;
  state.strike_len = 0;
  state.strike_data = NULL;

  state.n_modes = 0;
  state.modes = NULL;

  //state.direct_gain = g_value_get_double (init_params + PARAM_DIRECT_GAIN);
  //state.stick_hardness = g_value_get_double (init_params + PARAM_STICK_HARDNESS);
  //state.strike_position = g_value_get_double (init_params + PARAM_STRIKE_POSITION);
  state.base_frequency = g_value_get_double (init_params + PARAM_FREQUENCY);
  //state.vibrato_gain = g_value_get_double (init_params + PARAM_VIBRATO_GAIN);
  //state.vibrato_frequency = g_value_get_double (init_params + PARAM_VIBRATO_FREQUENCY);
  state.vibrato_step = 0;
  state.vibrato_pos = 0;
  state.sampling_rate = rate;
  state.damp_factor = compute_damp_factor (g_value_get_double (init_params + PARAM_DAMPING));

  set_instrument (&state, g_value_get_string (init_params + PARAM_INSTRUMENT));

  while (sample_at < n)
    {
      guint end_sample = n;
      while (event_at < n_events)
        {
          guint event_sample = (int)(events[event_at].time * rate);
          if (event_sample > sample_at)
            {
              end_sample = event_sample;
              break;
            }
          else if (events[event_at].type == BB_INSTRUMENT_EVENT_PARAM)
            {
              const GValue *v = &events[event_at].info.param.value;
              switch (events[event_at].info.param.index)
                {
                case PARAM_FREQUENCY:
                  state.base_frequency = g_value_get_double (v);
                  update_filters (&state);
                  break;
                case PARAM_DAMPING:
                  state.damp_factor = compute_damp_factor (g_value_get_double (v));
                  update_filters (&state);
                  break;
                case PARAM_INSTRUMENT:
                  set_instrument (&state, g_value_get_string (v));
                  break;
                default:
                  g_error ("%s, %u: no handling for event for param %s",
                           __FILE__, __LINE__,
                           instrument->param_specs[events[event_at].info.param.index]->name);
                }
            }
          else if (events[event_at].type == BB_INSTRUMENT_EVENT_ACTION)
            {
              GValue *args = events[event_at].info.action.args;
              switch (events[event_at].info.action.index)
                {
                case ACTION_STRIKE:
                  {
                    if (state.is_striking)
                      {
                        g_free (state.strike_data);
                      }
                    state.strike_data = create_strike_data (state.instrument->stick_hardness,
                                                            g_value_get_double (args + 0),
                                                            rate,
                                                            &state.strike_len);
                    state.strike_data_pos = 0;
                    state.is_striking = TRUE;
                    break;
                  } 
                default:
                  g_error ("unknown action %u (%s,%d)",
                           events[event_at].info.action.index, __FILE__, __LINE__);
                }
            }
          else
            g_error ("event of unknown type %u", events[event_at].type);
          event_at++;
        }
      
      while (sample_at < end_sample)
        {
          gdouble in;
          gdouble filtered;
          gdouble mixed;
          gdouble direct_gain;
          guint i;
          if (state.is_striking)
            {
              in = state.strike_data[state.strike_data_pos++];
              if (state.strike_data_pos >= state.strike_len)
                {
                  state.is_striking = FALSE;
                  g_free (state.strike_data);
                  state.strike_data = NULL;
                }
            }
          else
            in = 0;

          filtered = 0;
          for (i = 0; i < state.n_modes; i++)
            filtered += bb_filter1_run (state.modes[i].filter, in);

          direct_gain = state.instrument->direct_gain;
          mixed = (1.0 - direct_gain) * filtered
                + direct_gain * in;
          //g_message ("input=%.6f, filter-output=%.6f; mixed=%.6f", in,filtered,mixed);

          if (state.instrument->vibrato_gain != 0.0)
            {
              state.vibrato_pos += state.vibrato_step;
              if (state.vibrato_pos > BB_2PI)
                state.vibrato_pos -= BB_2PI;
              mixed *= 1.0 + sin(state.vibrato_pos) * state.instrument->vibrato_gain;
            }

          rv[sample_at] = mixed;

          /* update sample index */
          sample_at++;
        }
    }
  *n_samples_out = n;
  {
    guint i;
    for (i = 0; i < state.n_modes; i++)
      bb_filter1_free (state.modes[i].filter);
  }
  g_free (state.modes);
  if (state.is_striking)
    g_free (state.strike_data);
  return rv;
}

BB_INIT_DEFINE(_bb_modal_init)
{
  BbInstrument *instrument;
  instrument = bb_instrument_new_from_simple_param (G_N_ELEMENTS (modal_params),
                                                    modal_params);
  bb_instrument_add_action (instrument,
                            ACTION_STRIKE,
                            "strike",
                            g_param_spec_double ("amplitude", "Amplitude", NULL,
                                                 0, 1, 1, 0),
                            NULL);
  bb_instrument_add_param (instrument,
                           PARAM_INSTRUMENT,
                           g_param_spec_string ("instrument", "Instrument", NULL,
                                                "marimba", 0));
  bb_instrument_set_name (instrument, "modal");
  instrument->complex_synth_func = modal_complex_synth;
}
