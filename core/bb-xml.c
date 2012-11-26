#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "bb-xml.h"
#include "bb-error.h"
#include <gsk/gsklistmacros.h>

typedef struct _ParseState ParseState;

struct _ParseState
{
  BbXml *stack;
  GSList *results;
};
#define PARSE_STATE_INIT {NULL, NULL}

#define CHILD_LIST(node)                         \
        BbXml *,                                 \
        (node)->first_child, (node)->last_child, \
        prev_sibling, next_sibling

static void
parse_state_handle_start_element  (GMarkupParseContext *context,
                                   const gchar         *element_name,
                                   const gchar        **attribute_names,
                                   const gchar        **attribute_values,
                                   gpointer             user_data,
                                   GError             **error)
{
  ParseState *ps = user_data;
  BbXml *new = bb_xml_new_node2 (element_name, attribute_names, attribute_values);
  if (ps->stack)
    bb_xml_add_child (ps->stack, new);
  ps->stack = new;
}

static void
parse_state_handle_end_element  (GMarkupParseContext *context,
                                 const gchar         *element_name,
                                 gpointer             user_data,
                                 GError             **error)
{
  ParseState *ps = user_data;
  g_assert (ps->stack != NULL);
  if (ps->stack->parent == NULL)
    ps->results = g_slist_prepend (ps->results, ps->stack);
  ps->stack = ps->stack->parent;
}

static void
parse_state_handle_text(GMarkupParseContext *context,
                        const gchar         *text,
                        gsize                text_len,  
                        gpointer             user_data,
                        GError             **error)
{
  ParseState *ps = user_data;
  g_assert (ps->stack != NULL);
  bb_xml_add_child (ps->stack, bb_xml_new_text_len (text, text_len));
}
static void
parse_state_handle_passthrough  (GMarkupParseContext *context,
                                 const gchar         *passthrough_text,
                                 gsize                text_len,  
                                 gpointer             user_data,
                                 GError             **error)
{
  ParseState *ps = user_data;
  if (text_len >= 12 && g_str_has_prefix (passthrough_text, "<![CDATA["))
    bb_xml_add_child (ps->stack, bb_xml_new_text_len (passthrough_text + 9, text_len - 12));
}

static GMarkupParser parse_funcs =
{
  .start_element = parse_state_handle_start_element,
  .end_element = parse_state_handle_end_element,
  .text = parse_state_handle_text,
  .passthrough = parse_state_handle_passthrough,
};

BbXml   *bb_xml_parse_string (const char *str,
                              GError    **error)
{
  ParseState parse_state = PARSE_STATE_INIT;
  GMarkupParseContext *context = g_markup_parse_context_new (&parse_funcs, 0, &parse_state, NULL);
  BbXml *rv;
  if (!g_markup_parse_context_parse (context, str, -1, error)
   || !g_markup_parse_context_end_parse (context, error))
    {
      g_markup_parse_context_free (context);

      /* TODO CLEANUP */
      return NULL;
    }
  g_assert (g_slist_length (parse_state.results) == 1);
  g_assert (parse_state.stack == NULL);
  rv = parse_state.results->data;
  g_slist_free (parse_state.results);
  return rv;
}

BbXml   *bb_xml_parse_file   (const char *str,
                              GError    **error)
{
  ParseState parse_state = PARSE_STATE_INIT;
  GMarkupParseContext *context;
  BbXml *rv;
  FILE *fp = fopen (str, "rb");
  guint nread;
  char buf[8192];
  if (fp == NULL)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_FILE_READ,
                   "error opening %s: %s",
                   str, g_strerror (errno));
      return NULL;
    }
  context = g_markup_parse_context_new (&parse_funcs, 0, &parse_state, NULL);
  while ((nread=fread (buf, 1, sizeof (buf), fp)) != 0)
    {
      if (!g_markup_parse_context_parse (context, buf, nread, error))
        {
          fclose (fp);
          /* TODO CLEANUP */
          return NULL;
        }
    }
  fclose (fp);
  if (!g_markup_parse_context_end_parse (context, error))
    {
      /* TODO CLEANUP */
      return NULL;
    }

  g_assert (g_slist_length (parse_state.results) == 1);
  g_assert (parse_state.stack == NULL);
  rv = parse_state.results->data;
  g_slist_free (parse_state.results);
  return rv;
}

static void
bb_xml_iterate_list (BbXml *xml,
                     const char *path,
                     BbXmlFunc   func,
                     gpointer    data)
{
  const char *slash = strchr (path, '/');
  guint len;
  BbXml *at;
  if (slash == NULL)
    len = strlen (path);
  else
    len = slash - path;
  for (at = xml; at; at = at->next_sibling)
    if (at->type == BB_XML_NODE
     && memcmp (at->str, path, len) == 0
     && at->str[len] == '\0')
      {
        if (slash)
          bb_xml_iterate_list (at->first_child, slash + 1, func, data);
        else
          func (at, data);
      }
}


void     bb_xml_iterate      (BbXml      *xml,
                              const char *path,
                              BbXmlFunc   func,
          		      gpointer    data)
{
  g_assert (xml->next_sibling == NULL);
  bb_xml_iterate_list (xml, path, func, data);
}

BbXml *
bb_xml_find_child (BbXml *xml,
                   const char *name)
{
  BbXml *at;
  for (at = xml->first_child; at; at = at->next_sibling)
    if (at->type == BB_XML_NODE && strcmp (at->str, name) == 0)
      return at;
  return NULL;
}
const char *
bb_xml_find_attr (BbXml      *xml,
                  const char *name)
{
  guint i;
  g_return_val_if_fail (xml->type == BB_XML_NODE, NULL);
  if (xml->attrs == NULL)
    return NULL;
  for (i = 0; xml->attrs[2*i]; i++)
    if (strcmp (xml->attrs[2*i], name) == 0)
      return xml->attrs[2*i+1];
  return NULL;
}

void     bb_xml_free         (BbXml      *xml)
{
  BbXml *at, *next;
  for (at = xml->first_child; at; at = next)
    {
      next = at->next_sibling;
      bb_xml_free (at);
    }
  g_free (xml->str);
  if (xml->attrs)
    g_strfreev (xml->attrs);
  g_free (xml);
}


BbXml   *bb_xml_new_text     (const char *text)
{
  BbXml *xml = g_new0 (BbXml, 1);
  xml->type = BB_XML_TEXT;
  xml->str = g_strdup (text);
  return xml;
}

BbXml   *bb_xml_new_text_len (const char *text,
                              guint       len)
{
  BbXml *xml = g_new0 (BbXml, 1);
  xml->type = BB_XML_TEXT;
  xml->str = g_strndup (text, len);
  return xml;
}

BbXml   *bb_xml_new_node     (const char *name,
                              char      **key_value_pairs)
{
  BbXml *xml = g_new0 (BbXml, 1);
  xml->type = BB_XML_NODE;
  xml->str = g_strdup (name);
  xml->attrs = g_strdupv (key_value_pairs);
  return xml;
}

BbXml   *bb_xml_new_node2    (const char *name,
                              const char**attr_keys,
                              const char**attr_values)
{
  BbXml *xml = g_new0 (BbXml, 1);
  guint n_attrs;
  xml->type = BB_XML_NODE;
  xml->str = g_strdup (name);
  if (attr_keys == NULL)
    {
      xml->attrs = NULL;
    }
  else
    {
      guint i;
      for (n_attrs = 0; attr_keys[n_attrs]; n_attrs++)
        ;
      xml->attrs = g_new (char *, n_attrs * 2 + 1);
      for (i = 0; i < n_attrs; i++)
        {
          xml->attrs[2*i+0] = g_strdup (attr_keys[i]);
          xml->attrs[2*i+1] = g_strdup (attr_values[i]);
        }
      xml->attrs[n_attrs * 2] = NULL;
    }
  return xml;
}

void     bb_xml_add_child    (BbXml      *parent,
                              BbXml      *child)
{
  g_assert (child->parent == NULL);
  GSK_LIST_APPEND (CHILD_LIST (parent), child);
  child->parent = parent;
}

void     bb_xml_add_text_child(BbXml      *parent,
                               const char *node_name,
                               const char *contents)
{
  BbXml *child = bb_xml_new_node (node_name, NULL);
  bb_xml_add_child (child, bb_xml_new_text (contents));
  bb_xml_add_child (parent, child);
}

static void
append_escaped (GString *str, const char *text)
{
  char *tmp = g_markup_escape_text (text, -1);
  g_string_append (str, tmp);
  g_free (tmp);
}

static void
append_to_gstring (GString *str, BbXml *xml)
{
  switch (xml->type)
    {
    case BB_XML_TEXT:
      append_escaped (str, xml->str);
      break;

    case BB_XML_NODE:
      g_string_append_c (str, '<');
      append_escaped (str, xml->str);
      if (xml->attrs)
        {
          char **at;
          for (at = xml->attrs; *at; at += 2)
            {
              g_string_append_c (str, ' ');
              append_escaped (str, at[0]);
              g_string_append_c (str, '=');
              g_string_append_c (str, '"');
              append_escaped (str, at[1]);
              g_string_append_c (str, '"');
            }
        }
      g_string_append_c (str, '>');
      {
        BbXml *at;
        for (at = xml->first_child; at; at = at->next_sibling)
          append_to_gstring (str, at);
      }
      g_string_append (str, "</");
      append_escaped (str, xml->str);
      g_string_append_c (str, '>');
      break;

    default:
      g_assert_not_reached ();
    }
}

char    *bb_xml_to_string    (BbXml      *xml)
{
  GString *str = g_string_new ("");
  append_to_gstring (str, xml);
  return g_string_free (str, FALSE);
}

static void
get_all_text__append_to_gstring (GString *str, BbXml *xml)
{
  switch (xml->type)
    {
    case BB_XML_TEXT:
      g_string_append (str, xml->str);
      break;

    case BB_XML_NODE:
      {
        BbXml *at;
        for (at = xml->first_child; at; at = at->next_sibling)
          get_all_text__append_to_gstring (str, at);
      }
      break;

    default:
      g_assert_not_reached ();
    }
}

char    *bb_xml_get_all_text    (BbXml      *xml)
{
  GString *str = g_string_new ("");
  get_all_text__append_to_gstring (str, xml);
  return g_string_free (str, FALSE);
}

gboolean bb_xml_save         (BbXml      *xml,
                              const char *filename,
                              GError    **error)
{
  GString *str = g_string_new ("");
  FILE *fp;
  append_to_gstring (str, xml);
  g_string_append_c (str, '\n');
  fp = fopen (filename, "wb");
  if (fp == NULL)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_FILE_WRITE,
                   "error creating %s: %s", filename, g_strerror (errno));
      return FALSE;
    }
  if (fwrite (str->str, 1, str->len, fp) != str->len)
    {
      g_set_error (error, BB_ERROR_DOMAIN_QUARK, BB_ERROR_FILE_WRITE,
                   "error writing to file %s", filename);
      fclose (fp);
      g_string_free (str, TRUE);
      return FALSE;
    }
  g_string_free (str, TRUE);
  fclose (fp);
  return TRUE;
}
