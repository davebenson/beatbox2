#include <errno.h>
#include "bb-input-file.h"
#include "bb-error.h"

BbInputFile *
bb_input_file_open        (const char  *filename,
                           GError     **error)
{
  FILE *fp = fopen (filename, "r");
  if (fp == NULL)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_FILE_READ,
                   "error opening file %s for reading: %s", filename, g_strerror (errno));
      return FALSE;
    }
  return bb_input_file_new_from_fp (fp);
}

BbInputFile *
bb_input_file_new_from_fp (FILE *fp)
{
  BbInputFile *file = g_new (BbInputFile, 1);
  file->line_no = 0;
  file->fp = fp;
  file->eof = 0;
  file->line = NULL;
  bb_input_file_advance (file);
  return file;
}

gboolean
bb_input_file_advance (BbInputFile *file)
{
  char buf[8192];
  g_free (file->line);
  file->line = NULL;
  if (!fgets (buf, sizeof (buf), file->fp))
    {
      file->eof = TRUE;
      return FALSE;
    }
  else
    {
      GString *str;
      g_strstrip (buf);
      str = g_string_new (buf);
      file->line_no += 1;
      if (str->len > 0 && str->str[str->len-1] == '\\')
        {
          for (;;)
            {
              str->str[str->len - 1] = ' ';
              if (!fgets (buf, sizeof (buf), file->fp))
                g_error ("eof after \\");
              file->line_no += 1;
              g_strstrip (buf);
              g_string_append (str, buf);
              if (!g_str_has_suffix (buf, "\\"))
                break;
            }
        }
      file->line = g_string_free (str, FALSE);
      return TRUE;
    }
}

void
bb_input_file_close (BbInputFile *file)
{
  fclose (file->fp);
  g_free (file);
}
