
typedef struct _BbParseContext BbParseContext;
struct _BbParseContext
{
  GPtrArray *pspec_array;
  GError *error;
  BbProgram *program;
  guint n_source_pspecs;
  GParamSpec **source_pspecs;
};
