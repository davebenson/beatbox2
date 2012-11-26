
typedef struct _BbPerformInterpreter BbPerformInterpreter;
struct _BbPerformInterpreter
{
  char *name;
  char **args;
};

extern BbPerformInterpreter *bb_perform_interpreters;
extern guint                 bb_perform_interpreters_count;

BbPerformInterpreter *bb_perform_interpreter_find_by_name (const char *name);

GPid   bb_perform_interpreter_launch   (BbPerformInterpreter *interpreter,
                                        const char           *input_name,
                                        const char           *output_name,
                                        gdouble               sampling_rate);

