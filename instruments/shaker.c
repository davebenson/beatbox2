#include "instrument-includes.h"


#define MIN_ENERGY      0.3
#define MAX_SHAKE       2000.0

#define EVAL_MACRO_FOR_ALL_SHORTNAMES(macro) \
  macro(MARACA, maraca) \
  macro(CABASA, cabasa) \
  macro(SEKERE, sekere) \
  macro(GUIRO, guiro) \
  macro(WATER_DROPS, water_drops) \
  macro(BAMBOO_CHIMES, bamboo_chimes) \
  macro(TAMBOURINE, tambourine) \
  macro(SLEIGH_BELLS, sleigh_bells) \
  macro(STICKS, sticks) \
  macro(CRUNCH, crunch) \
  macro(WRENCH, wrench) \
  macro(SAND_PAPER, sand_paper) \
  macro(COKE_CAN, coke_can) \
  macro(NEXTMUG, nextmug) \
  macro(PENNY_AND_MUG, penny_and_mug) \
  macro(NICKEL_AND_MUG, nickel_and_mug) \
  macro(DIME_AND_MUG, dime_and_mug) \
  macro(QUARTER_AND_MUG, quarter_and_mug) \
  macro(FRANC_AND_MUG, franc_and_mug) \
  macro(PESO_AND_MUG, peso_and_mug) \
  macro(BIG_ROCKS, big_rocks) \
  macro(LITTLE_ROCKS, little_rocks) \
  macro(TUNED_BAMBOO_CHIMES, tuned_bamboo_chimes)

typedef enum
{
#define EN(SN, sn) BB_SHAKER_##SN,
  EVAL_MACRO_FOR_ALL_SHORTNAMES(EN)
#undef EN
} BbShakerInstrumentType;

static GEnumValue shaker_instrument_type_values[] =
{
#define EN(SN, sn) {BB_SHAKER_##SN, "BB_SHAKER_" #SN, #sn},
  EVAL_MACRO_FOR_ALL_SHORTNAMES(EN)
#undef EN
  {0,NULL,NULL}
};

GType
bb_shaker_instrument_type_get_type (void)
{
  static GType rv = 0;
  if (rv == 0)
    rv = g_enum_register_static ("BbShakerInstrumentType",
                                 shaker_instrument_type_values);
  return rv;
}
#define BB_TYPE_SHAKER_INSTRUMENT_TYPE  (bb_shaker_instrument_type_get_type())

typedef struct _ShakerState ShakerState;
typedef struct _Freq Freq;

#define MAX_FREQS	8

struct _Freq
{
  double input;
  double outputs[2];
  double coeffs[2];
  double gain;
  double t_center_freq;
  double center_freq;
  double reson;
  double freq_rand;
  int freqalloc;
};

enum
{
  PARAM_DECAY,  /* -1 .. 1 */
  PARAM_COUNT,
  PARAM_RESONANCE_FREQUENCY, /* -1 .. 1 */
  PARAM_INSTRUMENT
};


static const BbInstrumentSimpleParam shaker_params[] =
{
  { PARAM_DECAY,             "decay",                  0.9 },	/* -1 ... +1 */
  { PARAM_COUNT,             "count",                  1.0 },
  { PARAM_RESONANCE_FREQUENCY,"resonance_frequency",    0.0 },
};

enum
{
  ACTION_SHAKE
};
struct _ShakerState
{
  BbShakerInstrumentType instr_type;

  int ratchet_pos, last_ratchet_pos;
  double ratchet, ratchet_delta;

  double shake_energy;
  double base_gain;
  guint n_freqs;
  Freq freqs[MAX_FREQS];
  double sound_level;
  double sound_decay;
  double system_decay;
  double n_objects;
  double total_energy;

  double final_z[3];
  double final_z_coeffs[3];
  double base_decay;
  double decay_scale;

  double ngain;

  /* NOTE: not in the Shakers from stk */
  double sampling_rate;
  guint tuned_bamb_which;
  double base_n_objects;
};

/* --- instrument parameters --- */
typedef struct _InstrInfo InstrInfo;
typedef struct _FreqInfo FreqInfo;
struct _FreqInfo
{
  double gain;  /* will be multiplied by 'temp' */
  gdouble freq;
  gdouble reson;
  gboolean freqalloc;
  gdouble freq_rand;
};
struct _InstrInfo
{
  BbShakerInstrumentType instr_type;
  gdouble n_objects;
  gdouble sound_decay;
  gdouble system_decay;
  gdouble default_decay;
  gdouble decay_scale;
  gdouble base_gain;
  guint n_freqs;
  FreqInfo *freqs;
  gdouble final_z[3];
};

/* --- Maraca --- */
static FreqInfo maraca_freq_info[1] =
{
  {
    .gain = 1.0,
    .freq = 3200.0,
    .reson = 0.96,
    .freqalloc = FALSE,
    .freq_rand = 0.0
  }
};

static InstrInfo maraca_instr_info =
{
  .instr_type = BB_SHAKER_MARACA,
  .n_objects = 25,
  .sound_decay = 0.95,
  .system_decay = 0.999,
  .default_decay = 0.999,
  .decay_scale = 0.9,
  .base_gain = 20.0,
  .n_freqs = G_N_ELEMENTS (maraca_freq_info),
  .freqs = maraca_freq_info,
  .final_z = { 1.0, -1.0, 0.0 }
};

/* --- Sekere --- */
static FreqInfo sekere_freq_info[1] =
{
  {
    .gain = 1.0,
    .freq = 5500.0,
    .reson = 0.6,
    .freqalloc = FALSE,
    .freq_rand = 0.0
  }
};

static InstrInfo sekere_instr_info =
{
  .instr_type = BB_SHAKER_SEKERE,
  .n_objects = 64,
  .sound_decay = 0.96,
  .system_decay = 0.999,
  .default_decay = 0.999,
  .decay_scale = 0.94,
  .base_gain = 20.0,
  .n_freqs = G_N_ELEMENTS (sekere_freq_info),
  .freqs = sekere_freq_info,
  .final_z = { 1.0, 0.0, -1.0 }
};

/* --- Sandpaper --- */
static FreqInfo sand_paper_freq_info[1] =
{
  { .gain = 1.0, .freq = 4500.0, .reson = 0.6, }
};

static InstrInfo sand_paper_instr_info =
{
  .instr_type = BB_SHAKER_SAND_PAPER,
  .n_objects = 128,
  .sound_decay = 0.999,
  .system_decay = 0.999,
  .default_decay = 0.999,
  .decay_scale = 0.97,
  .base_gain = 0.5,
  .n_freqs = G_N_ELEMENTS (sand_paper_freq_info),
  .freqs = sand_paper_freq_info,
  .final_z = { 1.0, 0.0, -1.0 }
};

/* --- Cabasa --- */
static FreqInfo cabasa_freq_info[1] =
{
  { .gain = 1.0, .freq = 3000.0, .reson = 0.7, }
};

static InstrInfo cabasa_instr_info =
{
  .instr_type = BB_SHAKER_CABASA,
  .n_objects = 512,
  .sound_decay = 0.96,
  .system_decay = 0.997,
  .default_decay = 0.997,
  .decay_scale = 0.97,
  .base_gain = 40.0,
  .n_freqs = G_N_ELEMENTS (cabasa_freq_info),
  .freqs = cabasa_freq_info,
  .final_z = { 1.0, -1.0, 0.0 }
};


/* --- Bamboo Wind Chimes --- */
static FreqInfo bamboo_chimes_freq_info[3] =
{
  {
    .gain = 1.0,
    .freq = 2800.0,
    .reson = 0.995,
    .freqalloc = TRUE,
    .freq_rand = 0.2
  },
  {
    .gain = 1.0,
    .freq = 0.8 * 2800.0,
    .reson = 0.995,
    .freqalloc = TRUE,
    .freq_rand = 0.2
  },
  {
    .gain = 1.0,
    .freq = 1.2 * 2800.0,
    .reson = 0.995,
    .freqalloc = TRUE,
    .freq_rand = 0.2
  }
};

static InstrInfo bamboo_chimes_instr_info =
{
  .instr_type = BB_SHAKER_BAMBOO_CHIMES,
  .n_objects = 1.25,
  .sound_decay = 0.95,
  .system_decay = 0.9999,
  .default_decay = 0.9999,
  .decay_scale = 0.7,
  .base_gain = 2.0,
  .n_freqs = G_N_ELEMENTS (bamboo_chimes_freq_info),
  .freqs = bamboo_chimes_freq_info,
  .final_z = { 1.0, 0.0, 0.0 }
};


/* ---  Tuned Bamboo Wind Chimes (Anklung) --- */
static FreqInfo tuned_bamboo_chimes_freq_info[7] =
{
  { .gain = 1.0, .freq = 1046.6, .reson = 0.996, },
  { .gain = 1.0, .freq = 1174.8, .reson = 0.996, },
  { .gain = 1.0, .freq = 1397.0, .reson = 0.996, },
  { .gain = 1.0, .freq = 1568.0, .reson = 0.996, },
  { .gain = 1.0, .freq = 1760.0, .reson = 0.996, },
  { .gain = 1.0, .freq = 2093.0, .reson = 0.996, },
  { .gain = 1.0, .freq = 2350.0, .reson = 0.996, },
};

static InstrInfo tuned_bamboo_chimes_instr_info =
{
  .instr_type = BB_SHAKER_TUNED_BAMBOO_CHIMES,
  .n_objects = 1.25,
  .sound_decay = 0.95,
  .system_decay = 0.9999,
  .default_decay = 0.9999,
  .decay_scale = 0.7,
  .base_gain = 1.0,
  .n_freqs = G_N_ELEMENTS (tuned_bamboo_chimes_freq_info),
  .freqs = tuned_bamboo_chimes_freq_info,
  .final_z = { 1.0, 0.0, -1.0 }
};

/* --- Water Drops --- */
#define WATER_DROPS_CENTER_FREQ0          450.0
#define WATER_DROPS_CENTER_FREQ1          600.0
#define WATER_DROPS_CENTER_FREQ2          750.0
#define WATER_DROPS_FREQ_SWEEP            1.0001
static FreqInfo water_drops_freq_info[3] =
{
#define WATER_DROP_FREQ(frequ)                  \
  {                                             \
    .gain = 1.0,                                \
    .freq = frequ,                              \
    .reson = 0.9985,                            \
    .freqalloc = TRUE,                          \
    .freq_rand = 0.2                            \
  }
  WATER_DROP_FREQ(450.0),
  WATER_DROP_FREQ(600.0),
  WATER_DROP_FREQ(750.0),
#undef WATER_DROP_FREQ
};

static InstrInfo water_drops_instr_info =
{
  .instr_type = BB_SHAKER_WATER_DROPS,
  .n_objects = 10,
  .sound_decay = 0.95,
  .system_decay = 0.996,
  .default_decay = 0.996,
  .decay_scale = 1.0,
  .base_gain = 1.0,
  .n_freqs = G_N_ELEMENTS (water_drops_freq_info),
  .freqs = water_drops_freq_info,
  .final_z = { 1.0, 0.0, 0.0 }
};

/* --- Tambourine --- */
#define TAMBOURINE_SHELL_GAIN   0.1
static FreqInfo tambourine_freq_info[3] =
{
  {   /* shell */
    .gain = TAMBOURINE_SHELL_GAIN,
    .freq = 2300,
    .reson = 0.96,
    .freqalloc = FALSE,
    .freq_rand = 0.0
  },
  {   /* cymb */
    .gain = 0.8,
    .freq = 5600,
    .reson = 0.99,
    .freqalloc = TRUE,
    .freq_rand = 0.05
  },
  {   /* cymb */
    .gain = 1.0,
    .freq = 8100,
    .reson = 0.99,
    .freqalloc = TRUE,
    .freq_rand = 0.05
  },
};

static InstrInfo tambourine_instr_info =
{
  .instr_type = BB_SHAKER_TAMBOURINE,
  .n_objects = 32,
  .sound_decay = 0.95,
  .system_decay = 0.9985,
  .default_decay = 0.9985,
  .decay_scale = 0.95,
  .base_gain = 5.0,
  .n_freqs = G_N_ELEMENTS (tambourine_freq_info),
  .freqs = tambourine_freq_info,
  .final_z = { 1.0, 0.0, 0.0 }
};

/* --- Sleighbells --- */
static FreqInfo sleigh_bells_freq_info[5] =
{
#define SLEIGH_BELLS_FREQ(frequency, gain_) \
  {                                         \
    .gain = gain_,                          \
    .freq = frequency,                      \
    .reson = 0.99,                          \
    .freqalloc = TRUE,                      \
    .freq_rand = 0.03,                      \
  }
  SLEIGH_BELLS_FREQ(2500.0, 1.0),
  SLEIGH_BELLS_FREQ(5300.0, 1.0),
  SLEIGH_BELLS_FREQ(6500.0, 1.0),
  SLEIGH_BELLS_FREQ(8300.0, 0.5),
  SLEIGH_BELLS_FREQ(9800.0, 0.3),
#undef SLEIGH_BELLS_FREQ
};
static InstrInfo sleigh_bells_instr_info =
{
  .instr_type = BB_SHAKER_SLEIGH_BELLS,
  .n_objects = 32,
  .sound_decay = 0.97,
  .system_decay = 0.9994,
  .default_decay = 0.9994,
  .decay_scale = 0.9,
  .base_gain = 1.0,
  .n_freqs = G_N_ELEMENTS (sleigh_bells_freq_info),
  .freqs = sleigh_bells_freq_info,
  .final_z = { 1.0, 0.0, -1.0 }
};


/* --- Guiro --- */
static FreqInfo guiro_freq_info[2] =
{
  { .gain = 1.0, .freq = 2500.0, .reson = 0.97 },
  { .gain = 1.0, .freq = 4000.0, .reson = 0.97 },
};
static InstrInfo guiro_instr_info =
{
  .instr_type = BB_SHAKER_GUIRO,
  .n_objects = 128,
  .sound_decay = 0.95,
  .system_decay = 0.97,
  .default_decay = 0.9999,
  .decay_scale = 1.0,
  .base_gain = 10.0,
  .n_freqs = G_N_ELEMENTS (guiro_freq_info),
  .freqs = guiro_freq_info,
  .final_z = { 0, 0, 0 }
};

/* --- Wrench --- */
static FreqInfo wrench_freq_info[2] =
{
  { .gain = 1.0, .freq = 3200.0, .reson = 0.990 },
  { .gain = 1.0, .freq = 8000.0, .reson = 0.992 },
};
static InstrInfo wrench_instr_info =
{
  .instr_type = BB_SHAKER_WRENCH,
  .n_objects = 128,
  .sound_decay = 0.95,
  .system_decay = 1.0,
  .default_decay = 0.9999,
  .decay_scale = 0.98,
  .base_gain = 5.0,
  .n_freqs = G_N_ELEMENTS (wrench_freq_info),
  .freqs = wrench_freq_info,
  .final_z = { 0, 0, 0 }
};

/* --- Coke-can --- */
static FreqInfo coke_can_freq_info[5] =
{
  /* helm */
  { .freq =  370, .reson = 0.990, .gain = 1.0 },

  /* metal */
  { .freq = 1025, .reson = 0.992, .gain = 1.8 },
  { .freq = 1424, .reson = 0.992, .gain = 1.8 },
  { .freq = 2149, .reson = 0.992, .gain = 1.8 },
  { .freq = 3596, .reson = 0.992, .gain = 1.8 },
};

static InstrInfo coke_can_instr_info =
{
  .instr_type = BB_SHAKER_COKE_CAN,
  .n_objects = 48,
  .sound_decay = 0.98,
  .system_decay = 0.999,
  .default_decay = 0.999,
  .decay_scale = 0.95,
  .base_gain = 0.8,
  .n_freqs = G_N_ELEMENTS (coke_can_freq_info),
  .freqs = coke_can_freq_info,
  .final_z = { 1.0, 0.0, -1.0 }
};

// PhOLIES (Physically-Oriented Library of Imitated Environmental
// Sounds), Perry Cook, 1997-8

/* ---  Stix1 --- */
static FreqInfo sticks_freq_info[1] =
{
  { .gain = 1.0, .freq = 5500.0, .reson = 0.6 }
};

static InstrInfo sticks_instr_info =
{
  .instr_type = BB_SHAKER_STICKS,
  .n_objects = 2.0,
  .sound_decay = 0.96,
  .system_decay = 0.998,
  .default_decay = 0.998,
  .decay_scale = 0.96,
  .base_gain = 30.0,
  .n_freqs = G_N_ELEMENTS (sticks_freq_info),
  .freqs = sticks_freq_info,
  .final_z = { 1.0, 0.0, -1.0 }
};

/* --- Crunch1 --- */
static FreqInfo crunch_freq_info[1] =
{
  { .gain = 1.0, .freq = 800.0, .reson = 0.95 }
};

static InstrInfo crunch_instr_info =
{
  .instr_type = BB_SHAKER_CRUNCH,
  .n_objects = 7.0,
  .sound_decay = 0.95,
  .system_decay = 0.99806,
  .default_decay = 0.99806,
  .decay_scale = 0.96,
  .base_gain = 20.0,
  .n_freqs = G_N_ELEMENTS (crunch_freq_info),
  .freqs = crunch_freq_info,
  .final_z = { 1.0, -1.0, 0.0 }
};

// Nextmug
#define NEXTMUG_SOUND_DECAY        0.97
#define NEXTMUG_SYSTEM_DECAY       0.9995
#define NEXTMUG_GAIN               0.8
#define NEXTMUG_NUM_PARTS          3
#define NEXTMUG_FREQ0              2123
#define NEXTMUG_FREQ1              4518
#define NEXTMUG_FREQ2              8856
#define NEXTMUG_FREQ3              10753
#define NEXTMUG_RES                0.997

/* --- mug base frequencies and instrument defs --- */
#define MUG_FREQ_BASE                                   \
  { .gain = 1.0, .freq =  2123, .reson = 0.997 },       \
  { .gain = 0.8, .freq =  4518, .reson = 0.997 },       \
  { .gain = 0.6, .freq =  8856, .reson = 0.997 },       \
  { .gain = 0.4, .freq = 10753, .reson = 0.997 }
#define MUG_INSTR(SHORTNAME, shortname)                 \
{                                                       \
  .instr_type = BB_SHAKER_##SHORTNAME,                  \
  .n_objects = 3.0,                                     \
  .sound_decay = 0.97,                                  \
  .system_decay = 0.9995,                               \
  .default_decay = 0.9995,                              \
  .decay_scale = 0.95,                                  \
  .base_gain = 0.8,                                     \
  .n_freqs = G_N_ELEMENTS (shortname##_freq_info),      \
  .freqs = shortname##_freq_info,                       \
  .final_z = { 1.0, -1.0, 0.0 }                         \
}

static FreqInfo nextmug_freq_info[4] =
{
  MUG_FREQ_BASE
};
static InstrInfo nextmug_instr_info
   = MUG_INSTR (NEXTMUG, nextmug);

/* --- penny & mug --- */
static FreqInfo penny_and_mug_freq_info[7] =
{
  MUG_FREQ_BASE,
  { .gain = 1.0, .freq = 11000, .reson = 0.999 },
  { .gain = 0.8, .freq =  5200, .reson = 0.999 },
  { .gain = 0.5, .freq =  3835, .reson = 0.999 },
};
static InstrInfo penny_and_mug_instr_info
  = MUG_INSTR (PENNY_AND_MUG, penny_and_mug);
  
/* --- nickel & mug --- */
static FreqInfo nickel_and_mug_freq_info[6] =
{
  MUG_FREQ_BASE,
  { .gain = 1.0, .freq =  5583, .reson = 0.9992 },
  { .gain = 0.8, .freq =  9255, .reson = 0.9992 },
  //{ .gain = 0.5, .freq =  9805, .reson = 0.9992 },
};
static InstrInfo nickel_and_mug_instr_info
  = MUG_INSTR (NICKEL_AND_MUG, nickel_and_mug);

/* --- dime & mug --- */
static FreqInfo dime_and_mug_freq_info[6] =
{
  MUG_FREQ_BASE,
  { .gain = 1.0, .freq =  4450, .reson = 0.9993 },
  { .gain = 0.8, .freq =  4974, .reson = 0.9993 },
};
static InstrInfo dime_and_mug_instr_info
  = MUG_INSTR (DIME_AND_MUG, dime_and_mug);

/* --- quarter & mug --- */
static FreqInfo quarter_and_mug_freq_info[6] =
{
  MUG_FREQ_BASE,
  { .gain = 1.3, .freq =  1708, .reson = 0.9995 },
  { .gain = 1.0, .freq =  8863, .reson = 0.9995 },
  //{ .gain = 0.8, .freq =  9045, .reson = 0.9995 },
};
static InstrInfo quarter_and_mug_instr_info
  = MUG_INSTR (QUARTER_AND_MUG, quarter_and_mug);

/* --- franc & mug --- */
static FreqInfo franc_and_mug_freq_info[6] =
{
  MUG_FREQ_BASE,
  { .gain = 0.7, .freq =  5583, .reson = 0.9995 },
  { .gain = 0.4, .freq =  11010, .reson = 0.9995 },
  //{ .gain = 0.3, .freq =  1917, .reson = 0.9995 },
};
static InstrInfo franc_and_mug_instr_info
  = MUG_INSTR (FRANC_AND_MUG, franc_and_mug);

/* --- peso & mug --- */
static FreqInfo peso_and_mug_freq_info[6] =
{
  MUG_FREQ_BASE,
  { .gain = 1.2, .freq =  7250, .reson = 0.9996 },
  { .gain = 0.7, .freq =  8150, .reson = 0.9996 },
  //{ .gain = 0.3, .freq =  10060, .reson = 0.9996 },
};
static InstrInfo peso_and_mug_instr_info
  = MUG_INSTR (PESO_AND_MUG, peso_and_mug);

/* --- Big Gravel --- */
static FreqInfo big_rocks_freq_info[1] =
{
  { .gain = 1.0, .freq = 6460.0, .reson = 0.932,
    .freqalloc = TRUE, .freq_rand = 0.11 }
};

static InstrInfo big_rocks_instr_info =
{
  .instr_type = BB_SHAKER_BIG_ROCKS,
  .n_objects = 23,
  .sound_decay = 0.98,
  .system_decay = 0.9965,
  .default_decay = 0.9965,
  .decay_scale = 0.95,
  .base_gain = 20.0,
  .n_freqs = G_N_ELEMENTS (big_rocks_freq_info),
  .freqs = big_rocks_freq_info,
  .final_z = { 1.0, -1.0, 0.0 }
};


/* --- Little Gravel --- */
static FreqInfo little_rocks_freq_info[1] =
{
  { .gain = 1.0, .freq = 9000.0, .reson = 0.843,
    .freqalloc = TRUE, .freq_rand = 0.18 }
};

static InstrInfo little_rocks_instr_info =
{
  .instr_type = BB_SHAKER_LITTLE_ROCKS,
  .n_objects = 1600,
  .sound_decay = 0.98,
  .system_decay = 0.99586,
  .default_decay = 0.99586,
  .decay_scale = 0.95,
  .base_gain = 20.0,
  .n_freqs = G_N_ELEMENTS (little_rocks_freq_info),
  .freqs = little_rocks_freq_info,
  .final_z = { 1.0, -1.0, 0.0 }
};

static InstrInfo *all_instr_infos[] =
{
#define EN(SN, sn) &sn##_instr_info,
  EVAL_MACRO_FOR_ALL_SHORTNAMES(EN)
#undef EN
};

static inline gdouble
water_drops_tick (ShakerState *state)
{
  gdouble data;
  int j;
  Freq *f;
  state->shake_energy *= state->system_decay;     /* Exponential system decay */
  if ((guint) g_random_int_range (0, 32767) < state->n_objects)
    {
      guint j = g_random_int_range (0, 3);
      double r = 0.75 + 0.25 * j + g_random_double_range (-0.25, 0.25);
      Freq *f = state->freqs + j;
      state->sound_level = state->shake_energy;
      f->center_freq = WATER_DROPS_CENTER_FREQ1 * r;
      f->gain = g_random_double ();
    }

  for (j = 0; j < 3; j++)
    {
      Freq *f = state->freqs + j;
      f->gain *= f->reson;
      if (f->gain > 0.001)
        f->center_freq  *= WATER_DROPS_FREQ_SWEEP;
      f->coeffs[0] = -f->reson * 2.0 * cos (f->center_freq * BB_2PI / state->sampling_rate);
  }

  /* Each (all) event(s)  decay(s) exponentially  */
  state->sound_level *= state->sound_decay;

  f = state->freqs;
  f[0].input = g_random_double_range (-state->sound_level, state->sound_level);
  f[1].input = f[0].input * f[1].gain;
  f[2].input = f[0].input * f[2].gain;
  f[0].input = f[0].input * f[0].gain;

  data = 0;
  for (j = 0; j < 3; j++)
    {
      f[j].input -= f[j].outputs[0] * f[j].coeffs[0];
      f[j].input -= f[j].outputs[1] * f[j].coeffs[1];
      f[j].outputs[1] = f[j].outputs[0];
      f[j].outputs[0] = f[j].input;
      data += f[j].gain * f[j].outputs[0];
    }

  state->final_z[2] = state->final_z[1];
  state->final_z[1] = state->final_z[0];
  state->final_z[0] = data * 4;

  return state->final_z[2] - state->final_z[0];
}


static inline gdouble 
tuned_bamboo_tick(ShakerState *state)
{
  double data;

  if (state->shake_energy > MIN_ENERGY)
    {
      guint i;
      guint which;
      state->shake_energy *= state->system_decay;    /* Exponential system decay */
      if ((guint) g_random_int_range (0, 1024) < state->n_objects)
        {
          state->sound_level += state->shake_energy;
          state->tuned_bamb_which = g_random_int_range (0, 7);
        }
      which = state->tuned_bamb_which;
      for (i = 0; i < state->n_freqs; i++)
        state->freqs[i].input = 0;
      /* Actual Sound is Random */
      state->freqs[which].input = state->sound_level * g_random_double_range (-1, 1);
      state->sound_level *= state->sound_decay;      // Exponential Sound decay 
      state->final_z[2] = state->final_z[1];
      state->final_z[1] = state->final_z[0];
      state->final_z[0] = 0;
      for (i = 0; i < state->n_freqs; i++)
        {
          /* do resonant filter calculations */
          Freq *f = state->freqs + i;
          f->input -= f->outputs[0] * f->coeffs[0];
          f->input -= f->outputs[1] * f->coeffs[1];
          f->outputs[1] = f->outputs[0];
          f->outputs[0] = f->input;
          state->final_z[0] += f->gain * f->outputs[1];
        }

      /*  Extra zero(s) for shape */
      data = state->final_z_coeffs[0] * state->final_z[0]
           + state->final_z_coeffs[1] * state->final_z[1]
           + state->final_z_coeffs[2] * state->final_z[2];
      data *= 0.0001;
      return CLAMP (data, -1.0, 1.0);
    }
  else
    return 0;
}

static inline gdouble
ratchet_tick(ShakerState *state)
{
  Freq *f = state->freqs;
  if (g_random_double_range (0, 1024) < state->n_objects)
    state->sound_level += 512 * state->ratchet * state->total_energy;
  f[0].input = state->sound_level;
  f[0].input *= g_random_double_range (-1, 1) * state->ratchet;
  state->sound_level *= state->sound_decay;

  f[1].input = f[0].input;
  f[0].input -= f[0].outputs[0]*f[0].coeffs[0];
  f[0].input -= f[0].outputs[1]*f[0].coeffs[1];
  f[0].outputs[1] = f[0].outputs[0];
  f[0].outputs[0] = f[0].input;
  f[1].input -= f[1].outputs[0]*f[1].coeffs[0];
  f[1].input -= f[1].outputs[1]*f[1].coeffs[1];
  f[1].outputs[1] = f[1].outputs[0];
  f[1].outputs[0] = f[1].input;

  state->final_z[2] = state->final_z[1];
  state->final_z[1] = state->final_z[0];
  state->final_z[0] = f[0].gain * f[0].outputs[1]
                    + f[1].gain*f[1].outputs[1];
  return state->final_z[0] - state->final_z[2];
}


static inline gdouble
generic_tick (ShakerState *state)
{
  guint n_freqs = state->n_freqs;
  guint i;
  gdouble data;
  if (state->shake_energy <= MIN_ENERGY)
    return 0;
  state->shake_energy *= state->system_decay;    /* Exponential system decay */
  if ((guint)g_random_int_range (0, 1024) < state->n_objects)
    {
      state->sound_level += state->shake_energy;
      for (i = 0; i < n_freqs; i++)
        {
          Freq *f = state->freqs + i;
          if (f->freqalloc)
            {
              double r = g_random_double_range (-f->freq_rand, f->freq_rand);
              double temp_rand = f->t_center_freq * (1.0 + r);
              f->coeffs[0] = -f->reson * 2.0
                           * cos (temp_rand * BB_2PI / state->sampling_rate);
            }
        }
    }

  /* Actual Sound is Random */
  state->freqs[0].input = state->sound_level * g_random_double_range(-1,1);

  for (i = 1; i < n_freqs; i++)
    state->freqs[i].input = state->freqs[0].input;
  state->sound_level *= state->sound_decay;	// Exponential Sound decay 
  state->final_z[2] = state->final_z[1];
  state->final_z[1] = state->final_z[0];
  state->final_z[0] = 0;
  for (i = 0; i < n_freqs; i++)
    {
      /* Do resonant filter calculations */
      Freq *f = state->freqs + i;
      f->input -= f->outputs[0] * f->coeffs[0];
      f->input -= f->outputs[1] * f->coeffs[1];
      f->outputs[1] = f->outputs[0];
      f->outputs[0] = f->input;
      state->final_z[0] += f->gain * f->outputs[1];
    }
  /* Extra zero(s) for shape */
  data = state->final_z_coeffs[0] * state->final_z[0]
       + state->final_z_coeffs[1] * state->final_z[1]
       + state->final_z_coeffs[2] * state->final_z[2];
  data *= 0.0001;
  return CLAMP (data, -1.0, 1.0);
}

static void
set_decay (ShakerState *state,
           gdouble      value)
{
  if (state->instr_type != BB_SHAKER_GUIRO
   && state->instr_type != BB_SHAKER_WRENCH)
    {
      guint i;
      state->system_decay = state->base_decay
                          + (value * state->decay_scale
                             * (1.0 - state->base_decay));
      for (i = 0; i < state->n_freqs; i++)
        state->freqs[i].gain = state->ngain;
      if (state->instr_type == BB_SHAKER_TAMBOURINE)
        {
          state->freqs[0].gain *= TAMBOURINE_SHELL_GAIN;
          state->freqs[1].gain *= 0.8;
        }
      else if (state->instr_type == BB_SHAKER_SLEIGH_BELLS)
        {
          state->freqs[3].gain *= 0.5;
          state->freqs[4].gain *= 0.3;
        }
      else if (state->instr_type == BB_SHAKER_COKE_CAN)
        {
          for (i = 1; i<state->n_freqs;i++)
            state->freqs[i].gain *= 1.8;
        }
      for (i = 0; i < state->n_freqs; i++)
        state->freqs[i].gain *= ((128.0 - 64.0 * value)/100.0 + 0.36);
    }
}

static void
set_count_normalized (ShakerState *state,
                      gdouble      value)       /* range of 0..2 */
{
  guint i;
  if (state->instr_type == BB_SHAKER_BAMBOO_CHIMES) // bamboo
    state->n_objects = value * state->base_n_objects + 0.3;
  else
    state->n_objects = value * state->base_n_objects + 1.1;
  state->ngain = log(state->n_objects) * state->base_gain / state->n_objects;
  for (i = 0; i < state->n_freqs; i++)
    state->freqs[i].gain = state->ngain;
  switch (state->instr_type)
    {
    case BB_SHAKER_TAMBOURINE:
      state->freqs[0].gain *= TAMBOURINE_SHELL_GAIN;
      state->freqs[1].gain *= 0.8; 
      break;
    case BB_SHAKER_SLEIGH_BELLS:
      state->freqs[3].gain *= 0.5;
      state->freqs[4].gain *= 0.3;
      break;
    case BB_SHAKER_COKE_CAN:
      for (i = 1; i < state->n_freqs; i++)
        state->freqs[i].gain *= 1.8;
      break;
    default:
      break;
    }
  if (state->instr_type != BB_SHAKER_GUIRO
   && state->instr_type != BB_SHAKER_WRENCH)
    {
      // reverse calculate decay setting
      double temp = (double) (64.0 * (state->system_decay - state->base_decay)
                           / (state->decay_scale * (1.0 - state->base_decay)) + 64.0);
      // scale gains by decay setting
      for (i = 0; i < state->n_freqs; i++)
        state->freqs[i].gain *= ((128-temp)/100.0 + 0.36);
    }
}

static inline void
set_freq_and_reson (ShakerState *state,
                    guint        index,
                    gdouble      freq,
                    gdouble      reson)
{
  Freq *f = state->freqs + index;
  f->reson = reson;
  f->center_freq = freq;
  f->t_center_freq = freq;
  f->coeffs[1] = reson * reson;
  f->coeffs[0] = -reson * 2.0 * cos(freq * BB_2PI / state->sampling_rate);
}

static void
set_instrument (ShakerState *state,
                BbShakerInstrumentType instr_type)
{
  guint i;
  InstrInfo *ii = all_instr_infos[instr_type];
  g_assert (ii->instr_type == instr_type);

  state->instr_type = instr_type;
  state->n_objects = ii->n_objects;
  state->base_n_objects = ii->n_objects;
  state->system_decay = ii->system_decay;
  state->sound_decay = ii->sound_decay;
  state->base_decay = ii->default_decay;
  state->base_gain = ii->base_gain;
  state->decay_scale = ii->decay_scale;
  state->ngain = log(state->n_objects) * state->base_gain / state->n_objects;

  state->n_freqs = ii->n_freqs;
  for (i = 0; i < ii->n_freqs; i++)
    {
      state->freqs[i].gain = state->ngain * ii->freqs[i].gain;
      state->freqs[i].freqalloc = ii->freqs[i].freqalloc;
      state->freqs[i].freq_rand = ii->freqs[i].freq_rand;
      set_freq_and_reson (state, i,
                          ii->freqs[i].freq,
                          ii->freqs[i].reson);
    }
  for (i = 0; i < 3; i++)
    state->final_z_coeffs[i] = ii->final_z[i];

  switch (instr_type)
    {
    case BB_SHAKER_CABASA:
      state->freqs[0].outputs[0] = state->ngain;
      break;

    case BB_SHAKER_GUIRO:
    case BB_SHAKER_WRENCH:
      state->ratchet = 0;
      state->ratchet_pos = 10;
      break;

    default:
      break;
    }
}

static inline void
shaker_state_init (ShakerState *state,
                   double       sampling_rate)
{
  int i;

  state->instr_type = 0;
  state->shake_energy = 0.0;
  state->n_freqs = 0;
  state->sound_level = 0.0;

  for (i = 0; i < MAX_FREQS; i++)
    {
      Freq *f = state->freqs + i;
      f->input = 0.0;
      f->outputs[0] = 0.0;
      f->outputs[1] = 0.0;
      f->coeffs[0] = 0.0;
      f->coeffs[1] = 0.0;
      f->gain = 0.0;
      f->center_freq = 0.0;
      f->reson =  0.0;
      f->freq_rand = 0.0;
      f->freqalloc = 0;
    }

  state->sound_decay = 0.0;
  state->system_decay = 0.0;
  state->n_objects = 0.0;
  state->total_energy = 0.0;
  state->ratchet = 0.0;
  state->ratchet_delta = 0.0005;
  state->last_ratchet_pos = 0;
  state->final_z[0] = 0.0;
  state->final_z[1] = 0.0;
  state->final_z[2] = 0.0;
  state->final_z_coeffs[0] = 1.0;
  state->final_z_coeffs[1] = 0.0;
  state->final_z_coeffs[2] = 0.0;

  state->base_n_objects = 0;
  state->sampling_rate = sampling_rate;
  state->tuned_bamb_which = 0;

  //this->setupNum(instType);
}

static double *
shaker_complex_synth  (BbInstrument *instrument,
                       BbRenderInfo *render_info,
                       double        duration,
                       GValue       *init_params,
                       guint         n_events,
                       const BbInstrumentEvent *events,
                       guint        *n_samples_out)
{
  double rate = render_info->sampling_rate;
  guint n = render_info->note_duration_samples;
  double *rv = g_new (double, n);
  guint sample_at = 0;
  guint event_at = 0;
  ShakerState state;

  /* initialize state */
  shaker_state_init (&state, render_info->sampling_rate);
  set_instrument (&state, g_value_get_enum (init_params + PARAM_INSTRUMENT));
  set_decay (&state, g_value_get_double (init_params + PARAM_DECAY));
  set_count_normalized (&state, g_value_get_double (init_params + PARAM_COUNT));
  /* XXX: other params??? */

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
                case PARAM_DECAY:
                  set_decay (&state, g_value_get_double (v));
                  break;

                case PARAM_COUNT:
                  set_count_normalized (&state, g_value_get_double (v));
                  break;

                case PARAM_RESONANCE_FREQUENCY:
                  {
                    gdouble base;
                    gdouble mod;
                    guint i;
                    if (state.instr_type == BB_SHAKER_SEKERE
                     || state.instr_type == BB_SHAKER_TAMBOURINE
                     || state.instr_type == BB_SHAKER_SLEIGH_BELLS)
                      base = 1.66522934309515;          /* 1.008^64 */
                    else
                      base = 2.59314441608327;          /* 1.015^64 */
                    mod = pow (base, g_value_get_double (v));
                    for (i = 0; i < state.n_freqs; i++)
                      {
                        Freq *f = state.freqs + i;
                        f->t_center_freq = mod * f->center_freq;
                        f->coeffs[0] = -f->reson * 2.0
                                     * cos (f->t_center_freq * BB_2PI / rate);
                        f->coeffs[1] = f->reson * f->reson;
                      }
                    break;
                  }

                case PARAM_INSTRUMENT:
                  set_instrument (&state, g_value_get_enum (v));
                  break;

                default:
                  g_assert_not_reached ();
                }
            }
          else if (events[event_at].type == BB_INSTRUMENT_EVENT_ACTION)
            {
              switch (events[event_at].info.action.index)
                {
                case ACTION_SHAKE:
                  {
                    gdouble amount = g_value_get_double (events[event_at].info.action.args + 0);
                    state.shake_energy += amount * MAX_SHAKE * 0.1;
                    if (state.shake_energy > MAX_SHAKE)
                      state.shake_energy = MAX_SHAKE;
                    if (state.instr_type == BB_SHAKER_WRENCH
                     || state.instr_type == BB_SHAKER_GUIRO)
                      {
                        state.ratchet_pos = (int) (128.0 * fabs(amount - state.last_ratchet_pos));
                        state.ratchet_delta = 0.0002 * state.ratchet_pos;
                        state.last_ratchet_pos = (int)(128.0 * amount);
                      }
                    break;
                  }

                default:
                  g_assert_not_reached ();
                }
            }
          else
            {
              g_assert_not_reached ();
            }
          event_at++;
        }
      
      /* generate audio (using the appropriate instrument) */
      switch (state.instr_type)
        {
        case BB_SHAKER_WATER_DROPS:
          while (sample_at < end_sample)
            {
              if (state.shake_energy > MIN_ENERGY)
                rv[sample_at++] = water_drops_tick (&state) * 0.0001;
              else
                rv[sample_at++] = 0;
            }
          break;
        case BB_SHAKER_TUNED_BAMBOO_CHIMES:
          while (sample_at < end_sample)
            rv[sample_at++] = tuned_bamboo_tick (&state);
          break;

        case BB_SHAKER_WRENCH:
        case BB_SHAKER_GUIRO:
          while (sample_at < end_sample)
            {
              if (state.ratchet_pos > 0)
                {
                  state.ratchet -= (state.ratchet_delta + 0.002 * state.total_energy);
                  if (state.ratchet < 0.0)
                    {
                      state.ratchet = 1.0;
                      state.ratchet_pos -= 1;
                    }
                }
              state.total_energy = state.ratchet;
              rv[sample_at++] = ratchet_tick (&state) * 0.0001;
            }
          break;

        default:
          while (sample_at < end_sample)
            rv[sample_at++] = generic_tick (&state);
          break;
        }
    }
  *n_samples_out = n;
  return rv;
}



BB_INIT_DEFINE(_bb_shaker_init)
{
  BbInstrument *instrument;
  instrument = bb_instrument_new_from_simple_param (G_N_ELEMENTS (shaker_params),
                                                    shaker_params);
  bb_instrument_set_name (instrument, "shaker");
  instrument->complex_synth_func = shaker_complex_synth;
  bb_instrument_add_action (instrument,
                            ACTION_SHAKE,
                            "shake",
                            g_param_spec_double ("amount", "Amount", NULL, 0, 1, 1, 0),
                            NULL);
  bb_instrument_add_param (instrument,
                           PARAM_INSTRUMENT,
                           g_param_spec_enum ("instrument", "Instrument", NULL,
                                              BB_TYPE_SHAKER_INSTRUMENT_TYPE,
                                              BB_SHAKER_MARACA, 0));
}
