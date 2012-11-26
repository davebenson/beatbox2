#include <stdlib.h>
#include <ctype.h>
#include <gsk/gskutils.h>
#include <gsk/gskmacros.h>
#include <gsk/gskghelpers.h>
#include "bb-p.h"
#include "bb-vm.h"
#include "bb-error.h"
#include "bb-parse-context.h"


/* from lemon, in bb-p.c */
void *BbParseAlloc(void *(*mallocProc)(size_t));
void BbParse(
  void *yyp,                   /* The parser */
  int yymajor,                 /* The major token code number */
  char *yyminor,
  BbParseContext *context
);
void BbParseFree(
  void *p,                    /* The parser to be deleted */
  void (*freeProc)(void*)     /* Function used to reclaim memory */
);

BbProgram *
bb_program_parse (const char *expr_str,
                  GError    **error)
{
  static GPtrArray *reusable = NULL;
  BbProgram *rv;
  if (reusable == NULL)
    reusable = g_ptr_array_new ();
  rv = bb_program_parse_create_params (expr_str, reusable, 0, NULL, error);
  if (rv == NULL)
    return NULL;
  if (reusable->len > 0)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                   "parsing of expression caused param %s to be created (n-created=%u)",
                   ((GParamSpec*)reusable->pdata[0])->name,
                   reusable->len);
      g_ptr_array_foreach (reusable, (GFunc) g_param_spec_sink, NULL);
      g_ptr_array_set_size (reusable, 0);
      bb_program_unref (rv);
      return NULL;
    }
  return rv;
}

BbProgram *
bb_program_parse_create_params (const char   *expr_str,
                                GPtrArray    *pspecs_inout,
                                guint         n_source_pspecs,
                                GParamSpec  **source_pspecs,
                                GError      **error)
{
  BbParseContext context;
  const char *at;
  gpointer parser;
  context.pspec_array = pspecs_inout;
  context.error = NULL;
  context.program = NULL;
  context.n_source_pspecs = n_source_pspecs;
  context.source_pspecs = source_pspecs;
  parser = BbParseAlloc (malloc);
  at = expr_str;
  while (context.error == NULL && *at != 0)
    {
      GSK_SKIP_WHITESPACE (at);
      if (*at == 0)
        break;
      if (g_ascii_isdigit (*at)
        || *at == '.'
        || (*at == '-' && (g_ascii_isdigit (at[1]) || at[1] == '.')))
        {
          const char *start = at;

          /* maybe_skip_s is set to TRUE if we
             encountered a token that we permmit
             to be followed with a trailing "s",
             which occasionally more readable,eg:
               {duration=10beats}
             versus
               {duration=10beat}
             (both are accepted).
             we don't accept:
               {duration=10ss}  ("s", for second, cannot be made plural)
               {duration=5%s}  (obviously not conventional)
            */
          gboolean maybe_skip_s = FALSE;

          if (*at == '-')
            at++;
          while (*at &&
                 (g_ascii_isdigit (*at) || *at == 'e' || *at == 'E'
                  || *at == '.'))
            at++;
          if (g_str_has_prefix (at, "samp"))
            {
              at += 4;
              maybe_skip_s = TRUE;
            }
          else if (g_str_has_prefix (at, "sec"))
            {
              at += 3;
              maybe_skip_s = TRUE;
            }
          else if (g_str_has_prefix (at, "s") && !g_ascii_isalnum (at[1]))
            {
              at += 1;
            }
          else if (g_str_has_prefix (at, "b") && !g_ascii_isalnum (at[1]))
            {
              at += 1;
            }
          else if (g_str_has_prefix (at, "beat"))
            {
              at += 4;
              maybe_skip_s = TRUE;
            }
          else if (*at == '%')
            {
              at++;
            }
          BbParse (parser, BB_TOKEN_TYPE_NUMBER,
                   g_strndup (start, at - start), &context);
          if (maybe_skip_s && *at == 's')
            at++;
        }
      else if (g_ascii_isalpha (*at))
        {
          /* bareword */
          const char *start = at++;
          while (*at && (*at == '_' || g_ascii_isalnum (*at)))
            at++;
          BbParse (parser, BB_TOKEN_TYPE_BAREWORD,
                   g_strndup (start, at - start), &context);
        }
      else
        {
          switch (*at)
            {
#define WRITE_CASE(ch, str, shortname)                          \
            case ch:                                            \
              BbParse (parser, BB_TOKEN_TYPE_##shortname,       \
                       g_strdup (str), &context);               \
              at++;                                             \
              break
            WRITE_CASE ('+', "+", PLUS);
            WRITE_CASE ('-', "-", MINUS);
            WRITE_CASE ('*', "*", TIMES);
            WRITE_CASE ('/', "/", DIVIDE);
            WRITE_CASE ('(', "(", LPAREN);
            WRITE_CASE (')', ")", RPAREN);
            WRITE_CASE (',', ",", COMMA);
            WRITE_CASE ('[', "[", LBRACKET);
            WRITE_CASE (']', "]", RBRACKET);
#undef WRITE_CASE
            case '"':
              {
                const char *end;
                guint len;
                char *dequoted = gsk_unescape_memory (at, TRUE, &end, &len, error);
                if (dequoted == NULL)
                  {
                    gsk_g_error_add_prefix (error, "error dequoting string");
                    return FALSE;
                  }
                /* note: takes ownership of 'dequoted' */
                BbParse (parser, BB_TOKEN_TYPE_STRING_LITERAL, dequoted, &context);
                g_assert (*end != '"');
                at = end;
                break;
              }

            case ':':
              if (at[1] == ':')
                {
                  at += 2;
                  BbParse (parser, BB_TOKEN_TYPE_COLONCOLON, g_strdup ("::"), &context);
                  break;
                }
              /* fallthrough, since a single ':' is invalid */
    
            default:
              g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                           "unexpected character '%c' in expression", *at);
              return FALSE;
            }
        }
    }
  BbParse (parser, 0, NULL, &context);
  BbParseFree (parser, free);
  if (context.error != NULL)
    {
      g_propagate_error (error, context.error);
      g_ptr_array_foreach (context.pspec_array, (GFunc) g_param_spec_sink, NULL);
      g_ptr_array_free (context.pspec_array, TRUE);
      return NULL;
    }
  if (context.program == NULL)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_PARSE,
                   "incomplete expression (%s)",expr_str);
      return NULL;
    }
  return context.program;
}

const char  * bb_program_string_scan   (const char            *start)
{
  guint abalance = 0, pbalance = 0, bkbalance = 0, bcbalance = 0;
  GSK_SKIP_WHITESPACE (start);
  while (*start)
    {
      switch (*start)
        {
        case '<': abalance++; break;
        case '>': if (abalance == 0) goto maybe_done; 
                  abalance--; break;
        case '(': pbalance++; break;
        case ')': if (pbalance == 0) goto maybe_done; 
                  pbalance--; break;
        case '[': bkbalance++; break;
        case ']': if (bkbalance == 0) goto maybe_done;
                  bkbalance--; break;
        case '{': bcbalance++; break;
        case '}': if (bcbalance == 0) goto maybe_done;
                  bcbalance--; break;
        case ',': if (abalance==0 && pbalance==0 && bkbalance==0 && bcbalance==0) goto maybe_done;
                  break;
        }
      start++;
    }
maybe_done:
  if (abalance || pbalance || bkbalance || bcbalance)
    g_error ("bb_program_string_scan: mismatched paren etc at '%c' (angle,paren,brace,bracket counts=%u,%u,%u,%u)",
             *start,abalance,pbalance,bcbalance,bkbalance);
  return start;
}
