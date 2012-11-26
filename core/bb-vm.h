#ifndef __BB_VM_H_
#define __BB_VM_H_

typedef struct _BbInstruction BbInstruction;
typedef struct _BbProgram BbProgram;
typedef struct _BbVm BbVm;
typedef struct _BbVmStack BbVmStack;

#include <glib-object.h>
#include "bb-render-info.h"
#include "bb-function.h"

typedef enum
{
  BB_INSTRUCTION_PUSH,
  BB_INSTRUCTION_PUSH_PARAM,
  BB_INSTRUCTION_PUSH_SPECIAL,
  BB_INSTRUCTION_POP,
  BB_INSTRUCTION_BUILTIN,
  BB_INSTRUCTION_FUNCTION_ARG_START,
  BB_INSTRUCTION_FUNCTION,
  BB_INSTRUCTION_LAZY_FUNCTION
} BbInstructionType;

/* stack specials */
typedef enum
{
  BB_VM_SPECIAL_ARRAY_BEGIN
} BbVmSpecial;

typedef gboolean (*BbBuiltinFunc)   (BbVm *vm,
                                     const BbRenderInfo *render_info,
                                     GError **error);
struct _BbInstruction
{
  BbInstructionType type;
  union {
    GValue push;
    guint push_param;
    BbVmSpecial push_special;
    struct {
      BbBuiltinFunc func;
      const char *name;
    } builtin;
    BbFunction *function;
    char *lazy_function;
   } info;
};
void bb_instruction_dump (BbInstruction *insn,
                          GParamSpec   **pspecs_optional);

struct _BbProgram
{
  guint ref_count;
  guint n_instructions;
  BbInstruction *instructions;
  guint instructions_alloced;
};
BbProgram *bb_program_new   (void);
BbProgram *bb_program_ref   (BbProgram *);
void       bb_program_unref (BbProgram *);
void       bb_program_add_push_param    (BbProgram    *program,
                                         guint         param_index);
void       bb_program_add_pop           (BbProgram    *program);
void       bb_program_add_push          (BbProgram    *program,
                                         const GValue *value);
void       bb_program_add_push_special  (BbProgram    *program,
                                         BbVmSpecial   special);
void       bb_program_add_builtin       (BbProgram    *program,
                                         const char   *name,    /* statically allocated */
                                         BbBuiltinFunc func);
void       bb_program_add_function_begin(BbProgram    *program);
void       bb_program_add_function      (BbProgram    *program,
                                         BbFunction   *function);
void       bb_program_add_lazy_function (BbProgram    *program,
                                         const char   *function_name);
void       bb_program_append_program    (BbProgram    *program,
                                         BbProgram    *subprogram);

BbProgram * bb_program_parse               (const char   *expr_str,
                                            GError      **error);
BbProgram * bb_program_parse_create_params (const char   *expr_str,
                                            GPtrArray    *pspecs_inout,
                                            guint         n_source_pspecs,
                                            GParamSpec  **source_pspecs,
                                            GError      **error);

const char  * bb_program_string_scan   (const char            *start);

struct _BbVmStack
{
  BbVmStack *up;
  guint n_values;
  GValue *values;
  guint values_alloced;
};


struct _BbVm
{
  guint n_values;
  GValue *values;
  guint values_alloced;
  BbVmStack *stack;

  guint n_params;
  const GValue *params;
  gboolean free_params;

  guint recursion_depth;
  gboolean trace;
};
BbVm    *bb_vm_new           (guint         n_params,
                              const GValue *params,
                              gboolean      copy_params);
void     bb_vm_push          (BbVm         *vm,
                              const GValue *value);
void     bb_vm_pop           (BbVm         *vm,
                              GValue       *value);
gboolean bb_vm_run           (BbVm         *vm,
                              BbProgram    *program,
                              const BbRenderInfo *render_info,
                              GError      **error);
void     bb_vm_push_special  (BbVm         *vm,
                              BbVmSpecial   special);
gboolean bb_vm_is_special    (BbVm         *vm,
                              guint         depth,
                              BbVmSpecial  *out);
void     bb_vm_pop_special   (BbVm         *vm,
                              BbVmSpecial  *out);

/* for function call implementation */
void     bb_vm_push_stack    (BbVm         *vm);
void     bb_vm_pop_stack     (BbVm         *vm);

void     bb_vm_free          (BbVm         *vm);

void   bb_vm_run_get_value_or_die  (BbProgram    *program,
                                    const BbRenderInfo *render_info,
                                    guint         n_params,
                                    const GValue *params,
                                    GValue       *value);
double bb_vm_run_get_double_or_die (BbProgram    *program,
                                    const BbRenderInfo *render_info,
                                    guint         n_params,
                                    const GValue *params);
gboolean bb_vm_parse_run_string_get_value (const char *str,
                                           const BbRenderInfo *render_info,
                                           GValue     *out,
                                           GError    **error);
/* --- builtins --- */
void          bb_builtin_register (const char   *function_name,
                                   BbBuiltinFunc func);
BbBuiltinFunc bb_builtin_lookup   (const char   *function_name);

gboolean bb_builtin_create_array (BbVm *, const BbRenderInfo *, GError **);


#endif
