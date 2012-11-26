/* Driver template for the LEMON parser generator.
** The author disclaims copyright to this source code.
*/
/* First off, code is include which follows the "include" declaration
** in the input file. */
#include <stdio.h>
#line 1 "core/bb-p.lemon"

#include <string.h>
#include <stdlib.h>
#include "bb-vm.h"
#include "bb-p.h"
#include "bb-utils.h"
#include "bb-duration.h"
#include "bb-parse-context.h"
#line 18 "core/bb-p.c"
/* Next is all token values, in a form suitable for use by makeheaders.
** This section will be null unless lemon is run with the -m switch.
*/
/* 
** These constants (all generated automatically by the parser generator)
** specify the various kinds of tokens (terminals) that the parser
** understands. 
**
** Each symbol here is a terminal symbol in the grammar.
*/
/* Make sure the INTERFACE macro is defined.
*/
#ifndef INTERFACE
# define INTERFACE 1
#endif
/* The next thing included is series of defines which control
** various aspects of the generated parser.
**    YYCODETYPE         is the data type used for storing terminal
**                       and nonterminal numbers.  "unsigned char" is
**                       used if there are fewer than 250 terminals
**                       and nonterminals.  "int" is used otherwise.
**    YYNOCODE           is a number of type YYCODETYPE which corresponds
**                       to no legal terminal or nonterminal number.  This
**                       number is used to fill in empty slots of the hash 
**                       table.
**    YYFALLBACK         If defined, this indicates that one or more tokens
**                       have fall-back values which should be used if the
**                       original value of the token will not parse.
**    YYACTIONTYPE       is the data type used for storing terminal
**                       and nonterminal numbers.  "unsigned char" is
**                       used if there are fewer than 250 rules and
**                       states combined.  "int" is used otherwise.
**    BbParseTOKENTYPE     is the data type used for minor tokens given 
**                       directly to the parser from the tokenizer.
**    YYMINORTYPE        is the data type used for all minor tokens.
**                       This is typically a union of many types, one of
**                       which is BbParseTOKENTYPE.  The entry in the union
**                       for base tokens is called "yy0".
**    YYSTACKDEPTH       is the maximum depth of the parser's stack.
**    BbParseARG_SDECL     A static variable declaration for the %extra_argument
**    BbParseARG_PDECL     A parameter declaration for the %extra_argument
**    BbParseARG_STORE     Code to store %extra_argument into yypParser
**    BbParseARG_FETCH     Code to extract %extra_argument from yypParser
**    YYNSTATE           the combined number of states.
**    YYNRULE            the number of rules in the grammar
**    YYERRORSYMBOL      is the code number of the error symbol.  If not
**                       defined, then do no error processing.
*/
#define YYCODETYPE unsigned char
#define YYNOCODE 18
#define YYACTIONTYPE unsigned char
#define BbParseTOKENTYPE char *
typedef union {
  BbParseTOKENTYPE yy0;
  BbProgram * yy5;
  int yy35;
} YYMINORTYPE;
#define YYSTACKDEPTH 100
#define BbParseARG_SDECL BbParseContext *context;
#define BbParseARG_PDECL ,BbParseContext *context
#define BbParseARG_FETCH BbParseContext *context = yypParser->context
#define BbParseARG_STORE yypParser->context = context
#define YYNSTATE 29
#define YYNRULE 15
#define YYERRORSYMBOL 14
#define YYERRSYMDT yy35
#define YY_NO_ACTION      (YYNSTATE+YYNRULE+2)
#define YY_ACCEPT_ACTION  (YYNSTATE+YYNRULE+1)
#define YY_ERROR_ACTION   (YYNSTATE+YYNRULE)

/* Next are that tables used to determine what action to take based on the
** current state and lookahead token.  These tables are used to implement
** functions that take a state number and lookahead value and return an
** action integer.  
**
** Suppose the action integer is N.  Then the action is determined as
** follows
**
**   0 <= N < YYNSTATE                  Shift N.  That is, push the lookahead
**                                      token onto the stack and goto state N.
**
**   YYNSTATE <= N < YYNSTATE+YYNRULE   Reduce by rule N-YYNSTATE.
**
**   N == YYNSTATE+YYNRULE              A syntax error has occurred.
**
**   N == YYNSTATE+YYNRULE+1            The parser accepts its input.
**
**   N == YYNSTATE+YYNRULE+2            No such action.  Denotes unused
**                                      slots in the yy_action[] table.
**
** The action table is constructed as a single large table named yy_action[].
** Given state S and lookahead X, the action is computed as
**
**      yy_action[ yy_shift_ofst[S] + X ]
**
** If the index value yy_shift_ofst[S]+X is out of range or if the value
** yy_lookahead[yy_shift_ofst[S]+X] is not equal to X or if yy_shift_ofst[S]
** is equal to YY_SHIFT_USE_DFLT, it means that the action is not in the table
** and that yy_default[S] should be used instead.  
**
** The formula above is for computing the action when the lookahead is
** a terminal symbol.  If the lookahead is a non-terminal (as occurs after
** a reduce action) then the yy_reduce_ofst[] array is used in place of
** the yy_shift_ofst[] array and YY_REDUCE_USE_DFLT is used in place of
** YY_SHIFT_USE_DFLT.
**
** The following are the tables generated in this section:
**
**  yy_action[]        A single table containing all actions.
**  yy_lookahead[]     A table containing the lookahead for each entry in
**                     yy_action.  Used to detect hash collisions.
**  yy_shift_ofst[]    For each state, the offset into yy_action for
**                     shifting terminals.
**  yy_reduce_ofst[]   For each state, the offset into yy_action for
**                     shifting non-terminals after a reduce.
**  yy_default[]       Default action for each state.
*/
static const YYACTIONTYPE yy_action[] = {
 /*     0 */    12,   45,    1,    3,   14,   22,   17,   18,   23,   12,
 /*    10 */    26,    4,    8,   14,    5,   17,   18,   23,    7,   26,
 /*    20 */    43,   10,    2,    6,    4,    8,   10,    2,    6,    4,
 /*    30 */     8,    9,   11,   16,   13,   10,    2,    6,    4,    8,
 /*    40 */    15,   28,   21,   20,   10,    2,    6,    4,    8,    2,
 /*    50 */     6,    4,    8,   24,   34,   25,   27,   19,
};
static const YYCODETYPE yy_lookahead[] = {
 /*     0 */     3,   15,   16,   16,    7,    8,    9,   10,   11,    3,
 /*    10 */    13,    4,    5,    7,   16,    9,   10,   11,   16,   13,
 /*    20 */     0,    1,    2,    3,    4,    5,    1,    2,    3,    4,
 /*    30 */     5,   16,   16,    8,   16,    1,    2,    3,    4,    5,
 /*    40 */    16,   10,    8,   16,    1,    2,    3,    4,    5,    2,
 /*    50 */     3,    4,    5,   16,   17,   12,    6,    7,
};
#define YY_SHIFT_USE_DFLT (-4)
static const signed char yy_shift_ofst[] = {
 /*     0 */     6,   20,    6,    7,    6,   -4,    6,    7,    6,   -4,
 /*    10 */     6,   47,    6,    7,    6,   25,   -4,   -4,   50,   -3,
 /*    20 */    34,   -4,   -4,    6,   43,   -4,   -4,   31,   -4,
};
#define YY_REDUCE_USE_DFLT (-15)
static const signed char yy_reduce_ofst[] = {
 /*     0 */   -14,  -15,  -13,  -15,   -2,  -15,    2,  -15,   15,  -15,
 /*    10 */    16,  -15,   18,  -15,   24,  -15,  -15,  -15,  -15,   27,
 /*    20 */   -15,  -15,  -15,   37,  -15,  -15,  -15,  -15,  -15,
};
static const YYACTIONTYPE yy_default[] = {
 /*     0 */    44,   44,   44,   29,   44,   30,   44,   31,   44,   33,
 /*    10 */    44,   35,   44,   32,   44,   44,   34,   36,   37,   44,
 /*    20 */    44,   38,   39,   44,   44,   41,   42,   44,   40,
};
#define YY_SZ_ACTTAB (sizeof(yy_action)/sizeof(yy_action[0]))

/* The next table maps tokens into fallback tokens.  If a construct
** like the following:
** 
**      %fallback ID X Y Z.
**
** appears in the grammer, then ID becomes a fallback token for X, Y,
** and Z.  Whenever one of the tokens X, Y, or Z is input to the parser
** but it does not parse, the type of the token is changed to ID and
** the parse is retried before an error is thrown.
*/
#ifdef YYFALLBACK
static const YYCODETYPE yyFallback[] = {
};
#endif /* YYFALLBACK */

/* The following structure represents a single element of the
** parser's stack.  Information stored includes:
**
**   +  The state number for the parser at this level of the stack.
**
**   +  The value of the token stored at this level of the stack.
**      (In other words, the "major" token.)
**
**   +  The semantic value stored at this level of the stack.  This is
**      the information used by the action routines in the grammar.
**      It is sometimes called the "minor" token.
*/
struct yyStackEntry {
  int stateno;       /* The state-number */
  int major;         /* The major token value.  This is the code
                     ** number for the token at this stack level */
  YYMINORTYPE minor; /* The user-supplied minor token value.  This
                     ** is the value of the token  */
};
typedef struct yyStackEntry yyStackEntry;

/* The state of the parser is completely contained in an instance of
** the following structure */
struct yyParser {
  int yyidx;                    /* Index of top element in stack */
  int yyerrcnt;                 /* Shifts left before out of the error */
  BbParseARG_SDECL                /* A place to hold %extra_argument */
  yyStackEntry yystack[YYSTACKDEPTH];  /* The parser's stack */
};
typedef struct yyParser yyParser;

#ifndef NDEBUG
#include <stdio.h>
static FILE *yyTraceFILE = 0;
static char *yyTracePrompt = 0;
#endif /* NDEBUG */

#ifndef NDEBUG
/* 
** Turn parser tracing on by giving a stream to which to write the trace
** and a prompt to preface each trace message.  Tracing is turned off
** by making either argument NULL 
**
** Inputs:
** <ul>
** <li> A FILE* to which trace output should be written.
**      If NULL, then tracing is turned off.
** <li> A prefix string written at the beginning of every
**      line of trace output.  If NULL, then tracing is
**      turned off.
** </ul>
**
** Outputs:
** None.
*/
void BbParseTrace(FILE *TraceFILE, char *zTracePrompt){
  yyTraceFILE = TraceFILE;
  yyTracePrompt = zTracePrompt;
  if( yyTraceFILE==0 ) yyTracePrompt = 0;
  else if( yyTracePrompt==0 ) yyTraceFILE = 0;
}
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing shifts, the names of all terminals and nonterminals
** are required.  The following table supplies these names */
static const char *const yyTokenName[] = { 
  "$",             "COMMA",         "PLUS",          "MINUS",       
  "TIMES",         "DIVIDE",        "COLONCOLON",    "LPAREN",      
  "RPAREN",        "NUMBER",        "BAREWORD",      "LBRACKET",    
  "RBRACKET",      "STRING_LITERAL",  "error",         "toplevel",    
  "expr",        
};
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing reduce actions, the names of all rules are required.
*/
static const char *const yyRuleName[] = {
 /*   0 */ "expr ::= expr PLUS expr",
 /*   1 */ "expr ::= expr TIMES expr",
 /*   2 */ "expr ::= expr MINUS expr",
 /*   3 */ "expr ::= MINUS expr",
 /*   4 */ "expr ::= expr DIVIDE expr",
 /*   5 */ "expr ::= LPAREN expr RPAREN",
 /*   6 */ "expr ::= expr COMMA expr",
 /*   7 */ "expr ::= NUMBER",
 /*   8 */ "expr ::= BAREWORD",
 /*   9 */ "expr ::= BAREWORD LPAREN expr RPAREN",
 /*  10 */ "expr ::= BAREWORD LPAREN RPAREN",
 /*  11 */ "expr ::= BAREWORD COLONCOLON BAREWORD",
 /*  12 */ "expr ::= LBRACKET expr RBRACKET",
 /*  13 */ "expr ::= STRING_LITERAL",
 /*  14 */ "toplevel ::= expr",
};
#endif /* NDEBUG */

/*
** This function returns the symbolic name associated with a token
** value.
*/
const char *BbParseTokenName(int tokenType){
#ifndef NDEBUG
  if( tokenType>0 && tokenType<(sizeof(yyTokenName)/sizeof(yyTokenName[0])) ){
    return yyTokenName[tokenType];
  }else{
    return "Unknown";
  }
#else
  return "";
#endif
}

/* 
** This function allocates a new parser.
** The only argument is a pointer to a function which works like
** malloc.
**
** Inputs:
** A pointer to the function used to allocate memory.
**
** Outputs:
** A pointer to a parser.  This pointer is used in subsequent calls
** to BbParse and BbParseFree.
*/
void *BbParseAlloc(void *(*mallocProc)(size_t)){
  yyParser *pParser;
  pParser = (yyParser*)(*mallocProc)( (size_t)sizeof(yyParser) );
  if( pParser ){
    pParser->yyidx = -1;
  }
  return pParser;
}

/* The following function deletes the value associated with a
** symbol.  The symbol can be either a terminal or nonterminal.
** "yymajor" is the symbol code, and "yypminor" is a pointer to
** the value.
*/
static void yy_destructor(YYCODETYPE yymajor, YYMINORTYPE *yypminor){
  switch( yymajor ){
    /* Here is inserted the actions which take place when a
    ** terminal or non-terminal is destroyed.  This can happen
    ** when the symbol is popped from the stack during a
    ** reduce or during error processing or when a parser is 
    ** being destroyed before it is finished parsing.
    **
    ** Note: during a reduce, the only symbols destroyed are those
    ** which appear on the RHS of the rule, but which are not used
    ** inside the C code.
    */
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
#line 25 "core/bb-p.lemon"
{ g_free((yypminor->yy0)); }
#line 352 "core/bb-p.c"
      break;
    case 16:
#line 21 "core/bb-p.lemon"
{ bb_program_unref((yypminor->yy5)); }
#line 357 "core/bb-p.c"
      break;
    default:  break;   /* If no destructor action specified: do nothing */
  }
}

/*
** Pop the parser's stack once.
**
** If there is a destructor routine associated with the token which
** is popped from the stack, then call it.
**
** Return the major token number for the symbol popped.
*/
static int yy_pop_parser_stack(yyParser *pParser){
  YYCODETYPE yymajor;
  yyStackEntry *yytos = &pParser->yystack[pParser->yyidx];

  if( pParser->yyidx<0 ) return 0;
#ifndef NDEBUG
  if( yyTraceFILE && pParser->yyidx>=0 ){
    fprintf(yyTraceFILE,"%sPopping %s\n",
      yyTracePrompt,
      yyTokenName[yytos->major]);
  }
#endif
  yymajor = yytos->major;
  yy_destructor( yymajor, &yytos->minor);
  pParser->yyidx--;
  return yymajor;
}

/* 
** Deallocate and destroy a parser.  Destructors are all called for
** all stack elements before shutting the parser down.
**
** Inputs:
** <ul>
** <li>  A pointer to the parser.  This should be a pointer
**       obtained from BbParseAlloc.
** <li>  A pointer to a function used to reclaim memory obtained
**       from malloc.
** </ul>
*/
void BbParseFree(
  void *p,                    /* The parser to be deleted */
  void (*freeProc)(void*)     /* Function used to reclaim memory */
){
  yyParser *pParser = (yyParser*)p;
  if( pParser==0 ) return;
  while( pParser->yyidx>=0 ) yy_pop_parser_stack(pParser);
  (*freeProc)((void*)pParser);
}

/*
** Find the appropriate action for a parser given the terminal
** look-ahead token iLookAhead.
**
** If the look-ahead token is YYNOCODE, then check to see if the action is
** independent of the look-ahead.  If it is, return the action, otherwise
** return YY_NO_ACTION.
*/
static int yy_find_shift_action(
  yyParser *pParser,        /* The parser */
  int iLookAhead            /* The look-ahead token */
){
  int i;
  int stateno = pParser->yystack[pParser->yyidx].stateno;
 
  /* if( pParser->yyidx<0 ) return YY_NO_ACTION;  */
  i = yy_shift_ofst[stateno];
  if( i==YY_SHIFT_USE_DFLT ){
    return yy_default[stateno];
  }
  if( iLookAhead==YYNOCODE ){
    return YY_NO_ACTION;
  }
  i += iLookAhead;
  if( i<0 || i>=YY_SZ_ACTTAB || yy_lookahead[i]!=iLookAhead ){
#ifdef YYFALLBACK
    int iFallback;            /* Fallback token */
    if( iLookAhead<sizeof(yyFallback)/sizeof(yyFallback[0])
           && (iFallback = yyFallback[iLookAhead])!=0 ){
#ifndef NDEBUG
      if( yyTraceFILE ){
        fprintf(yyTraceFILE, "%sFALLBACK %s => %s\n",
           yyTracePrompt, yyTokenName[iLookAhead], yyTokenName[iFallback]);
      }
#endif
      return yy_find_shift_action(pParser, iFallback);
    }
#endif
    return yy_default[stateno];
  }else{
    return yy_action[i];
  }
}

/*
** Find the appropriate action for a parser given the non-terminal
** look-ahead token iLookAhead.
**
** If the look-ahead token is YYNOCODE, then check to see if the action is
** independent of the look-ahead.  If it is, return the action, otherwise
** return YY_NO_ACTION.
*/
static int yy_find_reduce_action(
  yyParser *pParser,        /* The parser */
  int iLookAhead            /* The look-ahead token */
){
  int i;
  int stateno = pParser->yystack[pParser->yyidx].stateno;
 
  i = yy_reduce_ofst[stateno];
  if( i==YY_REDUCE_USE_DFLT ){
    return yy_default[stateno];
  }
  if( iLookAhead==YYNOCODE ){
    return YY_NO_ACTION;
  }
  i += iLookAhead;
  if( i<0 || i>=YY_SZ_ACTTAB || yy_lookahead[i]!=iLookAhead ){
    return yy_default[stateno];
  }else{
    return yy_action[i];
  }
}

/*
** Perform a shift action.
*/
static void yy_shift(
  yyParser *yypParser,          /* The parser to be shifted */
  int yyNewState,               /* The new state to shift in */
  int yyMajor,                  /* The major token to shift in */
  YYMINORTYPE *yypMinor         /* Pointer ot the minor token to shift in */
){
  yyStackEntry *yytos;
  yypParser->yyidx++;
  if( yypParser->yyidx>=YYSTACKDEPTH ){
     BbParseARG_FETCH;
     yypParser->yyidx--;
#ifndef NDEBUG
     if( yyTraceFILE ){
       fprintf(yyTraceFILE,"%sStack Overflow!\n",yyTracePrompt);
     }
#endif
     while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
     /* Here code is inserted which will execute if the parser
     ** stack every overflows */
     BbParseARG_STORE; /* Suppress warning about unused %extra_argument var */
     return;
  }
  yytos = &yypParser->yystack[yypParser->yyidx];
  yytos->stateno = yyNewState;
  yytos->major = yyMajor;
  yytos->minor = *yypMinor;
#ifndef NDEBUG
  if( yyTraceFILE && yypParser->yyidx>0 ){
    int i;
    fprintf(yyTraceFILE,"%sShift %d\n",yyTracePrompt,yyNewState);
    fprintf(yyTraceFILE,"%sStack:",yyTracePrompt);
    for(i=1; i<=yypParser->yyidx; i++)
      fprintf(yyTraceFILE," %s",yyTokenName[yypParser->yystack[i].major]);
    fprintf(yyTraceFILE,"\n");
  }
#endif
}

/* The following table contains information about every rule that
** is used during the reduce.
*/
static const struct {
  YYCODETYPE lhs;         /* Symbol on the left-hand side of the rule */
  unsigned char nrhs;     /* Number of right-hand side symbols in the rule */
} yyRuleInfo[] = {
  { 16, 3 },
  { 16, 3 },
  { 16, 3 },
  { 16, 2 },
  { 16, 3 },
  { 16, 3 },
  { 16, 3 },
  { 16, 1 },
  { 16, 1 },
  { 16, 4 },
  { 16, 3 },
  { 16, 3 },
  { 16, 3 },
  { 16, 1 },
  { 15, 1 },
};

static void yy_accept(yyParser*);  /* Forward Declaration */

/*
** Perform a reduce action and the shift that must immediately
** follow the reduce.
*/
static void yy_reduce(
  yyParser *yypParser,         /* The parser */
  int yyruleno                 /* Number of the rule by which to reduce */
){
  int yygoto;                     /* The next state */
  int yyact;                      /* The next action */
  YYMINORTYPE yygotominor;        /* The LHS of the rule reduced */
  yyStackEntry *yymsp;            /* The top of the parser's stack */
  int yysize;                     /* Amount to pop the stack */
  BbParseARG_FETCH;
  yymsp = &yypParser->yystack[yypParser->yyidx];
#ifndef NDEBUG
  if( yyTraceFILE && yyruleno>=0 
        && yyruleno<sizeof(yyRuleName)/sizeof(yyRuleName[0]) ){
    fprintf(yyTraceFILE, "%sReduce [%s].\n", yyTracePrompt,
      yyRuleName[yyruleno]);
  }
#endif /* NDEBUG */

  switch( yyruleno ){
  /* Beginning here are the reduction cases.  A typical example
  ** follows:
  **   case 0:
  **  #line <lineno> <grammarfile>
  **     { ... }           // User supplied code
  **  #line <lineno> <thisfile>
  **     break;
  */
      case 0:
#line 28 "core/bb-p.lemon"
{ yygotominor.yy5 = bb_program_new ();
	  bb_program_add_function_begin (yygotominor.yy5);
	  bb_program_append_program (yygotominor.yy5, yymsp[-2].minor.yy5);
	  bb_program_append_program (yygotominor.yy5, yymsp[0].minor.yy5);
	  bb_program_add_lazy_function (yygotominor.yy5, "add");   yy_destructor(2,&yymsp[-1].minor);
}
#line 592 "core/bb-p.c"
        break;
      case 1:
#line 34 "core/bb-p.lemon"
{ yygotominor.yy5 = bb_program_new ();
	  bb_program_add_function_begin (yygotominor.yy5);
	  bb_program_append_program (yygotominor.yy5, yymsp[-2].minor.yy5);
	  bb_program_append_program (yygotominor.yy5, yymsp[0].minor.yy5);
	  bb_program_add_lazy_function (yygotominor.yy5, "mul");   yy_destructor(4,&yymsp[-1].minor);
}
#line 602 "core/bb-p.c"
        break;
      case 2:
#line 40 "core/bb-p.lemon"
{ yygotominor.yy5 = bb_program_new ();
	  bb_program_add_function_begin (yygotominor.yy5);
	  bb_program_append_program (yygotominor.yy5, yymsp[-2].minor.yy5);
	  bb_program_append_program (yygotominor.yy5, yymsp[0].minor.yy5);
	  bb_program_add_lazy_function (yygotominor.yy5, "sub");   yy_destructor(3,&yymsp[-1].minor);
}
#line 612 "core/bb-p.c"
        break;
      case 3:
#line 46 "core/bb-p.lemon"
{ yygotominor.yy5 = bb_program_new ();
	  bb_program_add_function_begin (yygotominor.yy5);
	  bb_program_append_program (yygotominor.yy5, yymsp[0].minor.yy5);
	  bb_program_add_lazy_function (yygotominor.yy5, "neg");   yy_destructor(3,&yymsp[-1].minor);
}
#line 621 "core/bb-p.c"
        break;
      case 4:
#line 51 "core/bb-p.lemon"
{ yygotominor.yy5 = bb_program_new ();
	  bb_program_add_function_begin (yygotominor.yy5);
	  bb_program_append_program (yygotominor.yy5, yymsp[-2].minor.yy5);
	  bb_program_append_program (yygotominor.yy5, yymsp[0].minor.yy5);
	  bb_program_add_lazy_function (yygotominor.yy5, "div");   yy_destructor(5,&yymsp[-1].minor);
}
#line 631 "core/bb-p.c"
        break;
      case 5:
#line 58 "core/bb-p.lemon"
{ yygotominor.yy5 = bb_program_new ();
	  bb_program_append_program (yygotominor.yy5, yymsp[-1].minor.yy5);   yy_destructor(7,&yymsp[-2].minor);
  yy_destructor(8,&yymsp[0].minor);
}
#line 639 "core/bb-p.c"
        break;
      case 6:
#line 61 "core/bb-p.lemon"
{ yygotominor.yy5 = bb_program_new ();
	  bb_program_append_program (yygotominor.yy5, yymsp[-2].minor.yy5);
	  bb_program_append_program (yygotominor.yy5, yymsp[0].minor.yy5);   yy_destructor(1,&yymsp[-1].minor);
}
#line 647 "core/bb-p.c"
        break;
      case 7:
#line 65 "core/bb-p.lemon"
{ GValue value;
          char *end;
          gdouble nvalue = strtod (yymsp[0].minor.yy0, &end);
          if (yymsp[0].minor.yy0 == end)
            g_error ("error parsing number from %s", yymsp[0].minor.yy0);
	  memset (&value, 0, sizeof (value));
          if (strcmp (end, "sec") == 0 || strcmp (end, "s") == 0)
            {
              g_value_init (&value, BB_TYPE_DURATION);
              bb_value_set_duration (&value, BB_DURATION_UNITS_SECONDS, nvalue);
            }
          else if (strcmp (end, "samp") == 0)
            {
              g_value_init (&value, BB_TYPE_DURATION);
              bb_value_set_duration (&value, BB_DURATION_UNITS_SAMPLES, nvalue);
            }
          else if (strcmp (end, "beat") == 0 || strcmp (end, "b") == 0)
            {
              g_value_init (&value, BB_TYPE_DURATION);
              bb_value_set_duration (&value, BB_DURATION_UNITS_BEATS, nvalue);
            }
          else if (strcmp (end, "%") == 0)
            {
              g_value_init (&value, BB_TYPE_DURATION);
              bb_value_set_duration (&value, BB_DURATION_UNITS_NOTE_LENGTHS, nvalue * 0.01);
            }
          else if (*end != 0)
            {
              g_error ("garbage after number (garbage is '%s'", end);
            }
          else
            {
              g_value_init (&value, G_TYPE_DOUBLE);
              g_value_set_double (&value, nvalue);
            }
	  yygotominor.yy5 = bb_program_new ();
	  bb_program_add_push (yygotominor.yy5, &value); }
#line 688 "core/bb-p.c"
        break;
      case 8:
#line 104 "core/bb-p.lemon"
{ /* find or allocate parameter */
	  guint i;
	  for (i = 0; i < context->pspec_array->len; i++)
	    {
	      GParamSpec *p = g_ptr_array_index (context->pspec_array, i);
	      if (bb_param_spec_names_equal (p->name, yymsp[0].minor.yy0))
	        break;
            }
	  if (i == context->pspec_array->len)
	    {
              for (i = 0; i < context->n_source_pspecs; i++)
                if (bb_param_spec_names_equal (yymsp[0].minor.yy0, context->source_pspecs[i]->name))
                  break;
              if (i == context->n_source_pspecs)
		{
		  g_message ("warning: allocating double parameter named %s", yymsp[0].minor.yy0);
		  g_ptr_array_add (context->pspec_array,
				   g_param_spec_double (yymsp[0].minor.yy0,yymsp[0].minor.yy0,NULL,
				      -1e9,1e9,0,G_PARAM_READWRITE));
		  }
              else
                g_ptr_array_add (context->pspec_array, context->source_pspecs[i]);
              i = context->pspec_array->len - 1;
	    }
	  yygotominor.yy5 = bb_program_new ();
	  bb_program_add_push_param (yygotominor.yy5, i); }
#line 718 "core/bb-p.c"
        break;
      case 9:
#line 132 "core/bb-p.lemon"
{ BbBuiltinFunc f;
          yygotominor.yy5 = bb_program_new ();
	  f = bb_builtin_lookup (yymsp[-3].minor.yy0);
	  if (f != NULL)
	    {
	      bb_program_append_program (yygotominor.yy5, yymsp[-1].minor.yy5);
	      bb_program_add_builtin (yygotominor.yy5, yymsp[-3].minor.yy0, f);
	    }
	  else
	    {
	      /* TODO: try resolving the function now */
	      bb_program_add_function_begin (yygotominor.yy5);
	      bb_program_append_program (yygotominor.yy5, yymsp[-1].minor.yy5);
	      bb_program_add_lazy_function (yygotominor.yy5, yymsp[-3].minor.yy0);
	    }
	  yy_destructor(7,&yymsp[-2].minor);
  yy_destructor(8,&yymsp[0].minor);
}
#line 740 "core/bb-p.c"
        break;
      case 10:
#line 149 "core/bb-p.lemon"
{ BbBuiltinFunc f;
          yygotominor.yy5 = bb_program_new ();
	  f = bb_builtin_lookup (yymsp[-2].minor.yy0);
	  if (f != NULL)
	    bb_program_add_builtin (yygotominor.yy5, yymsp[-2].minor.yy0, f);
	  else
	    {
	      bb_program_add_function_begin (yygotominor.yy5);
	      bb_program_add_lazy_function (yygotominor.yy5, yymsp[-2].minor.yy0);
	    }
	  yy_destructor(7,&yymsp[-1].minor);
  yy_destructor(8,&yymsp[0].minor);
}
#line 757 "core/bb-p.c"
        break;
      case 11:
#line 163 "core/bb-p.lemon"
{ GType type = g_type_from_name (yymsp[-2].minor.yy0);
          guint v;
          GValue ev;
          if (type == 0 || !g_type_is_a (type, G_TYPE_ENUM))
            g_error ("no enum named '%s'", yymsp[-2].minor.yy0);
          if (!bb_enum_value_lookup (type, yymsp[0].minor.yy0, &v))
            g_error ("no value of %s named %s", yymsp[-2].minor.yy0, yymsp[0].minor.yy0);

          memset (&ev, 0, sizeof (ev));
          g_value_init (&ev, type);
          g_value_set_enum (&ev, v);
          yygotominor.yy5 = bb_program_new ();
          bb_program_add_push (yygotominor.yy5, &ev);   yy_destructor(6,&yymsp[-1].minor);
}
#line 775 "core/bb-p.c"
        break;
      case 12:
#line 178 "core/bb-p.lemon"
{ yygotominor.yy5 = bb_program_new ();
	  bb_program_add_push_special (yygotominor.yy5, BB_VM_SPECIAL_ARRAY_BEGIN);
	  bb_program_append_program (yygotominor.yy5, yymsp[-1].minor.yy5);
	  bb_program_add_builtin (yygotominor.yy5, "create_array", bb_builtin_create_array);   yy_destructor(11,&yymsp[-2].minor);
  yy_destructor(12,&yymsp[0].minor);
}
#line 785 "core/bb-p.c"
        break;
      case 13:
#line 184 "core/bb-p.lemon"
{ GValue v = BB_VALUE_INIT;
          yygotominor.yy5 = bb_program_new ();
	  g_value_init (&v, G_TYPE_STRING);
          g_value_set_string (&v, yymsp[0].minor.yy0);
	  bb_program_add_push (yygotominor.yy5, &v); }
#line 794 "core/bb-p.c"
        break;
      case 14:
#line 191 "core/bb-p.lemon"
{ if (context->program)
	    { g_warning ("got two programs"); }
	  else
	    { context->program = bb_program_ref (yymsp[0].minor.yy5); }
	}
#line 803 "core/bb-p.c"
        break;
  };
  yygoto = yyRuleInfo[yyruleno].lhs;
  yysize = yyRuleInfo[yyruleno].nrhs;
  yypParser->yyidx -= yysize;
  yyact = yy_find_reduce_action(yypParser,yygoto);
  if( yyact < YYNSTATE ){
    yy_shift(yypParser,yyact,yygoto,&yygotominor);
  }else if( yyact == YYNSTATE + YYNRULE + 1 ){
    yy_accept(yypParser);
  }
}

/*
** The following code executes when the parse fails
*/
static void yy_parse_failed(
  yyParser *yypParser           /* The parser */
){
  BbParseARG_FETCH;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sFail!\n",yyTracePrompt);
  }
#endif
  while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
  /* Here code is inserted which will be executed whenever the
  ** parser fails */
  BbParseARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/*
** The following code executes when a syntax error first occurs.
*/
static void yy_syntax_error(
  yyParser *yypParser,           /* The parser */
  int yymajor,                   /* The major type of the error token */
  YYMINORTYPE yyminor            /* The minor type of the error token */
){
  BbParseARG_FETCH;
#define TOKEN (yyminor.yy0)
  BbParseARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/*
** The following is executed when the parser accepts
*/
static void yy_accept(
  yyParser *yypParser           /* The parser */
){
  BbParseARG_FETCH;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sAccept!\n",yyTracePrompt);
  }
#endif
  while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
  /* Here code is inserted which will be executed whenever the
  ** parser accepts */
  BbParseARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/* The main parser program.
** The first argument is a pointer to a structure obtained from
** "BbParseAlloc" which describes the current state of the parser.
** The second argument is the major token number.  The third is
** the minor token.  The fourth optional argument is whatever the
** user wants (and specified in the grammar) and is available for
** use by the action routines.
**
** Inputs:
** <ul>
** <li> A pointer to the parser (an opaque structure.)
** <li> The major token number.
** <li> The minor token number.
** <li> An option argument of a grammar-specified type.
** </ul>
**
** Outputs:
** None.
*/
void BbParse(
  void *yyp,                   /* The parser */
  int yymajor,                 /* The major token code number */
  BbParseTOKENTYPE yyminor       /* The value for the token */
  BbParseARG_PDECL               /* Optional %extra_argument parameter */
){
  YYMINORTYPE yyminorunion;
  int yyact;            /* The parser action. */
  int yyendofinput;     /* True if we are at the end of input */
  int yyerrorhit = 0;   /* True if yymajor has invoked an error */
  yyParser *yypParser;  /* The parser */

  /* (re)initialize the parser, if necessary */
  yypParser = (yyParser*)yyp;
  if( yypParser->yyidx<0 ){
    if( yymajor==0 ) return;
    yypParser->yyidx = 0;
    yypParser->yyerrcnt = -1;
    yypParser->yystack[0].stateno = 0;
    yypParser->yystack[0].major = 0;
  }
  yyminorunion.yy0 = yyminor;
  yyendofinput = (yymajor==0);
  BbParseARG_STORE;

#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sInput %s\n",yyTracePrompt,yyTokenName[yymajor]);
  }
#endif

  do{
    yyact = yy_find_shift_action(yypParser,yymajor);
    if( yyact<YYNSTATE ){
      yy_shift(yypParser,yyact,yymajor,&yyminorunion);
      yypParser->yyerrcnt--;
      if( yyendofinput && yypParser->yyidx>=0 ){
        yymajor = 0;
      }else{
        yymajor = YYNOCODE;
      }
    }else if( yyact < YYNSTATE + YYNRULE ){
      yy_reduce(yypParser,yyact-YYNSTATE);
    }else if( yyact == YY_ERROR_ACTION ){
      int yymx;
#ifndef NDEBUG
      if( yyTraceFILE ){
        fprintf(yyTraceFILE,"%sSyntax Error!\n",yyTracePrompt);
      }
#endif
#ifdef YYERRORSYMBOL
      /* A syntax error has occurred.
      ** The response to an error depends upon whether or not the
      ** grammar defines an error token "ERROR".  
      **
      ** This is what we do if the grammar does define ERROR:
      **
      **  * Call the %syntax_error function.
      **
      **  * Begin popping the stack until we enter a state where
      **    it is legal to shift the error symbol, then shift
      **    the error symbol.
      **
      **  * Set the error count to three.
      **
      **  * Begin accepting and shifting new tokens.  No new error
      **    processing will occur until three tokens have been
      **    shifted successfully.
      **
      */
      if( yypParser->yyerrcnt<0 ){
        yy_syntax_error(yypParser,yymajor,yyminorunion);
      }
      yymx = yypParser->yystack[yypParser->yyidx].major;
      if( yymx==YYERRORSYMBOL || yyerrorhit ){
#ifndef NDEBUG
        if( yyTraceFILE ){
          fprintf(yyTraceFILE,"%sDiscard input token %s\n",
             yyTracePrompt,yyTokenName[yymajor]);
        }
#endif
        yy_destructor(yymajor,&yyminorunion);
        yymajor = YYNOCODE;
      }else{
         while(
          yypParser->yyidx >= 0 &&
          yymx != YYERRORSYMBOL &&
          (yyact = yy_find_shift_action(yypParser,YYERRORSYMBOL)) >= YYNSTATE
        ){
          yy_pop_parser_stack(yypParser);
        }
        if( yypParser->yyidx < 0 || yymajor==0 ){
          yy_destructor(yymajor,&yyminorunion);
          yy_parse_failed(yypParser);
          yymajor = YYNOCODE;
        }else if( yymx!=YYERRORSYMBOL ){
          YYMINORTYPE u2;
          u2.YYERRSYMDT = 0;
          yy_shift(yypParser,yyact,YYERRORSYMBOL,&u2);
        }
      }
      yypParser->yyerrcnt = 3;
      yyerrorhit = 1;
#else  /* YYERRORSYMBOL is not defined */
      /* This is what we do if the grammar does not define ERROR:
      **
      **  * Report an error message, and throw away the input token.
      **
      **  * If the input token is $, then fail the parse.
      **
      ** As before, subsequent error messages are suppressed until
      ** three input tokens have been successfully shifted.
      */
      if( yypParser->yyerrcnt<=0 ){
        yy_syntax_error(yypParser,yymajor,yyminorunion);
      }
      yypParser->yyerrcnt = 3;
      yy_destructor(yymajor,&yyminorunion);
      if( yyendofinput ){
        yy_parse_failed(yypParser);
      }
      yymajor = YYNOCODE;
#endif
    }else{
      yy_accept(yypParser);
      yymajor = YYNOCODE;
    }
  }while( yymajor!=YYNOCODE && yypParser->yyidx>=0 );
  return;
}
