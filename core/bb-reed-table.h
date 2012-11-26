#ifndef __BB_REED_TABLE_H_
#define __BB_REED_TABLE_H_

/* mercilessly ripped off from the excellent STK library */

typedef struct _BbReedTable BbReedTable;

G_INLINE_FUNC BbReedTable *bb_reed_table_new        (void);
G_INLINE_FUNC void         bb_reed_table_set_offset (BbReedTable *reed_table,
                                                     double       offset);
G_INLINE_FUNC void         bb_reed_table_set_slope  (BbReedTable *reed_table,
                                                     double       slope);
G_INLINE_FUNC gdouble      bb_reed_table_tick       (BbReedTable *reed_table,
                                                     gdouble      pressure_diff);
G_INLINE_FUNC void         bb_reed_table_free       (BbReedTable *reed_table);


struct _BbReedTable
{
  gdouble offset;
  gdouble slope;
};


#if defined (G_CAN_INLINE) || defined (__BB_DEFINE_INLINES__)

G_INLINE_FUNC BbReedTable *
bb_reed_table_new        (void)
{
  BbReedTable *rv = g_new (BbReedTable, 1);
  rv->offset = 0.6;
  rv->slope = -0.8;
  return rv;
}

G_INLINE_FUNC void
bb_reed_table_set_offset (BbReedTable *reed_table,
                          double       offset)
{
  reed_table->offset = offset;
}

G_INLINE_FUNC void
bb_reed_table_set_slope  (BbReedTable *reed_table,
			  double       slope)
{
  reed_table->slope = slope;
}

G_INLINE_FUNC gdouble
bb_reed_table_tick       (BbReedTable *reed_table,
		          gdouble      input)
{
  double output = reed_table->offset + reed_table->slope * input;
  return CLAMP (output, -1.0, 1.0);
}

G_INLINE_FUNC void
bb_reed_table_free       (BbReedTable *reed_table)
{
  g_free (reed_table);
}

#endif

#endif
