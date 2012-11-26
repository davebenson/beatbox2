
typedef enum
{
  BB_RANDOM_ENVELOPE_CONSTANT,
  BB_RANDOM_ENVELOPE_LINEAR,
  BB_RANDOM_ENVELOPE_LINE_SEGMENTS,
} BbRandomEnvelopeMode;

GType bb_random_envelope_mode_get_type (void) G_GNUC_CONST;
#define BB_TYPE_RANDOM_ENVELOPE_MODE    (bb_random_envelope_mode_get_type())

