#ifndef __BB_INPUT_FILE_H_
#define __BB_INPUT_FILE_H_

typedef struct _BbInputFile BbInputFile;

#include <stdio.h>
#include <glib.h>

struct _BbInputFile
{
  FILE *fp;
  char *line;
  gboolean eof;
  guint line_no;
};

BbInputFile * bb_input_file_open        (const char  *filename,
                                         GError     **error);
BbInputFile * bb_input_file_new_from_fp (FILE        *fp);
gboolean      bb_input_file_advance     (BbInputFile *file);
void          bb_input_file_close       (BbInputFile *file);

#endif
