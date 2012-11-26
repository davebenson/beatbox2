#ifndef __BB_BOW_TABLE_H_
#define __BB_BOW_TABLE_H_

typedef struct _BbBowTable BbBowTable;
struct _BbBowTable
{
  double offset;
  double slope;
};

G_INLINE_FUNC void   bb_bow_table_init      (BbBowTable *bow_table);
G_INLINE_FUNC void   bb_bow_table_set_slope (BbBowTable *bow_table,
                                             double      slope);
G_INLINE_FUNC void   bb_bow_table_set_offset(BbBowTable *bow_table,
                                             double      offset);
G_INLINE_FUNC double bb_bow_table_tick      (BbBowTable *bow_table,
                                             double      input);
G_INLINE_FUNC void   bb_bow_table_clear     (BbBowTable *bow_table);



#if defined (G_CAN_INLINE) || defined (__BB_DEFINE_INLINES__)
G_INLINE_FUNC void bb_bow_table_init      (BbBowTable *bow_table)
{
  bow_table->offset = 0.0;
  bow_table->slope = 0.1;
}

G_INLINE_FUNC void bb_bow_table_set_slope (BbBowTable *bow_table,
                                           double      slope)
{
  bow_table->slope = slope;
}

G_INLINE_FUNC void bb_bow_table_set_offset(BbBowTable *bow_table,
                                           double      offset)
{
  bow_table->offset = offset;
}
G_INLINE_FUNC double bb_bow_table_tick      (BbBowTable *bow_table,
                                             double      input)
{
  gdouble sample = (input + bow_table->offset) * bow_table->slope;
  gdouble last_out = ABS (sample) + 0.75;

  /* last_out = pow(last_out,-4) */
  if (0.0 <= last_out && last_out <= 1.0)
    return 1.0;
  last_out *= last_out;
  return 1.0 / (last_out * last_out);
}

G_INLINE_FUNC void bb_bow_table_clear     (BbBowTable *bow_table)
{
}
#endif

#endif
