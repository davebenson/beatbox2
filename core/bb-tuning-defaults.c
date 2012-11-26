#include "bb-tuning.h"

#define DEFAULT_NOTE_FAMILY(let, base) \
  { let, base }, { let "-", base-1 }, { let "+", base+1 }


static BbTuningNote default_notes[] =
{
  DEFAULT_NOTE_FAMILY("a", 57),
  DEFAULT_NOTE_FAMILY("b", 59),
  DEFAULT_NOTE_FAMILY("c", 60),
  DEFAULT_NOTE_FAMILY("d", 62),
  DEFAULT_NOTE_FAMILY("e", 64),
  DEFAULT_NOTE_FAMILY("f", 65),
  DEFAULT_NOTE_FAMILY("g", 67),
};

static BbTuningNote c_default_notes[] =
{
  DEFAULT_NOTE_FAMILY("a", 69),
  DEFAULT_NOTE_FAMILY("b", 71),
  DEFAULT_NOTE_FAMILY("c", 60),
  DEFAULT_NOTE_FAMILY("d", 62),
  DEFAULT_NOTE_FAMILY("e", 64),
  DEFAULT_NOTE_FAMILY("f", 65),
  DEFAULT_NOTE_FAMILY("g", 67),
};

static gdouble default_equal_tempered_scale[] =
{
  440.000000,
  466.163762,
  493.883301,
  523.251131,
  554.365262,
  587.329536,
  622.253967,
  659.255114,
  698.456463,
  739.988845,
  783.990872,
  830.609395
};

static BbTuning default_tuning =
{
  "default",
  G_N_ELEMENTS (default_notes),
  default_notes,
  57,
  12,
  default_equal_tempered_scale,
  2.0
};

#define MIDDLE_C_FREQ 523.251131

static gdouble equal_tempered_scale[] =
{
  523.251131,                   /* note 60==middle c */
  554.365262,
  587.329536,
  622.253967,
  659.255114,
  698.456463,
  739.988845,
  783.990872,
  830.609395,
  440.000000 * 2.0,
  466.163762 * 2.0,
  493.883301 * 2.0,
};

static BbTuning equal_temperament_tuning =
{
  "equal",
  G_N_ELEMENTS (c_default_notes),
  c_default_notes,
  60,
  12,
  equal_tempered_scale,
  2.0
};



#define JUST_SEMITONE           (16.0/15.0)
static gdouble just_tuning_scale[] =
{
  MIDDLE_C_FREQ * 1.0,                  /* C */
  MIDDLE_C_FREQ * JUST_SEMITONE,
  MIDDLE_C_FREQ * 9.0 / 8.0,            /* D */
  MIDDLE_C_FREQ * 9.0 / 8.0 * JUST_SEMITONE,
  MIDDLE_C_FREQ * 5.0 / 4.0,            /* E */
  MIDDLE_C_FREQ * 4.0 / 3.0,            /* F */
  MIDDLE_C_FREQ * 4.0 / 3.0 * JUST_SEMITONE,
  MIDDLE_C_FREQ * 3.0 / 2.0,            /* G */
  MIDDLE_C_FREQ * 3.0 / 2.0 * JUST_SEMITONE,
  MIDDLE_C_FREQ * 5.0 / 3.0,            /* A */
  MIDDLE_C_FREQ * 5.0 / 3.0 * JUST_SEMITONE,
  MIDDLE_C_FREQ * 15.0 / 8.0,           /* B */
};

static BbTuning just_tuning =
{
  "just",
  G_N_ELEMENTS (c_default_notes),
  c_default_notes,
  60,
  12,
  just_tuning_scale,
  2.0
};

void _bb_tuning_register_defaults (void)
{
  bb_tuning_register (&default_tuning);
  bb_tuning_register (&equal_temperament_tuning);
  bb_tuning_register (&just_tuning);
}
