#ifndef __BB_DISCRETE_DIST_H_
#define __BB_DISCRETE_DIST_H_
#include <stdio.h>
#include <glib.h>

struct _BbDiscreteDistElt
{
  gpointer data;
  double weight;
};

struct _BbDiscreteDist
{
  guint n_elts;
  BbDiscreteDistElt *elts;
  double *cumulative;
  double total_weight;
};

BbDiscreteDist     *bb_discrete_dist_new_parse (const char         *str,
                                                GError            **error)
gpointer            bb_discrete_dist_pick      (BbDiscreteDist     *dist);

#endif
