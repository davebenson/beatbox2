#ifndef __BB_FUNCTION_H_
#define __BB_FUNCTION_H_

typedef struct _BbFunctionSignature BbFunctionSignature;
typedef struct _BbFunction BbFunction;

#include <glib-object.h>
#include "bb-render-info.h"

struct _BbFunctionSignature
{
  guint ref_count;
  guint n_inputs;
  GType *inputs;
  guint n_outputs;
  GType *outputs;
};

BbFunctionSignature *bb_function_signature_new_v (guint        n_inputs,
                                                  const GType *inputs,
                                                  guint        n_outputs,
                                                  const GType *outputs);
BbFunctionSignature *bb_function_signature_new   (GType        first_input_type,
                                                  ...);
BbFunctionSignature *bb_function_signature_ref   (BbFunctionSignature *);
void                 bb_function_signature_unref (BbFunctionSignature *);

typedef enum
{
  BB_FUNCTION_TYPE_GENERAL,
  BB_FUNCTION_TYPE_SINGLE_OUTPUT
} BbFunctionType;

typedef gboolean (*BbFunctionGeneralFunc) (guint         n_inputs,
                                           const GValue *inputs,
                                           guint        *n_outputs_out,
                                           GValue      **outputs_out,
                                           const BbRenderInfo *render_info,
                                           GError      **error);

/* note: unless you g_value_init(output),
   there will be no output from this function invocation */
typedef gboolean (*BbFunctionSingleFunc)  (guint         n_inputs,
                                           const GValue *inputs,
                                           GValue       *output,
                                           const BbRenderInfo *render_info,
                                           GError      **error);

struct _BbFunction
{
  BbFunctionSignature *sig;
  BbFunctionType type;
  guint ref_count;
  char *name;
  union
  {
    BbFunctionGeneralFunc general;
    BbFunctionSingleFunc single_output;
  } func;
};

BbFunction *bb_function_new_single_output (const char          *name,
                                           BbFunctionSignature *sig, /* mandatory */
                                           BbFunctionSingleFunc func);
BbFunction *bb_function_new_general       (const char          *name,
                                           BbFunctionSignature *sig, /* optional */
                                           BbFunctionGeneralFunc func);
BbFunction *bb_function_ref               (BbFunction          *function);
void        bb_function_unref             (BbFunction          *function);

BbFunction *bb_function_choose            (const char          *name,
                                           guint                n_inputs,
                                           const GType         *inputs_types);

#endif
