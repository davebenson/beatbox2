#include "bb-types.h"


GType bb_value_array_get_type (void)
{
  static GType rv = 0;
  if (rv == 0)
    rv = g_boxed_type_register_static ("BbValueArray",
                                       (GBoxedCopyFunc) bb_value_array_ref,
                                       (GBoxedFreeFunc) bb_value_array_unref);
  return rv;
}

BbValueArray *bb_value_array_new_take (guint         n,
                                       GValue       *arr)
{
  BbValueArray *rv = g_new (BbValueArray, 1);
  rv->ref_count = 1;
  rv->n_values = n;
  rv->values = arr;
  return rv;
}

BbValueArray *bb_value_array_ref      (BbValueArray *array)
{
  g_assert (array->ref_count > 0);
  ++(array->ref_count);
  return array;
}

void          bb_value_array_unref    (BbValueArray *array)
{
  g_assert (array->ref_count > 0);
  if (--(array->ref_count) == 0)
    {
      guint i;
      for (i = 0; i < array->n_values; i++)
        g_value_unset (array->values + i);
      g_free (array->values);
      g_free (array);
    }
}

/* --- double-array --- */
GType bb_double_array_get_type(void)
{
  static GType type = 0;
  if (type == 0)
    type = g_boxed_type_register_static ("BbDoubleArray",
                                         (GBoxedCopyFunc) bb_double_array_copy,
                                         (GBoxedFreeFunc) bb_double_array_free);
  return type;
}


BbDoubleArray *bb_double_array_new (guint          n_values,
                                    const double  *values)
{
  BbDoubleArray *array = g_new (BbDoubleArray, 1);
  array->values = g_memdup (values, sizeof (gdouble) * n_values);
  array->n_values = n_values;
  return array;
}

void           bb_double_array_free(BbDoubleArray *array)
{
  g_free (array->values);
  g_free (array);
}

BbDoubleArray *bb_double_array_copy(const BbDoubleArray *array)
{
  return bb_double_array_new (array->n_values, array->values);
}


static GTypeInfo bb_value_type_info =
{
  0,            /* class_size */
  NULL,         /* base_init */
  NULL,         /* base_finalize */
  NULL,         /* class_init */
  NULL,         /* class_finalize */
  NULL,         /* class_data */
  0,            /* instance_size */
  0,            /* n_preallocs */
  NULL,         /* instance_init */
  NULL          /* value_table */
};

static GTypeFundamentalInfo bb_value_type_fundamental_info = 
{
  G_TYPE_FLAG_VALUE_ABSTRACT
};

GType
bb_value_get_type (void)
{
  static GType rv = 0;
  if (rv == 0)
    {
      rv = g_type_fundamental_next ();
      g_type_register_fundamental (rv, "BbValue", 
                                   &bb_value_type_info,
                                   &bb_value_type_fundamental_info,
                                   0);
    }
  return rv;
}
